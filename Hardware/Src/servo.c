/******************************************************************************
 * 文件名:		servo.c
 * 描  述:		舵机控制模块实现文件
 * 说  明:		通过TIM2_CH1(PA0)输出50HzPWM信号控制标准舵机角度。
 *				定时器72MHz时钟经72分频后为1MHz，ARR=20000，PWM周期20ms(50Hz)。
 *				舵机角度与脉宽关系：0度=0.5ms(CCR=500)，180度=2.5ms(CCR=2500)。
 ******************************************************************************/
#include "servo.h"

/**
 * @brief  角度值转PWM比较值（静态内部函数）
 * @param  angle 目标角度（度），范围 0~180
 * @return uint16_t 对应TIM2_CH1的比较值（CCR）
 * @note   舵机脉宽线性映射：0度 -> 500(0.5ms)，180度 -> 2500(2.5ms)
 *		   中间值线性插值：CCR = 500 + angle * 2000 / 180
 */
static uint16_t Servo_AngleToCCR(float angle);

/**
 * @brief  舵机初始化
 * @note   配置TIM2_CH1(PA0)输出50Hz标准舵机PWM信号。
 *		   初始化后舵机停留于中位（1500对应90度）。
 *		   时序参数：
 *		   - PWM频率：50Hz（周期20ms），标准舵机要求
 *		   - 计数频率：1MHz（72MHz / 72），精度1us
 *		   - 脉宽范围：0.5ms~2.5ms，对应0度~180度
 */
void Servo_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    TIM_TimeBaseInitTypeDef TIM_TimeBase;
    TIM_OCInitTypeDef TIM_OC;

    /* 使能GPIOA时钟（PA0为PWM输出引脚）和AFIO时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_AFIO, ENABLE);

    /* 使能TIM2定时器时钟（位于APB1总线） */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* 配置PA0为复用推挽输出，连接TIM2_CH1 */
    /* PA0 -> TIM2_CH1 (舵机PWM信号) */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;		/* 复用推挽，由TIM2控制 */
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 配置TIM2时基参数 */
    /* 预分频72-1：72MHz / 72 = 1MHz，每计数一次=1us */
    TIM_TimeBase.TIM_Prescaler = 72 - 1;
    /* 自动重装载20000-1：PWM周期 = 20000 * 1us = 20ms（50Hz，标准舵机频率） */
    TIM_TimeBase.TIM_Period = 20000 - 1;
    TIM_TimeBase.TIM_CounterMode = TIM_CounterMode_Up;	/* 向上计数 */
    TIM_TimeBase.TIM_ClockDivision = TIM_CKD_DIV1;		/* 无时钟分割 */
    TIM_TimeBase.TIM_RepetitionCounter = 0;				/* 不使用重复计数 */
    TIM_TimeBaseInit(TIM2, &TIM_TimeBase);

    /* 配置PWM输出模式 */
    TIM_OC.TIM_OCMode = TIM_OCMode_PWM1;				/* PWM1模式 */
    TIM_OC.TIM_OutputState = TIM_OutputState_Enable;	/* 使能输出 */
    TIM_OC.TIM_OCPolarity = TIM_OCPolarity_High;		/* 高电平有效 */
    TIM_OC.TIM_Pulse = 1500;							/* 初始脉宽1500us=1.5ms（中位90度） */
    TIM_OC1Init(TIM2, &TIM_OC);							/* 初始化通道1 */

    /* 使能预装载寄存器 */
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);	/* 使能OC1预装载 */
    TIM_ARRPreloadConfig(TIM2, ENABLE);					/* 使能ARR预装载 */

    TIM_Cmd(TIM2, ENABLE);								/* 启动TIM2计数 */
}

/**
 * @brief  角度值转PWM比较值（静态内部函数）
 * @param  angle 目标角度（度）
 * @return uint16_t 对应的TIM2_CH1比较值(CCR)
 * @note   舵机标准脉宽映射：
 *		   0度   -> 0.5ms  -> CCR = 500
 *		   90度  -> 1.5ms  -> CCR = 1500（中位）
 *		   180度 -> 2.5ms  -> CCR = 2500
 *		   使用线性插值：CCR = 500 + angle * (2000/180)
 *		   输入自动钳位至 0~180 度范围。
 */
static uint16_t Servo_AngleToCCR(float angle)
{
    /* 角度钳位：舵机机械限位0~180度 */
    if (angle < 0)
        angle = 0;
    if (angle > 180)
        angle = 180;

    /*
     * 线性映射：脉宽范围 500us(0度) ~ 2500us(180度)
     * 总跨度2000us，对应180度，每度 = 2000/180 us
     * 返回值 = 500 + angle * (2000/180)
     */
    return (uint16_t)(500 + angle * 2000.0f / 180.0f);
}

/**
 * @brief  设置舵机角度
 * @param  angle 目标角度（度），范围 0~180
 * @note   内部调用Servo_AngleToCCR将角度转为CCR值，
 *		   写入TIM2_CH1比较寄存器以改变PWM脉宽。
 *		   舵机响应需一定时间，连续快速调用需加入适当延时。
 */
void Servo_SetAngle(float angle)
{
    /* 更新TIM2通道1比较值，改变PWM脉宽控制舵机角度 */
    TIM_SetCompare1(TIM2, Servo_AngleToCCR(angle));
}
