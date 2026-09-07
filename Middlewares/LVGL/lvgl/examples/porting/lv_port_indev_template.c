/**
  ******************************************************************************
  * @file    lv_port_indev_template.c
  * @brief   LVGL v8 输入移植层 — 接 interface/touch.c (CST816, I2C1)
  *          移植自 FryPi OV_Watch lv_port_indev.c (GPL-3.0)。
  *          轮询模式: LVGL 每 LV_INDEV_DEF_READ_PERIOD 周期调 touchpad_read 一次,
  *          每次一笔 4 字节连读 (FingerNum 与坐标合并语义见 touch.c)。
  *          备注: PB9 EXTI + g_tp_int_flag 留作低功耗唤醒扩展, 驱动走轮询。
  ******************************************************************************
***/
#include "lv_port_indev_template.h"
#include "lvgl.h"
#include "touch.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t *indev_touchpad;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    touchpad_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void touchpad_init(void)
{
    (void)Touch_Init();     /* 返回 ChipID (CST816S≈0xB5); 0 表示总线未见器件 */
}

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    uint16_t tx = 0, ty = 0;

    if (Touch_GetPoint(&tx, &ty)) {               /* 一次调用即判压+取坐标 */
        last_x = (lv_coord_t)tx;
        last_y = (lv_coord_t)ty;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;    /* 坐标保持 last_x/y 不变 */
    }
    data->point.x = last_x;
    data->point.y = last_y;
}
