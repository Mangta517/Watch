/**
  ******************************************************************************
  * @file    touch.h
  * @brief   CST816 电容触摸驱动 (硬件 I2C1, 从机地址 0x15)
  *          移植自 FryPi OV_Watch BSP/TOUCH/CST816.c (GPL-3.0), 软件 IIC 改为
  *          HAL_I2C 内存读写。引脚: SCL=PB6 SDA=PB7 RST=PB8 INT=PB9(EXTI下降沿)。
  ******************************************************************************
***/
#ifndef __TOUCH_H
#define __TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* 返回读到的 ChipID (CST816S≈0xB5, CST816D≈0xB7); 总线异常返回 0 */
uint8_t Touch_Init(void);

extern volatile uint8_t g_tp_chip_id;   /* 最近一次 ChipID 读数 (0xB5/0xB7=I2C通, 0=总线死) */

uint8_t Touch_GetChipID(void);

/* 读取一个触点: 返回手指数(0=无触摸/读失败), x/y 为屏幕坐标(与 LCD 对齐) */
uint8_t Touch_GetPoint(uint16_t *x, uint16_t *y);

void    Touch_Reset(void);
void    Touch_Sleep(void);                       /* 进低功耗(触摸唤醒) */
void    Touch_Wakeup(void);                       /* 复位唤醒 */

#ifdef __cplusplus
}
#endif

#endif /* __TOUCH_H */
