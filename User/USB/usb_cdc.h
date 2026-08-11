/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.h
 * Description        : USB CDC ACM 设备层接口（描述符/事件/端点/上传）。
 ********************************************************************************/
#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdint.h>
#include <stdbool.h>

/* USB CDC 初始化（描述符 + 接口 + 端点 + usbd_initialize，基址按 CONFIG_CH32_USBFS 选择） */
void cdc_acm_init(uint8_t busid);

/* UART1 接收数据上传到 USB（EP3 IN）。EP 忙时返回 false，需稍后重试 */
bool cdc_uart_data_upload(const uint8_t *buf, uint32_t len);

/* EP3 IN 是否传输中 */
bool cdc_tx_busy(void);

#endif /* USB_CDC_H */
