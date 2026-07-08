/******************************************************************************
 * 文件名:		led.c
 * 描  述:		LED驱动模块实现文件
 * 说  明:		控制板载两个LED指示灯（PB5和PE5），低电平点亮。
 *				用于系统运行状态指示、调试信号等场景。
 ******************************************************************************/
#include <led.h>
#include <stm32f10x.h>

/**
 * @brief  LED GPIO初始化
 * @note   配置PB5和PE5为推挽输出，初始状态为高电平（LED熄灭）。
 *		   PB5 -> 板载LED1（通常为绿色）
 *		   PE5 -> 板载LED2（通常为红色）
 *		   低电平点亮，高电平熄灭。
 */
void led_init(void)
{
    /* 使能GPIOB和GPIOE外设时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOE, ENABLE);

    /* GPIO初始化结构体配置 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		/* 推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;				/* 引脚PB5/PE5 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);					/* 初始化PB5 */
    GPIO_Init(GPIOE, &GPIO_InitStructure);					/* 初始化PE5 */

    /* 初始状态：输出高电平，LED熄灭（低电平点亮） */
    GPIO_SetBits(GPIOB, GPIO_Pin_5);
    GPIO_SetBits(GPIOE, GPIO_Pin_5);
}
