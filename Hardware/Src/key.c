/******************************************************************************
 * 文件名: key.c
 * 描  述: 按键驱动模块实现文件
 * 说  明: 本文件实现了两种按键处理方式：
 *         1. GPIO查询方式 (key_ctl_led) — 轮询检测按键电平，带软件去抖。
 *         2. EXTI中断方式 (KEY_EXTI_Init) — 通过外部中断下降沿触发，
 *            响应更快，适合需要实时响应的场景。
 * 硬  件: KEY0 — PE4 (上拉输入，按下为低电平)
 *         KEY1 — PE3 (上拉输入，按下为低电平)
 *         LED0 — PB5 (推挽输出，低电平点亮)
 *         LED1 — PE5 (推挽输出，低电平点亮)
 ******************************************************************************/
#include <key.h>
#include <delay.h>
#include <sys.h>

/**
 * @brief  LED小灯状态标志。
 *         记录当前小灯亮灭状态，0=关闭，1=点亮。
 *         由按键查询控制函数 key_ctl_led() 维护。
 */
volatile u8 led_flag = 0;

/**
 * @brief  初始化按键GPIO。
 *         PE3 (KEY1) 和 PE4 (KEY0) 配置为上拉输入模式。
 *         按键未按下时，引脚被内部上拉电阻拉高至3.3V；
 *         按下时，引脚接地变为低电平。
 *         同时开启AFIO时钟，为EXTI中断模式做准备。
 */
void key_init(void)
{
    /* 开启GPIOE时钟和AFIO复用功能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE |
                           RCC_APB2Periph_AFIO,
                           ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    /*
     * PE3 (KEY1) 和 PE4 (KEY0) 配置：
     *   模式: IPU (上拉输入) — 默认高电平，按下低电平
     *   速度: 50MHz (输入模式速度配置不影响功能)
     */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;          /* 上拉输入 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_4 | GPIO_Pin_3; /* KEY0 | KEY1 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(GPIOE, &GPIO_InitStructure);
}

/**
 * @brief  按键查询方式控制LED。
 *         轮询检测 KEY0 (PE4) 是否被按下，若按下则翻转 LED0 (PB5) 状态。
 *         带软件去抖：首次检测到按下后延时1ms再次确认。
 *         按下等待释放：使用 while 循环等待按键释放，避免重复触发。
 * @note   此为阻塞式查询，在循环中调用时注意实时性。
 *         PEin(4) 是 sys.h 中定义的宏，展开为 PAin(4) 的寄存器操作，
 *         但实际使用的是 PE4，需确认宏实现是否从 GPIOE 读取。
 */
void key_ctl_led(void)
{
    /*
     * 检测 KEY0 (PE4) 是否被按下：
     *   上拉输入，按下时引脚为低电平 (==0)
     */
    if(PEin(4) == 0)
    {
        delay_ms(1);                        /* 软件去抖，避开按键机械抖动时间 */

        if(PEin(4) == 0)                    /* 再次确认按键确实被按下 */
        {
            PBout(5) = !PBout(5);           /* 翻转 LED0 (PB5) 的亮灭状态 */
        }

        /*
         * 等待按键释放：
         *   当按键未释放时 (PE4=0)，循环等待不退出，
         *   防止一次按下多次触发翻转
         */
        while(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == 0)
        {
            /* 空循环等待 */
        }
    }
}

/**
 * @brief  EXTI中断的GPIO映射配置。
 *         将PE3和PE4的GPIO端口映射到EXTI线路3和4。
 *         EXTI3 ← PE3 (KEY1)
 *         EXTI4 ← PE4 (KEY0)
 * @note   必须在配置EXTI之前完成GPIO映射。
 *         每个EXTI线路只能映射到一个GPIO引脚。
 */
static void KEY_EXTI_GPIO_Config(void)
{
    /*
     * EXTI3 连接到 PE3 (KEY1)
     * 参数: GPIO端口 = GPIOE, 引脚源 = PinSource3
     */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOE,
                        GPIO_PinSource3);

    /*
     * EXTI4 连接到 PE4 (KEY0)
     * 参数: GPIO端口 = GPIOE, 引脚源 = PinSource4
     */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOE,
                        GPIO_PinSource4);
}

/**
 * @brief  配置EXTI中断类型。
 *         设置为中断模式，下降沿触发。
 *         按键按下时 PE3/PE4 从高电平变为低电平（下降沿），触发中断。
 */
static void KEY_EXTI_Config(void)
{
    EXTI_InitTypeDef EXTI_InitStructure;

    /*
     * ---- EXTI3 配置 (KEY1) ----
     * 中断模式: EXTI_Mode_Interrupt
     * 触发方式: EXTI_Trigger_Falling (下降沿触发，对应按键按下动作)
     */
    EXTI_InitStructure.EXTI_Line = EXTI_Line3;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /*
     * ---- EXTI4 配置 (KEY0) ----
     * 与EXTI3使用相同的参数配置，只改变线路号
     */
    EXTI_InitStructure.EXTI_Line = EXTI_Line4;
    EXTI_Init(&EXTI_InitStructure);
}

/**
 * @brief  配置EXTI中断的NVIC优先级。
 *         EXTI3 (KEY1) : 抢占优先级=2, 子优先级=2
 *         EXTI4 (KEY0) : 抢占优先级=3, 子优先级=3
 *         优先级数值越小，优先级越高。
 *         KEY1的优先级高于KEY0。
 */
static void KEY_NVIC_Config(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /*
     * ---- EXTI3 中断通道 (KEY1) ----
     * 抢占优先级 = 2, 子优先级 = 2
     */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /*
     * ---- EXTI4 中断通道 (KEY0) ----
     * 抢占优先级 = 3, 子优先级 = 3 (优先级低于KEY1)
     */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief  按键中断综合初始化。
 *         按顺序完成以下4步配置：
 *           1. GPIO初始化 (上拉输入)
 *           2. GPIO→EXTI 映射
 *           3. EXTI中断类型配置 (下降沿触发)
 *           4. NVIC中断优先级配置
 *         调用后，KEY0 (PE4) 和 KEY1 (PE3) 的按下动作
 *         将分别触发 EXTI4_IRQHandler 和 EXTI3_IRQHandler。
 */
void KEY_EXTI_Init(void)
{
    key_init();                    /* 1. GPIO初始化：PE3/PE4上拉输入 */
    KEY_EXTI_GPIO_Config();       /* 2. GPIO映射到EXTI线路 */
    KEY_EXTI_Config();            /* 3. 配置EXTI中断模式与触发方式 */
    KEY_NVIC_Config();            /* 4. 配置NVIC中断优先级 */
}

/**
 * @brief  EXTI3中断服务函数 (对应 KEY1 = PE3)。
 *         按键按下时触发下降沿中断。
 *         当前中断处理逻辑：
 *           1. 关闭 LED0 (PB5=1) 和 LED1 (PE5=1) — 先全部熄灭
 *           2. 点亮 LED0 (PB5=0) — 指示 KEY1 被按下
 *           3. 延时1秒保持点亮
 *           4. 熄灭 LED0 (PB5=1)
 * @note   中断服务函数应尽量短，避免长时间阻塞。
 *         当前的 delay_ms(1000) 会阻塞系统1ms，
 *         在实际产品中建议使用定时器或状态机替代。
 */
void EXTI3_IRQHandler(void)
{
    /* 判断是否为EXTI3中断 */
    if(EXTI_GetITStatus(EXTI_Line3) != RESET)
    {
        /*
         * 中断处理：按键按下 → 点亮LED0并保持1秒
         * 先关闭所有LED，再单独点亮D0作为KEY1按下指示
         */
        PBout(5) = 1;           /* 关闭 LED0 (PB5高电平=灭) */
        PEout(5) = 1;           /* 关闭 LED1 (PE5高电平=灭) */

        PBout(5) = 0;           /* 点亮 LED0 (PB5低电平=亮) */
        delay_ms(1000);         /* 保持点亮1秒 */
        PBout(5) = 1;           /* 关闭 LED0 */

        /* 清除中断标志位，否则中断会反复触发 */
        EXTI_ClearITPendingBit(EXTI_Line3);
    }
}

/**
 * @brief  EXTI4中断服务函数 (对应 KEY0 = PE4)。
 *         按键按下时触发下降沿中断。
 *         当前中断处理逻辑：
 *           1. 关闭 LED0 (PB5=1) 和 LED1 (PE5=1) — 先全部熄灭
 *           2. 点亮 LED1 (PE5=0) — 指示 KEY0 被按下
 *           3. 延时1秒保持点亮
 *           4. 熄灭 LED1 (PE5=1)
 * @note   同上，实际应用中应避免在中断中执行长延时。
 */
void EXTI4_IRQHandler(void)
{
    /* 判断是否为EXTI4中断 */
    if(EXTI_GetITStatus(EXTI_Line4) != RESET)
    {
        /*
         * 中断处理：按键按下 → 点亮LED1并保持1秒
         * 先关闭所有LED，再单独点亮D1作为KEY0按下指示
         */
        PBout(5) = 1;           /* 关闭 LED0 */
        PEout(5) = 1;           /* 关闭 LED1 */

        PEout(5) = 0;           /* 点亮 LED1 (PE5低电平=亮) */
        delay_ms(1000);         /* 保持点亮1秒 */
        PEout(5) = 1;           /* 关闭 LED1 */

        /* 清除中断标志位 */
        EXTI_ClearITPendingBit(EXTI_Line4);
    }
}
