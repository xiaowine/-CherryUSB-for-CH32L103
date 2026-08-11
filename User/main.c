/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : CherryUSB MSC RAM 模拟盘 (CH32L103 USBFS, 10KB)。
 *                      调试口 UART1(PA9/PA10)，printf 经 TX FIFO 同链路发送。
 ********************************************************************************/
#include "debug.h"
#include "msc_ram.h"
#include "uart1_bridge.h"
#include "ch32l103_conf.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    UART1_Simple_Init(); /* UART1 = 调试口（printf 经 TX FIFO 发送） */
    printf("SystemClk:%d\r\n", SystemCoreClock);
    printf("CherryUSB MSC RAM (CH32L103)\r\n");

    msc_ram_init(0);

    while (1) {
        /* UART1 TX 泵：printf（MSC 诊断日志）经 FIFO 发往串口。
         * 原 cdc_poll 的 TX 段；USB OUT 部分 MSC 不用，故只保留 TX。 */
        uint8_t byte;
        while (bridge_tx_pop(&byte)) {
            while ((USART1->STATR & USART_FLAG_TXE) == 0) {
            }
            USART1->DATAR = byte;
        }
    }
}
