/******************************************************************************
 * 文件名: key.h
 * 描  述: 按键驱动模块头文件
 * 说  明: 提供按键的两种驱动接口：
 *         1. GPIO查询方式 — key_ctl_led()，适用于非实时场景。
 *         2. EXTI中断方式 — KEY_EXTI_Init() + 中断服务函数，
 *            适用于需要快速响应的场景。
 *         硬件连接：KEY0 → PE4，KEY1 → PE3 (上拉输入，按下低电平)。
 *                  LED0 → PB5，LED1 → PE5 (推挽输出，低电平点亮)。
 ******************************************************************************/
#ifndef _KEY_H_
#define _KEY_H_

#include <stm32f10x.h>
#include <key.h>

/**
 * @brief  LED小灯状态标志。
 *         外部可访问，用于查询当前LED亮灭状态。
 *         0 — 关闭；1 — 点亮。
 */
extern volatile u8 led_flag;

/**
 * @brief  初始化按键GPIO。
 *         将 PE3 (KEY1) 和 PE4 (KEY0) 配置为上拉输入模式。
 */
void key_init(void);

/**
 * @brief  按键查询控制LED。
 *         轮询检测 KEY0 按下状态，翻转 LED0 的亮灭。
 *         含软件去抖和等待释放逻辑。
 */
void key_ctl_led(void);

/**
 * @brief  按键EXTI中断初始化。
 *         完成GPIO→EXTI映射、EXTI下降沿触发配置、NVIC优先级配置。
 *         初始化后 KEY0 触发 EXTI4_IRQHandler，KEY1 触发 EXTI3_IRQHandler。
 */
void KEY_EXTI_Init(void);

#endif
