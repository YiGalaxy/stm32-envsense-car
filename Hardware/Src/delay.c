/******************************************************************************
 * 文件名: delay.c
 * 描  述: 软件延时函数模块实现文件
 * 说  明: 基于SysTick系统定时器实现微秒级和毫秒级精确延时。
 *         系统时钟配置为72MHz，SysTick时钟源为HCLK/8 = 9MHz。
 *         通过设置SysTick重装载寄存器实现精确计数延时。
 *         提供两个核心接口：delay_us() 和 delay_ms()。
 ******************************************************************************/
#include "delay.h"

/**
 * @brief  微秒延时倍乘数。
 *         SysTick时钟频率 = 72MHz / 8 = 9MHz，
 *         即 1us = 9 个计数周期。
 *         fac_us = SystemCoreClock / 8000000 = 9。
 */
static uint8_t fac_us = 0;

/**
 * @brief  毫秒延时倍乘数。
 *         fac_ms = fac_us * 1000 = 9000，
 *         即 1ms = 9000 个计数周期。
 */
static uint16_t fac_ms = 0;

/**
 * @brief  初始化SysTick定时器，计算延时倍乘系数。
 *         配置SysTick时钟源为 HCLK/8 (AHB总线时钟8分频)。
 *         SYSCLK = 72MHz，HCLK = 72MHz，
 *         SysTick时钟 = 72MHz / 8 = 9MHz = 9,000,000 Hz，
 *         因此 1us 对应 9 个计数周期。
 *         @note 必须在调用 delay_us() 或 delay_ms() 之前调用此函数。
 */
void delay_init(void)
{
    /*
     * SysTick时钟源选择：HCLK/8
     * 72MHz / 8 = 9MHz，计数周期约 0.111us
     */
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);

    /*
     * fac_us = 72,000,000 / 8,000,000 = 9
     * 表示 1us 需要 9 个SysTick计数
     */
    fac_us = SystemCoreClock / 8000000;

    /*
     * fac_ms = 9 * 1000 = 9000
     * 表示 1ms 需要 9000 个SysTick计数
     */
    fac_ms = (uint16_t)fac_us * 1000;
}

/**
 * @brief  微秒级延时函数。
 *         通过SysTick定时器实现指定微秒数的精确延时。
 *         原理：将重装载寄存器 (LOAD) 设置为 nus * fac_us，
 *               启动计数器后等待 COUNTFLAG 置位。
 * @param  nus 需要延时的微秒数（最大值受限于24位计数器，约 1,864,135us）。
 * @note   SysTick计数器为24位递减计数器，最大计数值 2^24 - 1 = 16,777,215。
 *         当 fac_us = 9 时，最大延时约 1,864,135us ≈ 1.86s。
 *         超过此值需改用 delay_ms() 或多次调用。
 */
void delay_us(uint32_t nus)
{
    uint32_t temp;

    /*
     * 设置重装载值：
     *   从 nus*fac_us 递减到 0，耗时 nus 微秒
     */
    SysTick->LOAD = nus * fac_us;

    /* 清空当前计数值，确保从LOAD值开始递减 */
    SysTick->VAL = 0x00;

    /* 使能SysTick计数器（CTRL寄存器的ENABLE位写1） */
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

    /*
     * 等待计数完成：
     *   CTRRL[0] (ENABLE) 必须为 1
     *   CTRL[16] (COUNTFLAG) 为 1 表示计数到 0
     *   条件：(temp & 0x01) && !(temp & (1 << 16))
     *   即 ENABLE=1 且 COUNTFLAG=0 时继续等待
     */
    do
    {
        temp = SysTick->CTRL;
    }
    while((temp & 0x01) && !(temp & (1 << 16)));

    /* 关闭SysTick计数器，降低功耗 */
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;

    /* 清空计数器，为下次延时做准备 */
    SysTick->VAL = 0x00;
}

/**
 * @brief  毫秒级延时函数。
 *         通过循环调用 delay_us(1000) 实现毫秒延时。
 * @param  nms 需要延时的毫秒数（受 uint32_t 范围限制，最大约 4,294,967ms）。
 * @note   此实现方式在 nms 较大时精度不如直接配置SysTick，
 *         但实现简单且可避免24位计数器的范围限制。
 *         如需高精度长延时，可考虑直接操作SysTick的LOAD寄存器。
 */
void delay_ms(uint32_t nms)
{
    while(nms--)
    {
        delay_us(1000);     /* 累加1000次1us延时 = 1ms */
    }
}
