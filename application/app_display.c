/**
  ******************************************************************************
  * @file    app_display.c
  * @brief   应用层-显示服务实现 (详见 app_display.h 的分层说明)
  *          结构: 参考 lvgl_zet6 的 lvgl_demo 骨架, 带 LVGL 互斥锁的正式形态。
  *          内核版本: Middlewares/FreeRTOS 为 OV_Watch 同款 V10.3.1 自洽整套。
  ******************************************************************************
***/
#include "app_display.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lvgl.h"
#include "lcd.h"
#include "lv_port_disp_template.h"    /* → interface/lcd.c  (ST7789, SPI1+背光PWM) */
#include "lv_port_indev_template.h"   /* → interface/touch.c (CST816, I2C1)       */

/*********************
 *  任务与锁配置
 *********************/
#define APP_LVGL_TASK_PRIO    3
#define APP_LVGL_TASK_STK     1024    /* 字 = 4KB (实测足够跑主页+官方demo刷新) */

static SemaphoreHandle_t s_lvgl_mux;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void app_lvgl_task(void *pvParameters);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void App_Display_Init(void)
{
    /* 触摸为轮询语义, EXTI9_5 唤醒扩展暂不使用, 关掉避免误触发 */
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);

    lv_init();

    if (xTaskCreate(app_lvgl_task, "lvgl", APP_LVGL_TASK_STK, NULL,
                    APP_LVGL_TASK_PRIO, NULL) != pdPASS) {
        Error_Handler();        /* FreeRTOS 堆不足 */
    }

    vTaskStartScheduler();      /* 正常永不返回 */
    for (;;) { }                /* 调度器异常返回时死守 */
}

void App_Display_Lock(void)
{
    if (s_lvgl_mux != NULL)
        xSemaphoreTake(s_lvgl_mux, portMAX_DELAY);
}

void App_Display_Unlock(void)
{
    if (s_lvgl_mux != NULL)
        xSemaphoreGive(s_lvgl_mux);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void app_lvgl_task(void *pvParameters)
{
    (void)pvParameters;

    /* mutex 在调度器运行后创建: 此时临界区计数才真正归零工作
     * (CM4F port 的 uxCriticalNesting 以 0xaaaaaaaa 哨兵初值, 调度器前
     * 的 enter/exit 不解除 BASEPRI, 属设计行为, 见 SVC 启动时统一清零) */
    s_lvgl_mux = xSemaphoreCreateMutex();

    /* 硬件拉起 + LVGL 注册 + 建树, 全程跑在本任务栈上 */
    lv_port_disp_init();        /* → LCD_Init: ST7789 序列+刷黑+背光 80% */
    lv_port_indev_init();       /* → Touch_Init: CST816 复位+配置 (轮询) */
    App_UI_Create();            /* 主页 */

    for (;;) {
        App_Display_Lock();
        lv_timer_handler();
        App_Display_Unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/**********************
 *  栈溢出安全网 (configCHECK_FOR_STACK_OVERFLOW=1)
 *  触发说明: 某一任务栈越界, 停死便于调试器定位, 避免静默破坏。
 *********************/
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    __disable_irq();
    for (;;) { }
}
