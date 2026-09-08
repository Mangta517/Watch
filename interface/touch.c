/**
  ******************************************************************************
  * @file    touch.c
  * @brief   CST816 电容触摸驱动 (硬件 I2C1) — 移植自 FryPi OV_Watch
  *          BSP/TOUCH/CST816.c (GPL-3.0), 改动:
  *          ① 原软件模拟 IIC(iic_hal.c, PB6/PB7 GPIO) 改为硬件 I2C1 的
  *            HAL_I2C_Mem_Read/Write (PB6/PB7 已是 I2C1 AF);
  *          ② RST=PB8 / INT=PB9 引脚宏复用 CubeMX main.h (TP_RST_x 与 TP_INT_x);
  *          ③ INT 已有 EXTI 下降沿 + g_tp_int_flag(gpio.c), 本驱动按轮询语义
  *            实现(与 OV_Watch 的 lv_port_indev 相同), 低功耗唤醒可再用该标志。
  *          依赖: hi2c1 (i2c.c), MX_GPIO_Init 已把 PB8 配成推挽输出、PB9 EXTI 使能。
 *          低功耗: 空闲5s芯片自动降低扫描功耗; 触摸时芯片自醒+PB9发下降沿,
 *            Touch_GetPoint 消费 g_tp_int_flag 做`重读→复位`两级唤醒。
  ******************************************************************************
***/
#include "touch.h"
#include "i2c.h"

extern volatile uint8_t g_tp_int_flag;   /* PB9 EXTI 下降沿置1 (gpio.c 定义) */

/*--- CST816 寄存器 (同 OV_Watch CST816.h) ---*/
#define CST816_I2C_ADDR     (0x15 << 1)   /* HAL 用 8bit 地址格式 */
#define REG_GestureID       0x01
#define REG_FingerNum       0x02
#define REG_XposH           0x03            /* 0x03~0x06: XH XL YH YL */
#define REG_ChipID          0xA7
#define REG_SleepMode       0xE5
#define REG_AutoSleepTime   0xF9

/* 触摸面板与 LCD 装配差 180°(同 OV_Watch REVERSE=1): 坐标需镜像 */
#define TOUCH_REVERSE       1
#define TOUCH_SCREEN_W      240
#define TOUCH_SCREEN_H      280

#define TP_RST_0()  HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_RESET)
#define TP_RST_1()  HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_SET)

volatile uint8_t g_tp_chip_id = 0;   /* 诊断: 最近一次读到的 CST816 ChipID (0xB5/0xB7=总线正常) */

#define TP_I2C_TMO  50U    /* I2C 单笔超时(ms), 总线异常时不会永久卡死 */

/******************************************************************************
 * I2C 读写原语
 *****************************************************************************/
static uint8_t tp_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return (HAL_I2C_Mem_Read(&hi2c1, CST816_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, buf, len, TP_I2C_TMO) == HAL_OK) ? 1 : 0;
}

static void tp_write_reg(uint8_t reg, uint8_t val)
{
    HAL_I2C_Mem_Write(&hi2c1, CST816_I2C_ADDR, reg,
                      I2C_MEMADD_SIZE_8BIT, &val, 1, TP_I2C_TMO);
}

/******************************************************************************
 * 复位: 拉低10ms, 释放后等100ms 固件就绪 (同 OV_Watch CST816_RESET)
 *****************************************************************************/
void Touch_Reset(void)
{
    TP_RST_0();
    HAL_Delay(10);
    TP_RST_1();
    HAL_Delay(100);
}

uint8_t Touch_GetChipID(void)
{
    uint8_t id = 0;
    (void)tp_read_regs(REG_ChipID, &id, 1);
    g_tp_chip_id = id;    /* 总线活没活, 调试器读这个: 0xB5/0xB7=通, 0=死 */
    return id;    /* CST816S≈0xB5, CST816D≈0xB7 */
}

uint8_t Touch_Init(void)
{
    uint8_t id;
    Touch_Reset();
    id = Touch_GetChipID();
    tp_write_reg(REG_AutoSleepTime, 5);     /* 5s 无触摸进低功耗 (同 OV) */
    return id;
}

/******************************************************************************
 * 读一个触点坐标 (含低功耗唤醒协调)
 * 返回: 手指数量(0=无触摸/总线失败, 一般 1); x/y 出参为 LCD 对齐坐标
 *
 * 芯片 5s 空闲自动进低功耗扫描: 此时 I2C 不应答(NACK)或读回 0xFF。
 * 手指落下 → 芯片自醒 + PB9 下降沿置 g_tp_int_flag → 本函数两级唤醒:
 *   一级: 见 INT 提示即重读 (多数情况芯片已自醒, 免复位);
 *   二级: 重读仍无应答 → 复位唤醒 (Touch_Wakeup, ~110ms, 一次性代价)。
 *****************************************************************************/
uint8_t Touch_GetPoint(uint16_t *x, uint16_t *y)
{
    uint8_t num, d[4];
    uint16_t px, py;

    if (!tp_read_regs(REG_FingerNum, &num, 1)) {
        if (!g_tp_int_flag) return 0;                 /* 睡眠中且无人碰: 快速退出 */
        if (!tp_read_regs(REG_FingerNum, &num, 1)) {  /* 一级: 芯片自醒后重读 */
            Touch_Wakeup();                           /* 二级: 复位唤醒+重配置 */
            if (!tp_read_regs(REG_FingerNum, &num, 1)) return 0;
        }
    }
    if (num == 0xFF) return 0;                        /* 芯片仍在睡眠扫描 */
    if (g_tp_int_flag) g_tp_int_flag = 0;             /* 消费唤醒提示(含抬起残留沿) */
    if (num == 0x00) return 0;                        /* 无手指 */

    if (!tp_read_regs(REG_XposH, d, 4)) return 0;       /* XposH..YposL 连读 */
    px = (uint16_t)(((uint16_t)(d[0] & 0x0F) << 8) | d[1]);
    py = (uint16_t)(((uint16_t)(d[2] & 0x0F) << 8) | d[3]);

#if TOUCH_REVERSE
    px = TOUCH_SCREEN_W - 1 - px;
    py = TOUCH_SCREEN_H - 1 - py;
#endif
    *x = px;
    *y = py;
    return num;
}

void Touch_Sleep(void)
{
    tp_write_reg(REG_SleepMode, 0x03);      /* 无触摸唤醒功能的睡眠 (同 OV) */
}

void Touch_Wakeup(void)
{
    Touch_Reset();                          /* OV 的唤醒就是复位 */
    tp_write_reg(REG_AutoSleepTime, 5);     /* 复位后配置丢失, 重配自动休眠 */
}
