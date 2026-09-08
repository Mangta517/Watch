/**
  ******************************************************************************
  * @file    app_ui.c
  * @brief   首屏演示页 (240x280 竖屏, 字体仅启用 Montserrat 10/14)
  *          布局自上而下: 标题 / 触摸坐标行 / 圆形按压区 / 背光开关行
  ******************************************************************************
***/
#include "app_ui.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "lcd.h"

/* lv_port_indev_template.c 里注册指针设备的句柄 */
extern lv_indev_t * indev_touchpad;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touch_poll_cb(lv_timer_t *t);
static void backlight_switch_cb(lv_event_t *e);
static void pad_press_cb(lv_event_t *e);
static void stress_nav_cb(lv_event_t *e);
static void bench_nav_cb(lv_event_t *e);
static void key_poll_cb(lv_timer_t *t);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * s_home_scr;
static lv_obj_t * s_test_scr;      /* 官方 demo 的临时承载屏 (退出时删除) */
static lv_obj_t * s_touch_lbl;
static volatile uint8_t s_pad_down;   /* 按压状态: 由 pad 事件更新 (本 LVGL 版本无 lv_indev_get_state) */

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void App_UI_Create(void)
{
    lv_obj_t * scr = lv_scr_act();
    s_home_scr = scr;

    /* --- 标题 --- */
    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "F411 RET6 Watch");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    /* --- 触摸状态行 (由 100ms 软定时器刷新) --- */
    s_touch_lbl = lv_label_create(scr);
    lv_label_set_text(s_touch_lbl, "Touch: --,--");
    lv_obj_set_style_text_font(s_touch_lbl, &lv_font_montserrat_10, 0);
    lv_obj_align(s_touch_lbl, LV_ALIGN_TOP_MID, 0, 36);
    lv_timer_create(touch_poll_cb, 100, NULL);

    /* --- 圆形按压区: 按下变绿, 直观确认触摸落点与坐标镜像是否正确 --- */
    lv_obj_t * pad = lv_btn_create(scr);
    lv_obj_set_size(pad, 100, 100);
    lv_obj_set_style_radius(pad, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pad, lv_color_hex(0x303040), 0);
    lv_obj_set_style_bg_color(pad, lv_color_hex(0x2ECC71), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(pad, 0, 0);
    lv_obj_align(pad, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(pad, pad_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(pad, pad_press_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(pad, pad_press_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_t * pad_txt = lv_label_create(pad);
    lv_label_set_text(pad_txt, "PUSH");
    lv_obj_set_style_text_font(pad_txt, &lv_font_montserrat_14, 0);
    lv_obj_center(pad_txt);

    /* --- 背光档位开关: UI→interface 驱动的联动演示 (30% / 100%) --- */
    lv_obj_t * bl_sw = lv_switch_create(scr);
    lv_obj_set_size(bl_sw, 52, 28);
    lv_obj_align(bl_sw, LV_ALIGN_BOTTOM_MID, -50, -18);
    lv_obj_add_event_cb(bl_sw, backlight_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * bl_lbl = lv_label_create(scr);
    lv_label_set_text(bl_lbl, "Backlight\n100% / 30%");
    lv_obj_set_style_text_font(bl_lbl, &lv_font_montserrat_10, 0);
    lv_obj_align(bl_lbl, LV_ALIGN_BOTTOM_MID, 35, -18);

    /* --- 官方 demo 入口: LVGL v8.2 自带 stress/benchmark, KEY1(PA0) 短按退出 --- */
    lv_obj_t * st_btn = lv_btn_create(scr);
    lv_obj_set_size(st_btn, 62, 26);
    lv_obj_align(st_btn, LV_ALIGN_BOTTOM_LEFT, 8, -16);
    lv_obj_set_style_radius(st_btn, 6, 0);
    lv_obj_add_event_cb(st_btn, stress_nav_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * st_l = lv_label_create(st_btn);
    lv_label_set_text(st_l, "STRESS");
    lv_obj_set_style_text_font(st_l, &lv_font_montserrat_10, 0);
    lv_obj_center(st_l);

    lv_obj_t * bm_btn = lv_btn_create(scr);
    lv_obj_set_size(bm_btn, 62, 26);
    lv_obj_align(bm_btn, LV_ALIGN_BOTTOM_LEFT, 8, -48);
    lv_obj_set_style_radius(bm_btn, 6, 0);
    lv_obj_add_event_cb(bm_btn, bench_nav_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * bm_l = lv_label_create(bm_btn);
    lv_label_set_text(bm_l, "BENCH");
    lv_obj_set_style_text_font(bm_l, &lv_font_montserrat_10, 0);
    lv_obj_center(bm_l);

    lv_timer_create(key_poll_cb, 50, NULL);

    /* 初始档: LCD_Init 里是 80%, 与开关初始态(关)对齐为低档更直观 */
    LCD_Set_Light(30);
}

lv_obj_t * App_UI_GetHomeScr(void)
{
    return s_home_scr;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
/* 周期读当前 indev 指针状态刷坐标行 */
static void touch_poll_cb(lv_timer_t *t)
{
    (void)t;
    lv_point_t p;

    if (indev_touchpad == NULL) return;

    lv_indev_get_point(indev_touchpad, &p);
    if (s_pad_down) {
        lv_label_set_text_fmt(s_touch_lbl, "Touch: %d,%d  DOWN", (int)p.x, (int)p.y);
    } else {
        lv_label_set_text_fmt(s_touch_lbl, "Touch: %d,%d", (int)p.x, (int)p.y);
    }
}

/* 按压区事件 → 同步 DOWN 标志 */
static void pad_press_cb(lv_event_t *e)
{
    switch (lv_event_get_code(e)) {
        case LV_EVENT_PRESSED:          s_pad_down = 1; break;
        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST:
        default:                        s_pad_down = 0; break;
    }
}

/* 在独立空屏上启动官方 demo (它们都直接画在 lv_scr_act() 上, 空屏隔离好退出) */
static void run_official_demo(void (*demo_fn)(void))
{
    if (s_test_scr != NULL) return;           /* 已在 demo 中 */
    s_test_scr = lv_obj_create(NULL);
    lv_scr_load(s_test_scr);
    demo_fn();
}

static void stress_nav_cb(lv_event_t *e)
{
    (void)e;
#if LV_USE_DEMO_STRESS
    run_official_demo(lv_demo_stress);
#endif
}

static void bench_nav_cb(lv_event_t *e)
{
    (void)e;
#if LV_USE_DEMO_BENCHMARK
    run_official_demo(lv_demo_benchmark);
#endif
}

/* KEY1(PA0, 按下=低电平) 短按: 从任意页面切回主页并释放 demo 屏 */
static void key_poll_cb(lv_timer_t *t)
{
    static uint8_t prev_level = 1;
    uint8_t level;

    (void)t;
    level = (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET) ? 1 : 0;

    if (prev_level == 1 && level == 0 && lv_scr_act() != s_home_scr) {
        lv_scr_load(s_home_scr);
        if (s_test_scr != NULL) {             /* 官方 demo 的临时屏, 退出即销毁 */
            lv_obj_del(s_test_scr);
            s_test_scr = NULL;
        }
    }
    prev_level = level;
}

/* 开关注合 → 100% 亮, 断开 → 30% (验证 app→driver 反向控制链路) */
static void backlight_switch_cb(lv_event_t *e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    LCD_Set_Light(lv_obj_has_state(sw, LV_STATE_CHECKED) ? 100 : 30);
}
