/**
  ******************************************************************************
  * @file    app_display.h
  * @brief   应用层-显示服务: 集成 interface 外设驱动(lcd/touch) + LVGL + FreeRTOS
  *          分层: application(本层) → Middlewares(lv_port 移植层/lvgl) → interface(驱动) → Core(HAL)
  *          main() 只需调用 App_Display_Init(), 不再直接触碰任何硬件。
  ******************************************************************************
***/
#ifndef __APP_DISPLAY_H
#define __APP_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_ui.h"

/**
 * @brief 显示系统总入口 (永不返回):
 *        lv_init → 创建 LVGL 任务(任务内完成 显示/触摸移植层初始化 与 UI 构建)
 *        → vTaskStartScheduler
 */
void App_Display_Init(void);

/* LVGL 非线程安全: 其他 FreeRTOS 任务调用任何 lv_* API 前必须包锁 */
void App_Display_Lock(void);
void App_Display_Unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DISPLAY_H */
