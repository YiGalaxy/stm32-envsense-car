/******************************************************************************
 * 文件名:		motor.c
 * 描  述:		电机驱动模块实现文件
 * 说  明:		基于TIM4定时器的两路PWM输出，通过L298N驱动芯片控制左右两个直流电机。
 *				左轮电机由PB12/PB13控制方向、TIM4_CH1(PB6)控制速度；
 *				右轮电机由PB14/PB15控制方向、TIM4_CH2(PB7)控制速度。
 *				支持差速转向，并提供左右轮独立速度微调功能。
 ******************************************************************************/
#include "motor.h"

/**
 * @brief  左轮速度微调值（可正可负）
 * @note   正值增加左轮速度，负值减少左轮速度。
 *		   用于修正因电机/轮胎差异导致的行驶方向偏斜。
 *		   范围：-PWM_MAX ~ +PWM_MAX
 */
int16_t left_trim = 0;

/**
 * @brief  右轮速度微调值（可正可负）
 * @note   正值增加右轮速度，负值减少右轮速度。
 *		   与 left_trim 配合使用以校正直线行驶。
 *		   范围：-PWM_MAX ~ +PWM_MAX
 */
int16_t right_trim = 0;

/**
 * @brief  电机方向控制GPIO初始化
 * @note   配置PB12~PB15为推挽输出，用于控制左右电机的正反转方向。
 *		   PB12/PB13 -> 左轮电机方向控制（IN1/IN2）
 *		   PB14/PB15 -> 右轮电机方向控制（IN3/IN4）
 *		   初始化后所有引脚输出低电平（电机停止状态）。
 */
void Motor_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能GPIOB外设时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* PB12/PB13控制左轮方向，PB14/PB15控制右轮方向 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		/* 推挽输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 初始状态：所有方向控制引脚输出低电平，电机制动 */
    GPIO_ResetBits(GPIOB,
                   GPIO_Pin_12 |
                   GPIO_Pin_13 |
                   GPIO_Pin_14 |
                   GPIO_Pin_15);
}

/**
 * @brief  电机PWM初始化（静态内部函数）
 * @note   配置TIM4产生两路PWM信号：
 *		   - TIM4_CH1 (PB6) -> 左轮电机速度
 *		   - TIM4_CH2 (PB7) -> 右轮电机速度
 *		   定时器时钟源为72MHz，经72分频后计数频率为1MHz，
 *		   自动重装载值为1000，故PWM周期 = 1000 / 1MHz = 1ms（频率1kHz）。
 *		   占空比分辨率 = 1 / 1000 = 0.1%。
 */
static void Motor_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_Base;
    TIM_OCInitTypeDef TIM_OC;		/* 定时器输出比较初始化结构体 */

    /* 使能GPIOB时钟（PWM输出引脚）和AFIO时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO, ENABLE);

    /* 使能TIM4定时器时钟（位于APB1总线） */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    /* 配置PB6(PWM左)、PB7(PWM右)为复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;		/* 复用推挽，由TIM4控制引脚状态 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 配置TIM4时基参数 */
    /* 预分频72-1：72MHz / 72 = 1MHz，即每计数一次耗时1us */
    TIM_Base.TIM_Prescaler = 72 - 1;
    /* 自动重装载1000-1：PWM周期 = 1000 * 1us = 1ms，即频率1kHz */
    TIM_Base.TIM_Period = 1000 - 1;
    TIM_Base.TIM_CounterMode = TIM_CounterMode_Up;		/* 向上计数模式 */
    TIM_Base.TIM_ClockDivision = TIM_CKD_DIV1;			/* 无外部时钟分频 */
    TIM_TimeBaseInit(TIM4, &TIM_Base);

    /* 配置两路PWM输出模式（通道1和2共用配置） */
    TIM_OC.TIM_OCMode = TIM_OCMode_PWM1;				/* PWM1：CNT<CCR时输出有效电平 */
    TIM_OC.TIM_OutputState = TIM_OutputState_Enable;	/* 使能PWM输出 */
    TIM_OC.TIM_OCPolarity = TIM_OCPolarity_High;		/* 输出极性高电平有效 */
    TIM_OC.TIM_Pulse = 0;								/* 初始CCR=0，占空比0% */

    TIM_OC1Init(TIM4, &TIM_OC);							/* 初始化通道1（PB6，左轮） */
    TIM_OC2Init(TIM4, &TIM_OC);							/* 初始化通道2（PB7，右轮） */

    /* 使能预装载寄存器，更新事件时同步更新CCR，避免周期内突变 */
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);					/* 使能ARR预装载 */

    TIM_Cmd(TIM4, ENABLE);								/* 启动TIM4计数 */
}

/**
 * @brief  电机模块初始化
 * @note   依次初始化GPIO方向控制和PWM定时器。
 *		   必须在调用任何电机运动函数前调用此函数。
 */
void Motor_Init(void)
{
    Motor_GPIO_Init();		/* 初始化方向控制引脚 */
    Motor_PWM_Init();		/* 初始化PWM输出通道 */
}

/**
 * @brief  设置左轮PWM占空比（静态内部函数）
 * @param  pwm PWM比较值，范围0~PWM_MAX(999)
 * @note   内部自动钳位至PWM_MAX上限。
 *		   值越大电机转速越快。对应TIM4_CH1(PB6)输出。
 */
static void SetLeftPWM(uint16_t pwm)
{
    if (pwm > PWM_MAX)
        pwm = PWM_MAX;

    TIM_SetCompare1(TIM4, pwm);	/* 更新TIM4通道1比较值，改变PWM占空比 */
}

/**
 * @brief  设置右轮PWM占空比（静态内部函数）
 * @param  pwm PWM比较值，范围0~PWM_MAX(999)
 * @note   内部自动钳位至PWM_MAX上限。
 *		   值越大电机转速越快。对应TIM4_CH2(PB7)输出。
 */
static void SetRightPWM(uint16_t pwm)
{
    if (pwm > PWM_MAX)
        pwm = PWM_MAX;

    TIM_SetCompare2(TIM4, pwm);	/* 更新TIM4通道2比较值，改变PWM占空比 */
}

/**
 * @brief  设置左右轮转速和方向
 * @param  left  左轮目标速度，范围 -PWM_MAX ~ +PWM_MAX
 *				 正值=前进，负值=后退，0=停止
 * @param  right 右轮目标速度，范围 -PWM_MAX ~ +PWM_MAX
 *				 正值=前进，负值=后退，0=停止
 * @note   方向控制逻辑：
 *		   左轮：PB12=0/PB13=1 -> 前进, PB12=1/PB13=0 -> 后退
 *		   右轮：PB14=0/PB15=1 -> 前进, PB14=1/PB15=0 -> 后退
 *		   通过左右轮差速实现转向。
 */
void Motor_SetSpeed(int16_t left, int16_t right)
{
    /*
     * 物理接线说明：
     *   PB12/PB13(方向) + PB6(PWM) → 实际控制物理右轮
     *   PB14/PB15(方向) + PB7(PWM) → 实际控制物理左轮
     * 因此将 left 参数映射到物理左轮的GPIO，right 映射到物理右轮的GPIO。
     */

    /* ---- 左轮 (物理左轮：PB14/PB15方向 + PB7 PWM) ---- */
    if (left >= 0)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_14);
        GPIO_SetBits(GPIOB, GPIO_Pin_15);
        SetRightPWM(left);
    }
    else
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_14);
        GPIO_ResetBits(GPIOB, GPIO_Pin_15);
        SetRightPWM(-left);
    }

    /* ---- 右轮 (物理右轮：PB12/PB13方向 + PB6 PWM) ---- */
    if (right >= 0)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_12);
        GPIO_SetBits(GPIOB, GPIO_Pin_13);
        SetLeftPWM(right);
    }
    else
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_12);
        GPIO_ResetBits(GPIOB, GPIO_Pin_13);
        SetLeftPWM(-right);
    }
}

/**
 * @brief  紧急停止电机
 * @note   左右轮PWM占空比设为0，电机停止输出。
 *		   注：此为惯性停止，如需快速制动可短接电机两端。
 */
void Motor_Stop(void)
{
    SetLeftPWM(0);				/* 左轮PWM归零 */
    SetRightPWM(0);				/* 右轮PWM归零 */
}

/**
 * @brief  直行前进
 * @param  speed 基础速度值，范围 0 ~ PWM_MAX(999)
 * @note   实际输出速度 = speed + 对应轮 trim 微调值。
 *		   自动钳位至 [0, PWM_MAX] 范围。
 *		   trim值用于修正电机个体差异导致的直线跑偏。
 */
void Motor_Forward(uint16_t speed)
{
    int16_t l = (int16_t)speed + left_trim;		/* 左轮速度加微调 */
    int16_t r = (int16_t)speed + right_trim;	/* 右轮速度加微调 */

    /* 钳位至合法范围 */
    if (l < 0) l = 0;  if (l > PWM_MAX) l = PWM_MAX;
    if (r < 0) r = 0;  if (r > PWM_MAX) r = PWM_MAX;

    Motor_SetSpeed(l, r);		/* 两轮同时前进 */
}

/**
 * @brief  直线后退
 * @param  speed 后退速度值，范围 0 ~ PWM_MAX(999)
 * @note   左右轮同速反转，不应用trim微调值。
 */
void Motor_Backward(uint16_t speed)
{
    Motor_SetSpeed(-(int16_t)speed, -(int16_t)speed);
}

/**
 * @brief  原地左转（差速转向）
 * @param  speed 转向速度值，范围 0 ~ PWM_MAX(999)
 * @note   左轮反转 + 右轮正转 = 绕中心原地左转（逆时针）。
 *		   速度越大转向角速度越大。
 */
void Motor_Left(uint16_t speed)
{
    Motor_SetSpeed((int16_t)speed, -(int16_t)speed);	/* 左进右退 */
}

/**
 * @brief  原地右转（差速转向）
 * @param  speed 转向速度值，范围 0 ~ PWM_MAX(999)
 * @note   左轮正转 + 右轮反转 = 绕中心原地右转（顺时针）。
 *		   速度越大转向角速度越大。
 */
void Motor_Right(uint16_t speed)
{
    Motor_SetSpeed(-(int16_t)speed, (int16_t)speed);	/* 左退右进 */
}
