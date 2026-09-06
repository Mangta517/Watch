/**
 * @file lv_port_disp_templ.c
 *
 */

 /*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 0

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include "lvgl.h"
#include "lcd.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//        const lv_area_t * fill_area, lv_color_t color);

/**********************
 *  STATIC VARIABLES
 **********************/
/* LVGL绘制缓冲区: 放在内部SRAM(.bss, 0x20000000区域), 渲染热路径保持内部访问 */
/* 内部SRAM 64KB: LVGL对象堆已移至外部SRAM, 内部内存池40KB→10KB瘦身, 腾出空间 */
/* 双缓冲(24行x2): v8.3 渲染 part N+1 与 DMA 搬运 part N 重叠(lv_refr自动交替buf_act),
 * 渲染只写"非搬运中"的缓冲, 互不踩踏。RAM账: 原单缓冲40行25.6KB, 内部仅剩9.8KB空闲,
 * 放不下第二块; 改 2x24行 = 30.7KB(比原来还多花4.8KB), 剩约4.8KB余量。
 * 整屏刷新分 480/24=20 次flush, 每次窗口设置开销仅十几个FSMC写, 可忽略。 */
#define LVGL_BUF_LINES  24
static lv_color_t buf_1[320 * LVGL_BUF_LINES];
static lv_color_t buf_2[320 * LVGL_BUF_LINES];

#if LCD_FILL_DMA
static lv_disp_drv_t * s_disp_drv = NULL;                /* 记录显示驱动, 供DMA完成回调使用 */
static void flush_ready_wrapper(void)
{
    if (s_disp_drv)
        lv_disp_flush_ready(s_disp_drv);
}
#endif
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/


    /* 双缓冲模式: 两块24行缓冲均在内部SRAM, 渲染与DMA搬运交替并行 */
    static lv_disp_draw_buf_t draw_buf_dsc_1;
    lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, buf_2, 320 * LVGL_BUF_LINES);

    /* Example for 3) also set disp_drv.full_refresh = 1 below*/
//    static lv_disp_draw_buf_t draw_buf_dsc_3;
//    static lv_color_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES];            /*A screen sized buffer*/
//    static lv_color_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES];            /*Another screen sized buffer*/
//    lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2, MY_DISP_VER_RES * LV_VER_RES_MAX);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = lcddev.width;
    disp_drv.ver_res = lcddev.height;

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc_1;

    /*Required for Example 3)*/
    //disp_drv.full_refresh = 1

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);

#if LCD_FILL_DMA
    s_disp_drv = &disp_drv;                /* 记录驱动句柄给DMA完成回调 */
    lcd_dma_init();                        /* 初始化LCD填充DMA通道 */
    lcd_set_flush_ready_cb(flush_ready_wrapper);  /* 让DMA完成中断去调flush_ready */
#endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    lcd_init();

    /* 切为竖屏。lcd_display_dir() 内部会经 lcd_scan_dir() 同时设置:
     *   宽高(lcddev.width/height) + MADCTL扫描方向(0x36) + GRAM窗口坐标,
     * LVGL 随后读取 lcddev.width/height 作为 hor_res/ver_res, 软硬件保持一致。
     * NT35310(id 0x5310) 竖屏原生即 320*480。 */
    lcd_display_dir(0);             /* 竖屏 */

    /* 坑点说明(务必保留此注释): 这里【不再】手动覆盖 lcddev.width/height。
     * 手写覆盖会与 lcd_display_dir 抢所有权, 且不会重设 GRAM 窗口,
     * 一旦切换横竖屏就会尺寸不一致、显示错乱。
     * 若换用非 NT35310 的屏, 请核对 lcd_display_dir()/lcd_scan_dir() 中对应 id 分支,
     * 而不是在此直接改 lcddev。 */
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
#if LCD_FILL_DMA
    /* 发DMA把缓冲区搬进LCD GRAM, 完成由DMA中断回调调flush_ready(不在此处调) */
    lcd_fill_dma(area->x1, area->y1, area->x2, area->y2, (uint16_t*)color_p);
#else
    lcd_color_fill(area->x1, area->y1, area->x2, area->y2, (uint16_t*)color_p);
    lv_disp_flush_ready(disp_drv);
#endif
}

/*OPTIONAL: GPU INTERFACE*/

/*If your MCU has hardware accelerator (GPU) then you can use it to fill a memory with a color*/
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//                    const lv_area_t * fill_area, lv_color_t color)
//{
//    /*It's an example code which should be done by your GPU*/
//    int32_t x, y;
//    dest_buf += dest_width * fill_area->y1; /*Go to the first line*/
//
//    for(y = fill_area->y1; y <= fill_area->y2; y++) {
//        for(x = fill_area->x1; x <= fill_area->x2; x++) {
//            dest_buf[x] = color;
//        }
//        dest_buf+=dest_width;    /*Go to the next line*/
//    }
//}


#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
