/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : CherryUSB MSC RAM 模拟盘 (CH32L103 USBFS, 10KB)。
 *                      调试口 UART1(PA9/PA10)，printf 直接发送。
 ********************************************************************************/
#include "debug.h"
#include "msc_ram.h"
#include "ch32l103_conf.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200); /* UART1 = 调试口（printf 直接发送） */
    printf("SystemClk:%d\r\n", SystemCoreClock);
    printf("CherryUSB MSC RAM (CH32L103)\r\n");

    msc_ram_init(0);

    while (1) {
    }
}
