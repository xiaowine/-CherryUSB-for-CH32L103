/********************************** (C) COPYRIGHT *******************************
 * File Name          : msc_ram.h
 * Description        : CherryUSB MSC RAM 模拟盘（10KB）对外接口。
 *                      注意：不可命名 usb_msc.h，与 CherryUSB class/msc 头文件冲突。
 ********************************************************************************/
#ifndef __MSC_RAM_H
#define __MSC_RAM_H

void msc_ram_init(uint8_t busid);

#endif /* __MSC_RAM_H */
