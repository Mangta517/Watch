/**
  ******************************************************************************
  * @file    app_ui.h
  * @brief   应用层-界面: 集成"外设驱动+显示+触摸"三者的首屏演示页
  *          验证点: ①LVGL 渲染刷屏(显示功能) ②实时触摸坐标/按压状态(触摸功能)
  *                  ③开关注合背光档位(interface 驱动被 UI 反向控制)
  ******************************************************************************
***/
#ifndef __APP_UI_H
#define __APP_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* 在 lv_port_disp/indev 初始化完成后调用; 内部只建对象不起任务 */
void App_UI_Create(void);

/* 主页 screen 句柄 (demo 页返回导航用) */
lv_obj_t * App_UI_GetHomeScr(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_H */
