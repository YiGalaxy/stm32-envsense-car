/********************************************************
 * 文件: snake.h
 * 描述: 贪吃蛇游戏模块头文件
 *       16列x8行网格, 蓝牙方向控制, 最大蛇长128
 ********************************************************/
#ifndef __SNAKE_H
#define __SNAKE_H
#include "sys.h"

/* 方向宏定义 --------------------------------------------------------------*/
#define SNAKE_UP    0   // 上
#define SNAKE_DOWN  1   // 下
#define SNAKE_LEFT  2   // 左
#define SNAKE_RIGHT 3   // 右

/* 函数声明 ----------------------------------------------------------------*/

/**
 * @brief  进入贪吃蛇游戏
 * @note   设置 act=1, 调用 init() 初始化蛇、分数、食物
 */
void Snake_Enter(void);

/**
 * @brief  退出贪吃蛇游戏
 * @note   设置 act=0, 清屏并刷新 OLED
 */
void Snake_Exit(void);

/**
 * @brief  贪吃蛇主任务 (每帧调用一次)
 * @note   仅当 act=1 时执行: tick 递增, 每4帧触发一次 tick_fn(), 然后 draw()
 */
void Snake_Task(void);

/**
 * @brief  获取当前分数
 * @return uint16_t 当前 score 值
 */
uint16_t Snake_GetScore(void);

/**
 * @brief  清零分数
 */
void Snake_ClearScore(void);

/**
 * @brief  设置蛇的下一步方向 (被蓝牙/按键回调调用)
 * @param  dir : 方向值 (SNAKE_UP/DOWN/LEFT/RIGHT)
 * @note   dir>3 直接返回; slen>1 时禁止掉头 (rev[dir]==sdir 则忽略);
 *         仅写入 ndir, 在 tick_fn() 中才同步到 sdir
 */
void Snake_SetDir(u8 dir);

/**
 * @brief  查询游戏是否激活
 * @return u8 : 1=游戏中, 0=未激活
 */
u8  Snake_IsActive(void);

#endif
