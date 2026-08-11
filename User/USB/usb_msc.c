/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_msc.c
 * Description        : CherryUSB 复合设备（CDC ACM + MSC + HID），MSC 为 RAM 模拟盘
 *                      （16 扇区 × 512B，bss 静态数组，掉电丢失）。
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
#define USBD_PID       0xFE0C
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
    "L103-MSC-20260811",          /* Serial Number（唯一，避免与旧设备实例冲突） */
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
/* RAM 介质：10KB（20 扇区 × 512B），bss 静态数组                             */
/*   - 掉电丢失，符合 RAM 模拟盘语义；bss 由启动代码清零                       */
/*   - 越界写返回 -1（WRITE FAULT），绝不静默丢弃                             */
/* ------------------------------------------------------------------------- */
#define BLOCK_SIZE  512
#define BLOCK_COUNT 16 /* 8KB */

__attribute__((aligned(4))) static uint8_t mass_block[BLOCK_COUNT][BLOCK_SIZE];

void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint32_t *block_size)
{
    *block_num = BLOCK_COUNT; /* 真实容量：20 块 × 512B = 10KB */
    *block_size = BLOCK_SIZE;
}

int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    if ((sector + (length / BLOCK_SIZE)) > BLOCK_COUNT) {
        return -1;
    }
    memcpy(buffer, &mass_block[sector][0], length);
    return 0;
}

int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    if ((sector + (length / BLOCK_SIZE)) > BLOCK_COUNT) {
        return -1;
    }
    memcpy(&mass_block[sector][0], buffer, length);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* 启动预格式化 FAT12：Windows 对超小卷拒绝 format，但可挂载已有 FAT12 卷     */
/*   - 布局（20 扇区）：引导 1 + FAT 2 + 根目录 2 + 数据 15（簇=1 扇区）      */
/*   - 只写扇区 0(引导 BPB) 与 1-2(FAT×2)，根目录/数据区保持 bss 清零态       */
/*     （根目录 0x00 = 空目录结束符，数据区全 0 = 未分配，均为合法态）         */
/*   - 触发条件：扇区 0 无 0x55AA 签名；bss 清零后必然触发（每次上电重建，    */
/*     符合 RAM 盘掉电丢失语义）                                              */
/* ------------------------------------------------------------------------- */
#define FAT12_ROOT_ENTRIES 32
#define FAT12_RESERVED     1
#define FAT12_NUM_FATS     2
#define FAT12_FAT_SECTORS  1

static const uint8_t fat12_boot_sector[BLOCK_SIZE] = {
    0xEB, 0x3C, 0x90,                       /* BS_jmpBoot */
    'M', 'S', 'D', 'O', 'S', '5', '.', '0', /* BS_OEMName */
    0x00, 0x02,                             /* BPB_BytsPerSec = 512 */
    0x01,                                   /* BPB_SecPerClus = 1 */
    FAT12_RESERVED, 0x00,                   /* BPB_RsvdSecCnt = 1 */
    FAT12_NUM_FATS,                         /* BPB_NumFATs = 2 */
    FAT12_ROOT_ENTRIES, 0x00,               /* BPB_RootEntCnt = 32 */
    BLOCK_COUNT, 0x00,                      /* BPB_TotSec16 = 20 */
    0xF8,                                   /* BPB_Media = fixed disk */
    FAT12_FAT_SECTORS, 0x00,                /* BPB_FATSz16 = 1 */
    0x3F, 0x00,                             /* BPB_SecPerTrk = 63 */
    0xFF, 0x00,                             /* BPB_NumHeads = 255 */
    0x00, 0x00, 0x00, 0x00,                 /* BPB_HiddSec = 0 */
    0x00, 0x00, 0x00, 0x00,                 /* BPB_TotSec32 = 0 */
    0x80,                                   /* BS_DrvNum */
    0x00,                                   /* BS_Reserved1 */
    0x29,                                   /* BS_BootSig */
    0x12, 0x34, 0x56, 0x78,                 /* BS_VolID */
    'C', 'H', '3', '2', 'L', '1', '0', '3', ' ', ' ', ' ', /* BS_VolLab (11B) */
    'F', 'A', 'T', '1', '2', ' ', ' ', ' ', /* BS_FilSysType (8B) */
    /* 引导代码区（0x3E 起）全 0：USB 盘不引导，Windows 挂载不执行 */
    [510] = 0x55, [511] = 0xAA,             /* 签名 */
};

static void msc_preattach_format(void)
{
    uint8_t fat_table[BLOCK_SIZE] = { 0xF8, 0xFF, 0xFF }; /* FAT[0]=0xFF8, FAT[1]=0xFFF，其余 0=空闲 */

    if (*(volatile uint16_t *)&mass_block[0][510] == 0xAA55) {
        return; /* 已是合法 FAT12 卷 */
    }
    /* bss 已清零，直接写 BPB + FAT×2 到扇区 0-2；根目录/数据区保持 0 即可 */
    memcpy(&mass_block[0][0], fat12_boot_sector, BLOCK_SIZE);
    memcpy(&mass_block[1][0], fat_table, BLOCK_SIZE);
    memcpy(&mass_block[2][0], fat_table, BLOCK_SIZE);
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
    msc_preattach_format(); /* bss 清零后自动重建 FAT12 卷（纯内存，微秒级） */
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
