/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.c
 * Description        : USB CDC ACM 设备层 —— 描述符、事件/端点回调、初始化、
 *                      UART1->USB 上传。与平台无关（端口由 usb_dc_low_level_init.c
 *                      与 CMake 的 CHERRYUSB_USB_PORT 决定）。
 ********************************************************************************/
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "usb_cdc.h"
#include "uart1_bridge.h"

#define CDC_IN_EP  0x83
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x81

/* 沿用原工程 VID/PID (WCH 0x1A86 / 0xFE0C) */
#define USBD_VID       0x1A86
#define USBD_PID       0xFE0C
#define USBD_MAX_POWER 100

/* 配置描述符：与 WCH 官方 L103 例程 usb_desc.c 完全一致（无 IAD，
 * 中断 EP=0x81，数据 IN=0x83）。带 IAD 的 CDC_ACM_DESCRIPTOR_INIT 布局
 * 在 L103 + 本机 usbser 下触发 Code 10（CM_PROB_FAILED_START）。 */
#define USB_CONFIG_SIZE 67

/* ------------------------------------------------------------------------- */
/* 描述符                                                                     */
/* ------------------------------------------------------------------------- */
static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x02, 0x00, 0x00, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t config_descriptor[] = {
    /* 配置描述符 */
    0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    /* 接口 0 (CDC) */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    /* 功能描述符 */
    0x05, 0x24, 0x00, 0x10, 0x01,
    0x05, 0x24, 0x01, 0x00, 0x01,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01,
    /* 中断上传端点 */
    0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01,
    /* 接口 1 (数据) */
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    /* 端点描述符 */
    0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
    0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00,
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "WCH",                        /* Manufacturer */
    "CH32L103 SimulateCDC",       /* Product */
    "L103-20260811",              /* Serial Number（唯一，避免与旧设备实例冲突） */
};

/* 最小 BOS 描述符：避免 Windows 周期性请求 0x0F 时报错 */
static const uint8_t bos_descriptor[] = {
    0x05, /* bLength */
    USB_DESCRIPTOR_TYPE_BINARY_OBJECT_STORE, /* bDescriptorType = 0x0F */
    0x05, 0x00, /* wTotalLength = 5 */
    0x00, /* bNumDeviceCaps = 0 */
};

static const struct usb_bos_descriptor cdc_bos = {
    .string = bos_descriptor,
    .string_len = sizeof(bos_descriptor),
};

static const uint8_t* device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t* config_descriptor_callback(uint8_t speed)
{
    return config_descriptor;
}

static const char* string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index >= (sizeof(string_descriptors) / sizeof(char*)))
    {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor cdc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
    .bos_descriptor = &cdc_bos,
};

/* ------------------------------------------------------------------------- */
/* 数据缓冲 / 状态                                                            */
/* ------------------------------------------------------------------------- */
__attribute__((aligned(4))) static uint8_t cdc_out_buf[64]; /* EP2 OUT 接收缓冲 */
static volatile bool cdc_in_busy; /* EP3 IN 传输中 */

/* ------------------------------------------------------------------------- */
/* 事件 / 端点回调                                                            */
/* ------------------------------------------------------------------------- */
static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event)
    {
    case USBD_EVENT_RESET:
    case USBD_EVENT_CONNECTED:
    case USBD_EVENT_DISCONNECTED:
    case USBD_EVENT_RESUME:
    case USBD_EVENT_SUSPEND:
    case USBD_EVENT_SET_REMOTE_WAKEUP:
    case USBD_EVENT_CLR_REMOTE_WAKEUP:
        break;
    case USBD_EVENT_CONFIGURED:
        cdc_in_busy = false;
        /* 使能 EP2 OUT 接收 */
        usbd_ep_start_read(busid, CDC_OUT_EP, cdc_out_buf, 64);
        break;

    default:
        break;
    }
}

/* PC -> USB：数据进 UART1 发送 FIFO（由桥接层统一发往串口） */
static void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    for (uint32_t i = 0; i < nbytes; i++)
    {
        bridge_tx_push(cdc_out_buf[i]); /* 满则丢弃 */
    }
    usbd_ep_start_read(busid, CDC_OUT_EP, cdc_out_buf, 64);
}

/* USB -> PC：上传完成 */
static void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes)
    {
        /* 整包数据需要补 0 长度包，表示传输结束 */
        usbd_ep_start_write(busid, CDC_IN_EP, NULL, 0);
    }
    else
    {
        cdc_in_busy = false;
    }
}

static struct usbd_endpoint cdc_out_ep = {
    .ep_addr = CDC_OUT_EP,
    .ep_cb = usbd_cdc_acm_bulk_out
};

static struct usbd_endpoint cdc_in_ep = {
    .ep_addr = CDC_IN_EP,
    .ep_cb = usbd_cdc_acm_bulk_in
};

/* ------------------------------------------------------------------------- */
/* 对外接口                                                                   */
/* ------------------------------------------------------------------------- */
static struct usbd_interface intf0;
static struct usbd_interface intf1;

void cdc_acm_init(uint8_t busid)
{
    usbd_desc_register(busid, &cdc_descriptor);
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf0));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf1));
    usbd_add_endpoint(busid, &cdc_out_ep);
    usbd_add_endpoint(busid, &cdc_in_ep);
#ifdef CONFIG_CH32_USBFS
    usbd_initialize(busid, 0x50000000, usbd_event_handler); /* USBFS 基址 */
#else
    usbd_initialize(busid, 0x40005C00, usbd_event_handler); /* USBD (F103 兼容) 基址 */
#endif
}

bool cdc_uart_data_upload(const uint8_t* buf, uint32_t len)
{
    if (cdc_in_busy)
    {
        return false;
    }
    cdc_in_busy = true;
    usbd_ep_start_write(0, CDC_IN_EP, buf, len);
    return true;
}

bool cdc_tx_busy(void)
{
    return cdc_in_busy;
}
