/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_msc.c
 * Description        : CherryUSB 复合设备（CDC ACM + MSC + HID），MSC 为 2TB 假 U 盘
 *                      （预制 MBR+FAT32，读返回预制结构/0，写全部丢弃）。
 *                      CDC/HID 仅注册端点验证初始化，无实际业务。
 ********************************************************************************/
#include "usbd_core.h"
#include "usbd_msc.h"
#include "usbd_cdc_acm.h"
#include "usbd_hid.h"

#define CDC_IN_EP   0x81
#define CDC_OUT_EP  0x02
#define CDC_INT_EP  0x83

#define MSC_IN_EP   0x84
#define MSC_OUT_EP  0x05

#define HID_INT_EP          0x86
#define HID_INT_EP_SIZE     4
#define HID_INT_EP_INTERVAL 10

/* 沿用原工程 VID/PID (WCH 0x1A86 / 0xFE0C) */
#define USBD_VID       0x1A86
#define USBD_PID       0xFE0D /* 换 PID 触发 Windows 全新 usbstor 加载 */
#define USBD_MAX_POWER 100

#define USB_CONFIG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN + MSC_DESCRIPTOR_LEN + 25)

#define CDC_MAX_MPS 64
#define MSC_MAX_MPS 64

#define HID_MOUSE_REPORT_DESC_SIZE 74

/* ------------------------------------------------------------------------- */
/* 描述符                                                                     */
/* ------------------------------------------------------------------------- */
static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0200, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x04, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02),
    MSC_DESCRIPTOR_INIT(0x02, MSC_OUT_EP, MSC_IN_EP, MSC_MAX_MPS, 0x02),
    HID_MOUSE_DESCRIPTOR_INIT(0x03, 0x01, HID_MOUSE_REPORT_DESC_SIZE, HID_INT_EP, HID_INT_EP_SIZE, HID_INT_EP_INTERVAL),
};

static const uint8_t device_quality_descriptor[] = {
    /* device qualifier descriptor */
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "WCH",                        /* Manufacturer */
    "CH32L103 Composite",         /* Product */
    "L103-MSC-EXFAT-01",          /* Serial Number（换新触发 Windows 全新枚举） */
};

/* 最小 BOS 描述符：避免 Windows 周期性请求 0x0F 时报错（同 CDC 版） */
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

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor msc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
    .bos_descriptor = &cdc_bos,
};

/* ------------------------------------------------------------------------- */
/* 事件回调：MSC 类由 usbd_msc 内部处理，无需额外逻辑                          */
/* ------------------------------------------------------------------------- */
static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        case USBD_EVENT_RESET:
        case USBD_EVENT_CONNECTED:
        case USBD_EVENT_DISCONNECTED:
        case USBD_EVENT_RESUME:
        case USBD_EVENT_SUSPEND:
        case USBD_EVENT_CONFIGURED:
        case USBD_EVENT_SET_REMOTE_WAKEUP:
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* 预制 MBR + FAT32 引导（1TB 假盘：读返回预制结构/0，写全部丢弃）            */
/*   分区：LBA 2048 起，类型 0x0C，大小 0x7FFFF800 扇区（1TB）               */
/*   FAT32：簇 128 扇区(64KB)，FAT 表 131056 扇区(67MB) ×2，根目录簇 2                         */
/*   FAT 头(FAT[0..2])虚拟返回；FAT/目录/数据读全 0 = 空盘                    */
/* ------------------------------------------------------------------------- */
#define FAKE_PART_LBA 2048
#define FAKE_RSVD      32
#define FAKE_FAT_LBA   (FAKE_PART_LBA + FAKE_RSVD)

static const uint8_t fake_mbr[512] = {
    [0x1B8] = 0x78, [0x1B9] = 0x56, [0x1BA] = 0x34, [0x1BB] = 0x12,
    [0x1BE] = 0x00,
    [0x1BF] = 0x00, [0x1C0] = 0x02, [0x1C1] = 0x00,
    [0x1C2] = 0x0C,
    [0x1C3] = 0xFF, [0x1C4] = 0xFF, [0x1C5] = 0xFF,
    [0x1C6] = 0x00, [0x1C7] = 0x08, [0x1C8] = 0x00, [0x1C9] = 0x00,
    [0x1CA] = 0x00, [0x1CB] = 0xF8, [0x1CC] = 0xFF, [0x1CD] = 0x7F,
    [510] = 0x55, [511] = 0xAA,
};

static const uint8_t fake_bpb[512] = {
    [0x000] = 0xEB, [0x001] = 0x58, [0x002] = 0x90, [0x003] = 0x4D, [0x004] = 0x53, [0x005] = 0x44,
    [0x006] = 0x4F, [0x007] = 0x53, [0x008] = 0x35, [0x009] = 0x2E, [0x00A] = 0x30, [0x00C] = 0x02,
    [0x00D] = 0x80, [0x00E] = 0x20, [0x010] = 0x02, [0x015] = 0xF8, [0x018] = 0x3F, [0x01A] = 0xFF,
    [0x01D] = 0x08, [0x020] = 0x00, [0x021] = 0xF8, [0x022] = 0xFF, [0x023] = 0x7F, [0x024] = 0xF0,
    [0x025] = 0xFF, [0x026] = 0x01, [0x02C] = 0x02, [0x030] = 0x01, [0x032] = 0x06, [0x040] = 0x80, [0x042] = 0x29,
    [0x043] = 0x78, [0x044] = 0x56, [0x045] = 0x34, [0x046] = 0x12, [0x047] = 0x46, [0x048] = 0x41,
    [0x049] = 0x4B, [0x04A] = 0x45, [0x04B] = 0x32, [0x04C] = 0x35, [0x04D] = 0x36, [0x04E] = 0x47,
    [0x04F] = 0x42, [0x050] = 0x20, [0x051] = 0x20, [0x052] = 0x46, [0x053] = 0x41, [0x054] = 0x54,
    [0x055] = 0x33, [0x056] = 0x32, [0x057] = 0x20, [0x058] = 0x20, [0x059] = 0x20, [0x1FE] = 0x55,
    [0x1FF] = 0xAA,
};

static const uint8_t fake_fsinfo[512] = {
    [0x000] = 0x52, [0x001] = 0x52, [0x002] = 0x61, [0x003] = 0x41,
    [0x1E4] = 0x72, [0x1E5] = 0x72, [0x1E6] = 0x41, [0x1E7] = 0x61,
    [0x1E8] = 0xFF, [0x1E9] = 0xFF, [0x1EA] = 0xFF, [0x1EB] = 0xFF,
    [0x1EC] = 0x03, [0x1FE] = 0x55, [0x1FF] = 0xAA,
};

static const uint8_t fake_fat_head[12] = {
    0xF8, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0x0F
};

void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint32_t *block_size)
{
    *block_num = 0x80000000; /* 假容量 1TB */
    *block_size = 512;
}

int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    uint32_t i;

    if (sector == 0) {
        memcpy(buffer, fake_mbr, length);
    } else if (sector == FAKE_PART_LBA) {
        memcpy(buffer, fake_bpb, length);
    } else if (sector == FAKE_PART_LBA + 1) {
        memcpy(buffer, fake_fsinfo, length);
    } else if (sector == FAKE_FAT_LBA) {
        memcpy(buffer, fake_fat_head, 12);
        for (i = 12; i < length; i++) {
            buffer[i] = 0x00;
        }
    } else {
        for (i = 0; i < length; i++) {
            buffer[i] = 0x00;
        }
    }
    return 0;
}

int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    /* 整蛊核心：所有写入直接丢弃，永远假装成功 */
    return 0;
}

/* ------------------------------------------------------------------------- */
/* HID 鼠标报告描述符（仅注册用，无实际上报业务）                               */
/* ------------------------------------------------------------------------- */
static const uint8_t hid_mouse_report_desc[HID_MOUSE_REPORT_DESC_SIZE] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x02, // USAGE (Mouse)
    0xA1, 0x01, // COLLECTION (Application)
    0x09, 0x01, // USAGE (Pointer)
    0xA1, 0x00, // COLLECTION (Physical)
    0x05, 0x09, // USAGE_PAGE (Button)
    0x19, 0x01, // USAGE_MINIMUM (Button 1)
    0x29, 0x03, // USAGE_MAXIMUM (Button 3)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x95, 0x03, // REPORT_COUNT (3)
    0x75, 0x01, // REPORT_SIZE (1)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x05, // REPORT_SIZE (5)
    0x81, 0x01, // INPUT (Cnst,Var,Abs)
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x30, // USAGE (X)
    0x09, 0x31, // USAGE (Y)
    0x09, 0x38, // Usage (Wheel)
    0x15, 0x81, // LOGICAL_MINIMUM (-127)
    0x25, 0x7F, // LOGICAL_MAXIMUM (127)
    0x75, 0x08, // REPORT_SIZE (8)
    0x95, 0x03, // REPORT_COUNT (2)
    0x81, 0x06, // INPUT (Data,Var,Rel)
    0xC0,       // END_COLLECTION
    0x09, 0x3c, // USAGE (Motion Wakeup)
    0x05, 0xff, // USAGE_PAGE (Vendor Defined 0xFF)
    0x09, 0x01, // USAGE (Vendor Usage 1)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x75, 0x01, // REPORT_SIZE (1)
    0x95, 0x02, // REPORT_COUNT (2)
    0xb1, 0x22, // FEATURE (Data,Var,Abs,NPrf)
    0x75, 0x06, // REPORT_SIZE (6)
    0x95, 0x01, // REPORT_COUNT (1)
    0xb1, 0x01, // FEATURE (Cnst,Ary,Abs)
    0xc0        // END_COLLECTION
};

/* ------------------------------------------------------------------------- */
/* 对外接口：CDC/HID 业务为空（ep_cb = NULL，core 对 NULL 回调有保护）        */
/* ------------------------------------------------------------------------- */
static struct usbd_interface intf0;
static struct usbd_interface intf1;
static struct usbd_interface intf2;
static struct usbd_interface intf3;

void msc_ram_init(uint8_t busid)
{
    usbd_desc_register(busid, &msc_descriptor);

    /* CDC ACM：接口 0/1（控制/数据），端点 IN 0x81 / OUT 0x02 / INT 0x83 */
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf0));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf1));
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = CDC_OUT_EP, .ep_cb = NULL });
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = CDC_IN_EP, .ep_cb = NULL });

    /* MSC：接口 2，OUT 0x05 / IN 0x84（usbd_msc_init_intf 自注册端点） */
    usbd_add_interface(busid, usbd_msc_init_intf(busid, &intf2, MSC_OUT_EP, MSC_IN_EP));

    /* HID：接口 3，INT IN 0x86 */
    usbd_add_interface(busid, usbd_hid_init_intf(busid, &intf3, hid_mouse_report_desc, HID_MOUSE_REPORT_DESC_SIZE));
    usbd_add_endpoint(busid, &(struct usbd_endpoint){ .ep_addr = HID_INT_EP, .ep_cb = NULL });

#ifdef CONFIG_CH32_USBFS
    usbd_initialize(busid, 0x50000000, usbd_event_handler); /* USBFS 基址 */
#else
    usbd_initialize(busid, 0x40005C00, usbd_event_handler); /* USBD (F103 兼容) 基址 */
#endif
}
