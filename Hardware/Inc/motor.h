/******************************************************************************
 * 文件名:		motor.h
 * 描  述:		电机驱动模块头文件
 * 说  明:		声明电机控制相关函数和全局变量。
 *				PWM频率1kHz，占空比分辨率0.1%。
 *				左轮：TIM4_CH1(PB6)，右轮：TIM4_CH2(PB7)。
 *				方向控制：左轮IN1/IN2(PB12/PB13)，右轮IN3/IN4(PB14/PB15)。
 ******************************************************************************/
#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

/**
 * @def PWM_MAX
 * @brief PWM比较值上限（对应100%占空比）
 * @note  对应定时器ARR值 1000-1，CCR范围 0~999
 */
#define PWM_MAX     999

/**
 * @brief  电机模块初始化
 * @note   初始化电机GPIO方向控制和PWM定时器
 */
void Motor_Init(void);

/**
 * @brief  电机方向控制GPIO初始化
 * @note   单独初始化方向引脚，供调试或重新配置时使用
 */
void Motor_GPIO_Init(void);

/**
 * @brief  设置左右轮转速和方向
 * @param  left  左轮速度(-PWM_MAX~+PWM_MAX)，正=前进，负=后退
 * @param  right 右轮速度(-PWM_MAX~+PWM_MAX)，正=前进，负=后退
 * @note   速度符号决定方向，绝对值决定PWM占空比
 */
void Motor_SetSpeed(int16_t left, int16_t right);

/**
 * @brief  紧急停止电机
 * @note   两轮PWM置0，惯性停止
 */
void Motor_Stop(void);

/**
 * @brief  直行前进（应用trim微调）
 * @param  speed 基础速度 0~PWM_MAX
 */
void Motor_Forward(uint16_t speed);

/**
 * @brief  直线后退
 * @param  speed 后退速度 0~PWM_MAX
 */
void Motor_Backward(uint16_t speed);

/**
 * @brief  原地左转（差速）
 * @param  speed 转向速度 0~PWM_MAX
 */
void Motor_Left(uint16_t speed);

/**
 * @brief  原地右转（差速）
 * @param  speed 转向速度 0~PWM_MAX
 */
void Motor_Right(uint16_t speed);

/** @brief 左轮速度微调值，外部可访问 */
extern int16_t left_trim;
/** @brief 右轮速度微调值，外部可访问 */
extern int16_t right_trim;

#endif
