/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_dc_low_level_init.c
 * Description        : CH32L103 USBFS 底层初始化（时钟 / SIE 复位 / NVIC）。
 *                      CherryUSB 的 __WEAK 钩子，由 usb_dc_usbfs.c 调用。
 ********************************************************************************/
#include "usbd_core.h"
#include "ch32l103_conf.h"

void usb_dc_low_level_init(void)
{
    /* USB 需要 48MHz 时钟，按系统主频选择 PLL 分频（同官方 USBFS_RCC_Init）：
     * L103 分频档：48M->Div1, 72M->Div1_5, 96M->Div2 */
    if (SystemCoreClock == 96000000) {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div2);
    } else if (SystemCoreClock == 72000000) {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1_5);
    } else if (SystemCoreClock == 48000000) {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1);
    }
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBFS, ENABLE);

    /* SIE 复位 + FIFO 清除（对照官方 USBFS_Device_Init） */
    USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    Delay_Us(10);
    USBFSH->BASE_CTRL = 0x00;

    /* 上拉由端口 BASE_CTRL SYS_CTRL=1x（内部 1.5K）提供，无需额外配置 */
    /* 中断：端口自带 USBFS_IRQHandler（向量 59） */
    NVIC_EnableIRQ(USBFS_IRQn);
}
