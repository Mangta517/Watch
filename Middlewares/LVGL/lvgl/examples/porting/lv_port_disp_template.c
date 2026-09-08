/**
  ******************************************************************************
  * @file    lv_port_disp_template.c
  * @brief   LVGL v8 显示移植层 — 接 interface/lcd.c (ST7789 240x280)
  *          结构参照 FryPi OV_Watch lv_port_disp.c (GPL-3.0)。
  *          差异: 本工程 CubeMX 未配 DMA, OV 的 "DMA完成回调异步 flush"
  *          简化为阻塞 flush (LCD_Color_Fill 内 HAL_SPI_Transmit 整块发送)。
  *          缓冲策略: 双 partial buffer, 每块 1/10 屏 = 240*280/10*2B ≈ 13.4KB。
  *          前提: lv_conf.h 中 LV_COLOR_16_SWAP=1 (SPI 大端字节序)。
  ******************************************************************************
***/
#include "lv_port_disp_template.h"
#include "lvgl.h"
#include "lcd.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
/* 双块 1/10 屏 partial buffer, 内部 SRAM (.bss): LVGL 渲染 buf_2 的同时可阻塞发送 buf_1;
 * 保留两块是为将来改 DMA 时渲染/搬运并行 */
#define LVGL_BUF_PX   (LCD_WIDTH * LCD_HEIGHT / 10)
static lv_color_t buf_1[LVGL_BUF_PX];
static lv_color_t buf_2[LVGL_BUF_PX];

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_port_disp_init(void)
{
    /* 先初始化物理屏 (含刷黑与背光) */
    disp_init();

    static lv_disp_draw_buf_t draw_buf_dsc;
    lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, buf_2, LVGL_BUF_PX);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = LCD_WIDTH;
    disp_drv.ver_res  = LCD_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void disp_init(void)
{
    LCD_Init();
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    LCD_Color_Fill((uint16_t)area->x1, (uint16_t)area->y1,
                   (uint16_t)area->x2, (uint16_t)area->y2,
                   (const uint16_t *)color_p);
    lv_disp_flush_ready(disp_drv);      /* 阻塞发送, 返回即完成 */
}
