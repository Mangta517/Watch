/**
  ******************************************************************************
  * @file    lcd.h
  * @brief   ST7789 240x280 LCD 驱动 (SPI1 + 软件CS + DC/RST GPIO + PA15背光PWM)
  *          移植自 FryPi OV_Watch BSP/LCD (GPL-3.0, 作者 No-Chicken),
  *          去掉字库/画图例程, 仅保留 LVGL 集成所需接口; OV 的 DMA 刷屏因本工程
  *          CubeMX 未配 DMA 而改为阻塞整块发送, 语义不变。
  *          引脚: SCK=PB3 MOSI=PB5 MISO=PA6(未用) CS=PD2 DC=PC12 RST=PB4 BLK=PA15(TIM2_CH1)
  ******************************************************************************
***/
#ifndef __LCD_H
#define __LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* 面板参数: 竖屏 USE_HORIZONTAL=1, MADCTL=0xC0, ST7789 显存320行 -> 280行面板 Y 偏移20 */
#define LCD_WIDTH       240
#define LCD_HEIGHT      280
#define LCD_OFFSET_Y    20

/* 背光极性映射: 当前 CubeMX 里 TIM2 CH1 OCPolarity = LOW。
 * OV_Watch 原工程是 HIGH + 高电平点亮, 电路为高有效。
 * 在 LOW 极性下, 引脚高电平占比 = (ARR-CCR)/ARR, 故需要 CCR 反转映射。
 * 若你在 CubeMX 把极性改回 HIGH, 把此宏改为 0。 */
#define LCD_BL_CCR_INVERT   1
#define LCD_BL_ARR          300       /* TIM2.Period, 与 CubeMX 保持一致 */

/* RGB565 常用色 */
#define LCD_BLACK       0x0000
#define LCD_WHITE       0xFFFF
#define LCD_RED         0xF800
#define LCD_GREEN       0x07E0
#define LCD_BLUE        0x001F

/*--- 初始化 / 电源 ---*/
void LCD_Init(void);                              /* 复位+ST7789初始化序列, 结束后CS常拉低 */
void LCD_ST7789_SleepIn(void);                    /* 0x10 省电 */
void LCD_ST7789_SleepOut(void);                   /* 0x11 唤醒 */

/*--- 基础写 ---*/
void LCD_WR_REG(uint8_t reg);                     /* DC=0 写命令 */
void LCD_WR_DATA8(uint8_t dat);                   /* DC=1 写一字节 */
void LCD_WR_DATA(uint16_t dat);                   /* DC=1 写一字(高字节先出) */
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2); /* 已含 OFFSET_Y 补偿 */

/*--- 刷屏接口 ---*/
void LCD_Color_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, const uint16_t *color_p); /* RGB565块传输(阻塞) */

/* DMA 异步刷屏(SPI1_TX DMA, OV_Watch 同款): 发起即返回,
 * 传输完成经 HAL_SPI_TxCpltCallback → LCD_Set_Flush_Complete_Callback 注册的回调通知 */
typedef void (*LCD_CallbackFunc_t)(void);
void LCD_Set_Flush_Complete_Callback(LCD_CallbackFunc_t cb);
void LCD_Color_Fill_DMA(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, const uint16_t *color_p);

void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);                /* 单色填充 */
void LCD_Clear(uint16_t color);                                                                             /* 全屏填充 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);

/*--- 背光 ---*/
void LCD_Open_Light(void);                        /* 启动 TIM2_CH1 PWM */
void LCD_Set_Light(uint8_t percent);              /* 0~100 亮度 */
void LCD_Close_Light(void);                       /* 关背光 */

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H */
