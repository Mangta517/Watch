/**
 ****************************************************************************************************
 * @file        lvgl_demo.c
 * @version     V1.0
 * @brief       LVGL v8 + FreeRTOS demo entry for STM32F411RET6 Watch
 *              (task boilerplate based on the ALIENTEK LVGL-V8 sample, restructured)
 * @license     MIT
 ****************************************************************************************************
 */
 
#include "lvgl_demo.h"
#include "FreeRTOS.h"
#include "task.h"

#include "lvgl.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
/* lv_demo_stress.h removed: this LVGL copy has no demos dir */


/******************************************************************************************************/
/*FreeRTOS配置*/

/* START_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define START_TASK_PRIO     1           /* 任务优先级 */
#define START_STK_SIZE      128         /* 任务堆栈大小 */
TaskHandle_t StartTask_Handler;         /* 任务句柄 */
void start_task(void *pvParameters);    /* 任务函数 */

/* LV_DEMO_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define LV_DEMO_TASK_PRIO   3           /* 任务优先级 */
#define LV_DEMO_STK_SIZE    1024        /* 任务堆栈大小 */
TaskHandle_t LV_DEMOTask_Handler;       /* 任务句柄 */
void lv_demo_task(void *pvParameters);  /* 任务函数 */

/******************************************************************************************************/


void lvgl_demo(void)
{
    lv_init();                                          /* lvgl系统初始化 */
    //lv_port_disp_init();                                /* lvgl显示接口初始化,放在lv_init()的后面 */
    //lv_port_indev_init();                               /* lvgl输入接口初始化,放在lv_init()的后面 */

    xTaskCreate((TaskFunction_t )start_task,            /* 任务函数 */
                (const char*    )"start_task",          /* 任务名称 */
                (uint16_t       )START_STK_SIZE,        /* 任务堆栈大小 */
                (void*          )NULL,                  /* 传递给任务函数的参数 */
                (UBaseType_t    )START_TASK_PRIO,       /* 任务优先级 */
                (TaskHandle_t*  )&StartTask_Handler);   /* 任务句柄 */

    vTaskStartScheduler();                              /* 开启任务调度 */
}

/**
 * @brief       start_task
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void start_task(void *pvParameters)
{
    pvParameters = pvParameters;
    
    taskENTER_CRITICAL();           /* 进入临界区 */

    /* 创建LVGL任务 */
    xTaskCreate((TaskFunction_t )lv_demo_task,
                (const char*    )"lv_demo_task",
                (uint16_t       )LV_DEMO_STK_SIZE, 
                (void*          )NULL,
                (UBaseType_t    )LV_DEMO_TASK_PRIO,
                (TaskHandle_t*  )&LV_DEMOTask_Handler);

    taskEXIT_CRITICAL();            /* 退出临界区 */
    vTaskDelete(StartTask_Handler); /* 删除开始任务 */
}

/**
 * @brief       LVGL运行例程
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void lv_demo_task(void *pvParameters)
{
    pvParameters = pvParameters;
    
    //lv_demo_stress();  /* 测试的demo */
    //创建一个开关
   /*  lv_obj_t* switch_obj = lv_switch_create(lv_scr_act());
    lv_obj_set_size(switch_obj, 120, 60);
    lv_obj_align(switch_obj, LV_ALIGN_CENTER, 0, 0); */

    //设置一个本地按压变色的样式,居中对齐,并设置边框和轮廓
    /* lv_obj_t *obj1=lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(obj1, lv_color_hex(0xE06356), LV_STATE_PRESSED);
    lv_obj_set_align(obj1, LV_ALIGN_CENTER);

    lv_obj_set_style_border_color(obj1, lv_color_hex(0x4FC245), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj1, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(obj1,150,LV_STATE_DEFAULT);

    lv_obj_set_style_outline_color(obj1, lv_color_hex(0x4F66A3), LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(obj1, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(obj1,150,LV_STATE_DEFAULT); */

    //设置一个滑块
    /* lv_obj_t *slider=lv_slider_create(lv_scr_act());
    lv_obj_set_align(slider, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xE06356), LV_STATE_DEFAULT | LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x4FC245), LV_STATE_DEFAULT | LV_PART_KNOB);
 */

 /* lv_mainstart(): TODO add your UI file back */  /* LVGL演示 */
    
    while(1)
    {
        lv_timer_handler(); /* LVGL计时器 */
        vTaskDelay(5);
    }
}


