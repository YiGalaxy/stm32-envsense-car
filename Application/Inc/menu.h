#ifndef __MENU_H
#define __MENU_H

#include "sys.h"

/**
 * @def MENU_ITEMS
 * @brief 菜单项总数
 * @note  共6项，索引0~5：
 *         0 - 工作模式(REMOTE/AUTO/FOLLOW)
 *         1 - 左轮微调
 *         2 - 右轮微调
 *         3 - 基准速度
 *         4 - 贪吃蛇
 *         5 - WiFi配网
 */
#define MENU_ITEMS  6

/**
 * @brief  进入菜单模式
 * @param  无
 * @retval 无
 * @note   初始化菜单状态(光标归零、重置计时器)，绘制菜单界面
 */
void Menu_Enter(void);

/**
 * @brief  光标上移(UP按键响应)
 * @param  无
 * @retval 无
 * @note   cursor索引减1(不低于0)，重绘菜单
 */
void Menu_Up(void);

/**
 * @brief  光标下移(DOWN按键响应)
 * @param  无
 * @retval 无
 * @note   cursor索引加1(不超过MENU_ITEMS-1)，重绘菜单
 */
void Menu_Down(void);

/**
 * @brief  减小当前选中项的值或切换模式(LEFT按键响应)
 * @param  无
 * @retval 无
 * @note   项0切换工作模式，项1/2/3减小参数值，其他项无操作
 */
void Menu_Left(void);

/**
 * @brief  增大当前选中项的值或切换模式(RIGHT按键响应)
 * @param  无
 * @retval 无
 * @note   项0切换工作模式，项1/2/3增大参数值，其他项无操作
 */
void Menu_Right(void);

/**
 * @brief  确认/进入当前选中项(OK按键响应)
 * @param  无
 * @retval 无
 * @note   项4进入贪吃蛇，项5启动WiFi配网，其他项退出菜单
 */
void Menu_Ok(void);

/**
 * @brief  退出菜单模式
 * @param  无
 * @retval 无
 * @note   清除激活标志，清空OLED屏幕
 */
void Menu_Exit(void);

/**
 * @brief  菜单定时任务(需周期性调用)
 * @param  无
 * @retval 无
 * @note   处理光标闪烁和空闲超时自动退出
 */
void Menu_Task(void);

/**
 * @brief  查询菜单是否激活
 * @param  无
 * @retval 1=菜单激活中，0=菜单已关闭
 */
u8  Menu_IsActive(void);

#endif
