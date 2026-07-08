/******************************************************************************
 * 文件名:		beep.h
 * 描  述:		蜂鸣器驱动模块头文件
 * 说  明:		声明蜂鸣器初始化、响铃、音调播放及音乐旋律函数。
 *				使用PF0驱动有源蜂鸣器，支持任意频率方波输出。
 ******************************************************************************/
#ifndef _BEEP_H_
#define _BEEP_H_
#include <stm32f10x.h>

/**
 * @brief  蜂鸣器GPIO初始化
 * @note   配置PF0为推挽输出，初始关闭
 */
void beep_init(void);

/**
 * @brief  蜂鸣器响指定时长（2kHz固定频率）
 * @param  ms 响铃时长（毫秒）
 */
void beep_on(uint16_t ms);

/**
 * @brief  播放指定频率的音调
 * @param  freq 频率(Hz)，0=休止
 * @param  ms   时长(毫秒)
 */
void beep_tone(uint16_t freq, uint16_t ms);

/**
 * @brief  播放洒水车音乐旋律
 * @note   使用beep_tone播放预定义旋律序列
 */
void beep_sprinkler(void);

#endif
