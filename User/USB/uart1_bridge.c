/********************************** (C) COPYRIGHT *******************************
 * File Name          : uart1_bridge.c
 * Description        : UART1(PA9/PA10) 驱动 + FIFO 桥接数据面 + 主循环轮询。
 *                      调试口与桥接共用 USART1：printf 经 uart1_putc 走 TX FIFO，
 *                      与 USB OUT 数据共用同一条发送链路，避免互相踩踏。
 ********************************************************************************/
#include "usb_ringbuffer.h"
#include "ch32l103_conf.h"
#include "usb_cdc.h"
#include "uart1_bridge.h"

/* ------------------------------------------------------------------------- */
/* FIFO 数据面：TX = USB OUT + printf -> UART1；RX = UART1 -> USB IN            */
/* ------------------------------------------------------------------------- */
#define FIFO_SIZE 512
static usb_ringbuffer_t uart1_tx_fifo; /* USB -> UART1 */
static usb_ringbuffer_t uart1_rx_fifo; /* UART1 -> USB */
__attribute__((aligned(4))) static uint8_t uart1_tx_pool[FIFO_SIZE];
__attribute__((aligned(4))) static uint8_t uart1_rx_pool[FIFO_SIZE];

void bridge_tx_push(uint8_t c)
{
    usb_ringbuffer_write_byte(&uart1_tx_fifo, c);
}

bool bridge_tx_pop(uint8_t *c)
{
    return usb_ringbuffer_read_byte(&uart1_tx_fifo, c);
}

void bridge_rx_push(uint8_t c)
{
    usb_ringbuffer_write_byte(&uart1_rx_fifo, c);
}

bool bridge_rx_pop(uint8_t *c)
{
    return usb_ringbuffer_read_byte(&uart1_rx_fifo, c);
}

/* ------------------------------------------------------------------------- */
/* UART1 驱动                                                                 */
/* ------------------------------------------------------------------------- */
void UART1_Simple_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA | RCC_PB2Periph_USART1, ENABLE);

    /* PA9: TX 复用推挽 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    /* PA10: RX 上拉输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    /* RXNE 中断：串口数据进 FIFO */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);

    usb_ringbuffer_init(&uart1_tx_fifo, uart1_tx_pool, FIFO_SIZE);
    usb_ringbuffer_init(&uart1_rx_fifo, uart1_rx_pool, FIFO_SIZE);
}

/* printf 出口：与 USB OUT 数据共用 UART1 TX FIFO */
void uart1_putc(uint8_t c)
{
    bridge_tx_push(c);
}

void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t byte = USART_ReceiveData(USART1);
        bridge_rx_push(byte); /* 满则丢弃 */
    }
}

/* ------------------------------------------------------------------------- */
/* 主循环轮询                                                                 */
/* ------------------------------------------------------------------------- */
void cdc_poll(void)
{
    /* UART1 RX FIFO -> USB IN */
    if (!cdc_tx_busy()) {
        uint8_t buf[64];
        uint32_t n = 0;
        while (n < 64 && bridge_rx_pop(&buf[n])) {
            n++;
        }
        if (n > 0) {
            cdc_uart_data_upload(buf, n);
        }
    }

    /* USB OUT FIFO (+ printf) -> UART1 TX */
    uint8_t byte;
    while (bridge_tx_pop(&byte)) {
        while ((USART1->STATR & USART_FLAG_TXE) == 0) {
        }
        USART1->DATAR = byte;
    }
}
