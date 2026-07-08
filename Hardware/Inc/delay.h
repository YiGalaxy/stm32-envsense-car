/******************************************************************************
 * 文件名: delay.h
 * 描  述: 延时函数模块头文件
 * 说  明: 提供基于SysTick定时器的微秒/毫秒级延时接口。
 *         使用前必须先调用 delay_init() 初始化SysTick时钟配置。
 *         系统时钟默认为72MHz，SysTick时钟 = 72MHz/8 = 9MHz。
 ******************************************************************************/
#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

/**
 * @brief  初始化SysTick定时器，计算延时倍乘系数。
 *         配置SysTick时钟源为 HCLK/8 (9MHz)，
 *         计算 fac_us 和 fac_ms 供延时函数使用。
 *         @note 必须在使用延时函数前调用一次。
 */
void delay_init(void);

/**
 * @brief  微秒级延时。
 *         通过SysTick递减计数器实现精确延时。
 * @param  nus 需要延时的微秒数。
 *         最大受限于24位计数器：约 1,864,135us (1.86s)。
 */
void delay_us(uint32_t nus);

/**
 * @brief  毫秒级延时。
 *         通过循环调用 delay_us(1000) 实现。
 * @param  nms 需要延时的毫秒数。
 */
void delay_ms(uint32_t nms);

#endif
