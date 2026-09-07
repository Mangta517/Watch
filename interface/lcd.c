/**
  ******************************************************************************
  * @file    lcd.c
  * @brief   ST7789 (240x280, 竖屏) SPI1 驱动 — 移植自 FryPi OV_Watch BSP/LCD
  *          (lcd_init.c ST7789 初始化序列 + lcd.c Color_Fill, GPL-3.0)。
  *          改动: ①CS/DC/RST 复用 CubeMX 已生成的引脚宏, 不再自建 GPIO;
  *                ②DMA 刷屏改阻塞整块发送(本工程未配 DMA), 大块自动分片;
  *                ③背光亮度带极性映射, 兼容当前 OCPolarity=LOW 配置。
  *          依赖: hspi1(spi.c), htim2(tim.c), MX_GPIO_Init 已配置 CS/DC/RST 输出。
  ******************************************************************************
***/
#include "lcd.h"
#include "spi.h"
#include "tim.h"

/* 引脚快捷宏 (全部来自 CubeMX 生成的 main.h) */
#define LCD_CS_Clr()  HAL_GPIO_WritePin(LCD_CS_GPIO_Port,  LCD_CS_Pin,  GPIO_PIN_RESET)
#define LCD_CS_Set()  HAL_GPIO_WritePin(LCD_CS_GPIO_Port,  LCD_CS_Pin,  GPIO_PIN_SET)
#define LCD_DC_Clr()  HAL_GPIO_WritePin(LCD_DC_GPIO_Port,  LCD_DC_Pin,  GPIO_PIN_RESET)
#define LCD_DC_Set()  HAL_GPIO_WritePin(LCD_DC_GPIO_Port,  LCD_DC_Pin,  GPIO_PIN_SET)
#define LCD_RES_Clr() HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET)
#define LCD_RES_Set() HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET)

/* 本工程 SPI1 为 2LINES 全双工接线但只发送(MISO悬空, ST7789 无回读),
 * 与 OV_Watch 相同的 "全双工外设跑单向" 用法, RXNE/OVR 忽略。 */
#define LCD_SPI_TMO   HAL_MAX_DELAY

/******************************************************************************
 * 底层: 硬件SPI 写
 *****************************************************************************/
static void LCD_Writ_Bus(uint8_t dat)
{
    HAL_SPI_Transmit(&hspi1, &dat, 1, LCD_SPI_TMO);
}

void LCD_WR_DATA8(uint8_t dat)
{
    LCD_Writ_Bus(dat);
}

/* 16bit 数据高字节先出 (ST7789 大端要求, 与 LVGL 的 LV_COLOR_16_SWAP=1 配套) */
void LCD_WR_DATA(uint16_t dat)
{
    uint8_t tmp[2];
    tmp[0] = (uint8_t)(dat >> 8);
    tmp[1] = (uint8_t)(dat);
    HAL_SPI_Transmit(&hspi1, tmp, 2, LCD_SPI_TMO);
}

void LCD_WR_REG(uint8_t reg)
{
    LCD_DC_Clr();             /* 命令 */
    LCD_Writ_Bus(reg);
    LCD_DC_Set();             /* 恢复数据模式 */
}

/******************************************************************************
 * 地址窗口 (自动加 Y 偏移; 0x2A 高16bit恒0, 0x2B 按16bit发)
 *****************************************************************************/
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    y1 += LCD_OFFSET_Y;
    y2 += LCD_OFFSET_Y;

    LCD_WR_REG(0x2A);         /* 列地址 */
    LCD_WR_DATA8(0x00); LCD_WR_DATA8((uint8_t)x1);
    LCD_WR_DATA8(0x00); LCD_WR_DATA8((uint8_t)x2);
    LCD_WR_REG(0x2B);         /* 行地址 */
    LCD_WR_DATA8((uint8_t)(y1 >> 8)); LCD_WR_DATA8((uint8_t)y1);
    LCD_WR_DATA8((uint8_t)(y2 >> 8)); LCD_WR_DATA8((uint8_t)y2);
    LCD_WR_REG(0x2C);         /* 进入显存写入 */
}

/******************************************************************************
 * 刷屏接口
 *****************************************************************************/
/* LVGL flush: 把 w*h 个 RGB565 像素发到指定窗口 (字节序由 LV_COLOR_16_SWAP=1 保证)。
 * HAL_SPI_Transmit 的 Size 是 uint16_t, 超过 64KB 的块自动分片;
 * GRAM 游标在窗口内自动前进, 分片之间不需要重设地址。 */
void LCD_Color_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, const uint16_t *color_p)
{
    uint32_t bytes = (uint32_t)(xend - xsta + 1) * (uint32_t)(yend - ysta + 1) * 2U;
    const uint8_t *p = (const uint8_t *)color_p;

    LCD_Address_Set(xsta, ysta, xend, yend);
    while (bytes > 0xFFFFU) {                     /* 每片 <=64KB-2, 保证整像素边界 */
        HAL_SPI_Transmit(&hspi1, (uint8_t *)p, 0xFFFEU, LCD_SPI_TMO);
        p += 0xFFFEU;
        bytes -= 0xFFFEU;
    }
    if (bytes) HAL_SPI_Transmit(&hspi1, (uint8_t *)p, (uint16_t)bytes, LCD_SPI_TMO);
}

/* 单色填充(局部): 用 64 像素的重复色缓冲分片写 */
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color)
{
    uint32_t px = (uint32_t)(xend - xsta + 1) * (uint32_t)(yend - ysta + 1);
    uint16_t buf[64];
    uint32_t i;

    for (i = 0; i < 64; i++) buf[i] = color;

    LCD_Address_Set(xsta, ysta, xend, yend);
    while (px >= 64U) { HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, 128, LCD_SPI_TMO); px -= 64U; }
    if (px) HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, (uint16_t)(px * 2U), LCD_SPI_TMO);
}

void LCD_Clear(uint16_t color)
{
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, color);
}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_Address_Set(x, y, x, y);
    LCD_WR_DATA(color);
}

/******************************************************************************
 * 背光 (TIM2_CH1 PWM, ARR=300, 频率约3.33kHz)
 *****************************************************************************/
void LCD_Open_Light(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

void LCD_Set_Light(uint8_t percent)
{
    uint32_t ccr;
    if (percent > 100) percent = 100;
#if LCD_BL_CCR_INVERT
    ccr = (uint32_t)(100U - percent) * LCD_BL_ARR / 100U;   /* 低极性: CCR 越小越亮 */
#else
    ccr = (uint32_t)percent * LCD_BL_ARR / 100U;
#endif
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);
}

void LCD_Close_Light(void)
{
    LCD_Set_Light(0);
}

/******************************************************************************
 * ST7789 初始化序列 (来自 OV_Watch lcd_init.c, USE_HORIZONTAL==1 竖屏分支展开)
 *****************************************************************************/
void LCD_Init(void)
{
    /* CS/DC/RST 已由 CubeMX 的 MX_GPIO_Init() 配好推挽输出, 这里只管电平 */
    LCD_CS_Clr();             /* 单从机, CS 常拉低 (同 OV_Watch) */

    LCD_RES_Clr();            /* 硬复位 */
    HAL_Delay(100);
    LCD_RES_Set();
    HAL_Delay(100);

    LCD_WR_REG(0x11);         /* Sleep Out */
    HAL_Delay(120);

    LCD_WR_REG(0x36); LCD_WR_DATA8(0xC0);   /* MADCTL: 竖屏 */
    LCD_WR_REG(0x3A); LCD_WR_DATA8(0x05);   /* 16bit RGB565 */

    LCD_WR_REG(0xB2);                       /* Porch: 0C 0C 00 33 33 */
    LCD_WR_DATA8(0x0C); LCD_WR_DATA8(0x0C); LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x33); LCD_WR_DATA8(0x33);

    LCD_WR_REG(0xB7); LCD_WR_DATA8(0x35);   /* Gate Control */
    LCD_WR_REG(0xBB); LCD_WR_DATA8(0x19);   /* VCOM */
    LCD_WR_REG(0xC0); LCD_WR_DATA8(0x2C);   /* LCM */
    LCD_WR_REG(0xC2); LCD_WR_DATA8(0x01);   /* VDV/VRH Enable */
    LCD_WR_REG(0xC3); LCD_WR_DATA8(0x12);   /* VRH Set */
    LCD_WR_REG(0xC4); LCD_WR_DATA8(0x20);   /* VDV Set */
    LCD_WR_REG(0xC6); LCD_WR_DATA8(0x0F);   /* Frame Rate 60Hz */
    LCD_WR_REG(0xD0); LCD_WR_DATA8(0xA4); LCD_WR_DATA8(0xA1); /* Power Enable */

    LCD_WR_REG(0xE0);                       /* GTCT 正极性 14 字节 */
    LCD_WR_DATA8(0xD0); LCD_WR_DATA8(0x04); LCD_WR_DATA8(0x0D); LCD_WR_DATA8(0x11);
    LCD_WR_DATA8(0x13); LCD_WR_DATA8(0x2B); LCD_WR_DATA8(0x3F); LCD_WR_DATA8(0x54);
    LCD_WR_DATA8(0x4C); LCD_WR_DATA8(0x18); LCD_WR_DATA8(0x0D); LCD_WR_DATA8(0x0B);
    LCD_WR_DATA8(0x1F); LCD_WR_DATA8(0x23);

    LCD_WR_REG(0xE1);                       /* GTCT 负极性 14 字节 */
    LCD_WR_DATA8(0xD0); LCD_WR_DATA8(0x04); LCD_WR_DATA8(0x0C); LCD_WR_DATA8(0x11);
    LCD_WR_DATA8(0x13); LCD_WR_DATA8(0x2C); LCD_WR_DATA8(0x3F); LCD_WR_DATA8(0x44);
    LCD_WR_DATA8(0x51); LCD_WR_DATA8(0x2F); LCD_WR_DATA8(0x1F); LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x20); LCD_WR_DATA8(0x23);

    LCD_WR_REG(0x21);                       /* Display Inversion ON (OV_Watch 实配) */
    LCD_WR_REG(0x29);                       /* Display ON */

    LCD_Open_Light();
    LCD_Set_Light(80);                      /* 默认 80% 亮度 */
}

/******************************************************************************
 * 省电
 *****************************************************************************/
void LCD_ST7789_SleepIn(void)
{
    LCD_WR_REG(0x10);
    HAL_Delay(100);
}

void LCD_ST7789_SleepOut(void)
{
    LCD_WR_REG(0x11);
    HAL_Delay(100);
}
