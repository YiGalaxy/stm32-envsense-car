/******************************************************************************
 * 文件名: sr04.c
 * 描  述: 超声波测距模块实现文件
 * 说  明: 本文件实现了HC-SR04超声波测距模块的底层驱动。
 *         通过TIM3输入捕获功能测量Echo引脚高电平脉宽，
 *         进而换算为距离值。Trig引脚输出10us高电平触发测距，
 *         Echo引脚接收回波信号，脉宽与距离成正比。
 * 硬  件: Trig — PA7 (推挽输出)
 *         Echo — PA6 (浮空输入)
 *         时钟: SYSCLK = 72MHz
 ******************************************************************************/
#include "sr04.h"
#include "delay.h"

/**
 * @brief  TIM3通道1捕获的上升沿计数值。
 *         在捕获中断中，首次上升沿到来时记录该值。
 */
volatile uint16_t CaptureValue1 = 0;

/**
 * @brief  TIM3通道1捕获的下降沿计数值。
 *         在捕获中断中，下降沿到来时记录该值。
 */
volatile uint16_t CaptureValue2 = 0;

/**
 * @brief  超声波Echo回波信号的高电平脉宽（TIM计数个数）。
 *         CaptureValue2与CaptureValue1的差值，
 *         若发生溢出则做环形补偿计算。
 */
volatile uint16_t PulseWidth = 0;

/**
 * @brief  捕获状态标志位。
 *         0 — 等待上升沿捕获（首次捕获）；
 *         1 — 已捕获上升沿，等待下降沿捕获（第二次捕获）。
 */
volatile uint8_t CaptureFlag = 0;

/**
 * @brief  一次完整的测距完成标志位。
 *         由TIM3捕获中断置1，主循环查询该位判断测距是否结束。
 */
volatile uint8_t SR04_Finish = 0;

/**
 * @brief  最近一次测距的距离计算结果，单位cm。
 *         -1 表示测量失败（超时或脉宽异常）。
 */
volatile float Distance = -1;

/**
 * @brief  初始化TIM3定时器，配置通道1为输入捕获模式。
 *         时钟源：APB1 (72MHz)，预分频72-1 → 计数频率1MHz (1us计数1次)。
 *         捕获滤波器设置为8，用于硬件去噪。
 *         开启CC1捕获中断（NVIC抢占优先级1，子优先级1）。
 */
static void TIM3_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_Base;       /* 定时器时基结构体 */
    TIM_ICInitTypeDef TIM_IC;               /* 输入捕获结构体 */
    NVIC_InitTypeDef NVIC_InitStructure;    /* 中断优先级结构体 */

    /* 开启TIM3时钟（APB1总线） */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /*
     * 预分频器 = 72-1，72MHz / 72 = 1MHz，即每us计数1次
     * 自动重装载 = 65535，最大计数值对应约65.535ms
     */
    TIM_Base.TIM_Prescaler = 72 - 1;
    TIM_Base.TIM_Period = 65535;
    TIM_Base.TIM_CounterMode = TIM_CounterMode_Up;      /* 向上计数模式 */
    TIM_Base.TIM_ClockDivision = TIM_CKD_DIV1;          /* 时钟不分频 */
    TIM_TimeBaseInit(TIM3, &TIM_Base);

    /* 使用默认值初始化输入捕获结构体 */
    TIM_ICStructInit(&TIM_IC);

    /*
     * 配置TIM3通道1 (CH1) 输入捕获参数：
     *   通道    : TIM_Channel_1 (对应PA6复用功能)
     *   极性    : 上升沿捕获（首次捕获上升沿）
     *   输入选择: 直接输入到TI1
     *   预分频  : 不分频（每个边沿都捕获）
     *   滤波器  : 8个采样点，用于抑制毛刺
     */
    TIM_IC.TIM_Channel = TIM_Channel_1;
    TIM_IC.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_IC.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_IC.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_IC.TIM_ICFilter = 8;
    TIM_ICInit(TIM3, &TIM_IC);

    /* 使能TIM3通道1捕获比较中断 */
    TIM_ITConfig(TIM3, TIM_IT_CC1, ENABLE);

    /*
     * 配置NVIC中断优先级：
     *   抢占优先级 = 1
     *   子优先级   = 1
     */
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 启动TIM3计数器 */
    TIM_Cmd(TIM3, ENABLE);
}

/**
 * @brief  TIM3全局中断服务函数。
 *         处理通道1 (CH1) 的输入捕获事件。
 *         捕获机制：
 *           首次捕获（CaptureFlag==0）：记录上升沿计数值 CaptureValue1，
 *            然后将捕获极性切换为下降沿。
 *           第二次捕获（CaptureFlag==1）：记录下降沿计数值 CaptureValue2，
 *            计算脉宽 PulseWidth，置位 SR04_Finish，
 *            将捕获极性恢复为上升沿，等待下一轮触发。
 *         @note 若计数值溢出（CaptureValue2 < CaptureValue1），
 *               通过 65535 - CaptureValue1 + CaptureValue2 做环形补偿。
 */
void TIM3_IRQHandler(void)
{
    /* 判断是否为通道1捕获中断 */
    if(TIM_GetITStatus(TIM3, TIM_IT_CC1) != RESET)
    {
        /* 清除中断挂起位 */
        TIM_ClearITPendingBit(TIM3, TIM_IT_CC1);

        if(CaptureFlag == 0)
        {
            /* ---- 第1次捕获：上升沿到达 ---- */
            CaptureValue1 = TIM_GetCapture1(TIM3);      /* 记录上升沿计数值 */
            CaptureFlag = 1;                            /* 标记已捕获上升沿 */
            TIM_OC1PolarityConfig(TIM3, TIM_ICPolarity_Falling); /* 切换为下降沿捕获 */
        }
        else
        {
            /* ---- 第2次捕获：下降沿到达 ---- */
            CaptureValue2 = TIM_GetCapture1(TIM3);      /* 记录下降沿计数值 */

            /* 计算高电平脉宽（考虑计数器溢出情况） */
            if(CaptureValue2 >= CaptureValue1)
            {
                /* 正常情况：下降沿计数值 >= 上升沿计数值 */
                PulseWidth = CaptureValue2 - CaptureValue1;
            }
            else
            {
                /* 溢出情况：计数器经过65535后回零 */
                PulseWidth = 65535 - CaptureValue1 + CaptureValue2;
            }

            SR04_Finish = 1;        /* 标记本次测量完成 */
            CaptureFlag = 0;        /* 重置捕获标志，准备下一轮 */
            TIM_OC1PolarityConfig(TIM3, TIM_ICPolarity_Rising); /* 恢复上升沿捕获 */
        }
    }
}

/**
 * @brief  触发超声波模块发送测距脉冲。
 *         向Trig引脚输出一个10us的高电平脉冲，
 *         HC-SR04收到后会自动发送8个40kHz的超声波脉冲，
 *         并等待回波，在Echo引脚输出与距离成正比的高电平脉宽。
 * @note   触发前需将CaptureFlag和SR04_Finish清零，
 *         以便中断服务函数正确捕获本次测距结果。
 */
void SR04_StartMeasure(void)
{
    CaptureFlag = 0;                    /* 复位捕获状态机 */
    SR04_Finish = 0;                    /* 清除完成标志 */

    GPIO_ResetBits(GPIOA, GPIO_Pin_7);  /* Trig引脚先拉低，确保起始电平稳定 */
    delay_us(2);                        /* 保持低电平2us */

    GPIO_SetBits(GPIOA, GPIO_Pin_7);    /* Trig引脚拉高，开始发送触发脉冲 */
    delay_us(10);                       /* 高电平保持10us (HC-SR04要求>10us) */

    GPIO_ResetBits(GPIOA, GPIO_Pin_7);  /* 拉低Trig引脚，脉冲结束 */
}

/**
 * @brief  初始化超声波模块的GPIO和TIM3。
 *         GPIOA Pin7 (Trig) 配置为推挽输出，初始输出低电平。
 *         GPIOA Pin6 (Echo) 配置为浮空输入（复用为TIM3_CH1输入捕获）。
 *         调用TIM3_Init()完成定时器输入捕获配置。
 */
void SR04_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 开启GPIOA时钟和AFIO复用功能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_AFIO,
                           ENABLE);

    /*
     * ---- Trig引脚 (PA7) 配置 ----
     * 模式: 推挽输出 (Out_PP)
     * 速度: 50MHz
     * 初始电平: 低电平
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_7);  /* Trig默认输出低电平 */

    /*
     * ---- Echo引脚 (PA6) 配置 ----
     * 模式: 浮空输入 (IN_FLOATING)，复用为TIM3_CH1
     * 注: PA6的TIM3_CH1复用功能由硬件自动重映射，无需额外配置
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 初始化TIM3输入捕获 */
    TIM3_Init();
}

/**
 * @brief  执行一次完整的超声波测距。
 *         流程：
 *           1. 调用 SR04_StartMeasure() 触发测距脉冲。
 *           2. 等待 TIM3 捕获中断完成（轮询 SR04_Finish 标志）。
 *           3. 若超时 (约40ms) 则返回 -1。
 *           4. 若脉宽 > 38000 计数 (对应约 38ms，即距离 > 6.5m) 视为无效。
 *           5. 有效则通过 PulseWidth / 58.0 换算为厘米。
 * @return float 距离值，单位厘米 (cm)。
 *         -1 表示测距失败（超时或超出量程）。
 * @note  距离换算公式：D(cm) = PulseWidth(us) / 58.0
 *        依据：声速340m/s，回波一来回，1us对应约0.034cm，
 *        但 HC-SR04 实测经验公式为 脉宽(us)/58 = 距离(cm)。
 */
float SR04_GetDistance(void)
{
    uint16_t timeout = 0;               /* 超时计数器 */

    SR04_StartMeasure();                /* 触发超声波发送脉冲 */

    /* 轮询等待捕获完成，每次约10us */
    while(SR04_Finish == 0)
    {
        delay_us(10);                   /* 每10us检查一次完成标志 */
        timeout++;

        if(timeout > 4000)              /* 超时约 4000*10us = 40ms */
        {
            Distance = -1;              /* 标记超时 */
            return Distance;
        }
    }

    /*
     * 脉宽 > 38000 对应约 38ms，
     * 根据 HC-SR04 规格，最大量程约 4m (脉宽约 23200us)，
     * 此处放宽阈值用于异常过滤
     */
    if(PulseWidth > 38000)
    {
        Distance = -1;                  /* 超出有效范围 */
    }
    else
    {
        /*
         * 换算公式：距离(cm) = 脉宽(us) / 58
         * 推导：声速340m/s = 0.034cm/us，回波一来回距离折半
         *       实际 cm = (us * 0.034) / 2 = us / 58.82
         *       HC-SR04 数据手册推荐 us/58
         */
        Distance = PulseWidth / 58.0f;
    }

    return Distance;
}
