/******************************************************************************
 * 文件名:		beep.c
 * 描  述:		蜂鸣器驱动模块实现文件
 * 说  明:		通过PF0 GPIO输出方波驱动有源蜂鸣器。
 *				提供beep_on（简易2kHz方波响铃）、
 *				beep_tone（指定频率音调播放）和
 *				beep_sprinkler（洒水车音乐旋律）三种驱动方式。
 ******************************************************************************/
#include <beep.h>
#include <sys.h>
#include <delay.h>

/**
 * @brief  蜂鸣器GPIO初始化
 * @note   配置PF0为推挽输出，初始输出低电平（蜂鸣器不响）。
 *		   PF0连接有源蜂鸣器（高电平驱动），通过方波产生声音。
 */
void beep_init(void)
{
    /* 使能GPIOF外设时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);

    /* 配置PF0为推挽输出 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		/* 推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;				/* PF0 -> 蜂鸣器控制 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOF, &GPIO_InitStructure);

    PFout(0) = 0;											/* 初始低电平，蜂鸣器关闭 */
}

/**
 * @brief  蜂鸣器响指定时长（简易模式）
 * @param  ms 响铃时长（毫秒）
 * @note   使用固定2kHz方波（周期500us，半周期250us）驱动蜂鸣器。
 *		   总周期数 = ms * 4 （每毫秒4个完整方波周期）。
 *		   响铃结束后确保输出低电平关闭蜂鸣器。
 *		   适用场景：按键反馈、简单报警提示。
 */
void beep_on(uint16_t ms)
{
    /* 2kHz方波：半周期250us，每毫秒4个完整周期 */
    uint32_t cycles = ms * 4;				/* 总方波周期数 = ms * (1000us / 500us) */

    for (uint32_t i = 0; i < cycles; i++)
    {
        PFout(0) = !PFout(0);				/* 翻转PF0电平，产生方波 */
        delay_us(250);						/* 半周期延时250us */
    }

    PFout(0) = 0;							/* 结束输出低电平，关闭蜂鸣器 */
}

/**
 * @brief  播放指定频率的音调
 * @param  freq 目标频率（Hz），0表示静音/休止
 * @param  ms   播放时长（毫秒）
 * @note   根据频率计算半周期微秒数：half_cycle = 500000 / freq。
 *		   总方波周期数 = (ms * 1000) / (half_cycle * 2)。
 *		   当freq=0时，仅延时ms毫秒（休止符效果）。
 *		   适用场景：播放旋律、音乐。
 */
void beep_tone(uint16_t freq, uint16_t ms)
{
    /* 频率为0表示休止，仅延时 */
    if (freq == 0)
    {
        delay_ms(ms);
        return;
    }

    /* 半周期微秒数 = 1,000,000us / (2 * freq) = 500,000 / freq */
    uint32_t half_cycle = 500000 / freq;
    /* 总周期数 = 总时间 / 周期时间 = (ms * 1000) / (half_cycle * 2) */
    uint32_t cycles = (uint32_t)ms * 1000 / (half_cycle * 2);

    for (uint32_t i = 0; i < cycles; i++)
    {
        PFout(0) = 1;						/* 输出高电平 */
        delay_us(half_cycle);				/* 保持半周期 */
        PFout(0) = 0;						/* 输出低电平 */
        delay_us(half_cycle);				/* 保持半周期 */
    }
}

/**
 * @brief  播放洒水车音乐旋律
 * @note   基于常见洒水车提示音旋律简化改编，使用C大调音阶。
 *		   旋律定义在音符-时长对数组中，自动循环播放一次。
 *		   音符频率使用C5~C6范围。
 *		   适用场景：设备启动提示音、状态提醒。
 */
void beep_sprinkler(void)
{
    /* 音符频率定义（C大调，音高范围C5~C6） */
    #define C5 523			/* do (中央C) */
    #define D5 587			/* re */
    #define E5 659			/* mi */
    #define F5 698			/* fa */
    #define G5 784			/* so */
    #define A5 880			/* la */
    #define B5 988			/* si */
    #define C6 1047			/* do (高八度) */

    /*
     * 旋律数据：按 {音符频率, 时长(ms)} 交替存储
     * 频率0表示休止符（不发音仅等待）
     * 旋律结构：前奏 + 主旋律 + 尾声
     */
    const uint16_t melody[] = {
        G5, 200, E5, 200, C5, 200, D5, 200,		/* 第一乐句 */
        E5, 200, C5, 300, 0, 100,					/* 第二乐句 + 休止 */
        G5, 200, E5, 200, C5, 200, D5, 200,		/* 重复第一乐句 */
        E5, 200, C5, 300, 0, 100,					/* 重复第二乐句 + 休止 */
        C5, 150, E5, 150, G5, 150, A5, 150,		/* 上行音阶过渡 */
        G5, 200, E5, 200, C5, 200, D5, 200,		/* 主旋律再现 */
        C5, 400, 0, 200,							/* 结束音 + 休止 */
    };

    /* 遍历旋律数组，步进2（每次取一个音符和其时长） */
    for (uint8_t i = 0; i < sizeof(melody) / sizeof(melody[0]); i += 2)
    {
        beep_tone(melody[i], melody[i + 1]);		/* 播放一个音符 */
    }
}
