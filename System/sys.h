/******************************************************************************
 * 文件名: sys.h
 * 描  述: 系统配置头文件
 * 说  明: 定义位带操作宏实现 GPIO 位操作（类似51单片机），以及GPIO地址映射
 *       位带操作原理：将1个bit映射到1个word地址，通过读写该地址实现单bit控制
 *       详细参考 <<CM3权威指南>> 第五章 87-92页
 ******************************************************************************/
#ifndef __SYS_H
#define __SYS_H	
#include "stm32f10x.h" 

//0,不支持ucos
//1,支持ucos
#define SYSTEM_SUPPORT_UCOS		0		//系统文件夹是否支持UCOS(0:不支持 1:支持)
																	    
	 
//位带操作,实现51类似的GPIO控制功能
//具体实现思想,参考<<CM3权威指南>>第五章(87页~92页).
//IO口操作宏定义
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2))  //将addr的bitnum位映射到别名区的字地址
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr))  //将addr作为unsigned long指针取值（读写寄存器值）
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum))  //读取addr的bitnum位别名区地址处的值（0或1）
//IO口地址映射
//ODR(Output Data Register)偏移=12，用于GPIO输出操作
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) //GPIOA输出数据寄存器地址 0x4001080C
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //GPIOB输出数据寄存器地址 0x40010C0C
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) //GPIOC输出数据寄存器地址 0x4001100C
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) //GPIOD输出数据寄存器地址 0x4001140C
#define GPIOE_ODR_Addr    (GPIOE_BASE+12) //GPIOE输出数据寄存器地址 0x4001180C
#define GPIOF_ODR_Addr    (GPIOF_BASE+12) //GPIOF输出数据寄存器地址 0x40011A0C
#define GPIOG_ODR_Addr    (GPIOG_BASE+12) //GPIOG输出数据寄存器地址 0x40011E0C

//IDR(Input Data Register)偏移=8，用于GPIO输入操作
#define GPIOA_IDR_Addr    (GPIOA_BASE+8)  //GPIOA输入数据寄存器地址 0x40010808
#define GPIOB_IDR_Addr    (GPIOB_BASE+8)  //GPIOB输入数据寄存器地址 0x40010C08
#define GPIOC_IDR_Addr    (GPIOC_BASE+8)  //GPIOC输入数据寄存器地址 0x40011008
#define GPIOD_IDR_Addr    (GPIOD_BASE+8)  //GPIOD输入数据寄存器地址 0x40011408
#define GPIOE_IDR_Addr    (GPIOE_BASE+8)  //GPIOE输入数据寄存器地址 0x40011808
#define GPIOF_IDR_Addr    (GPIOF_BASE+8)  //GPIOF输入数据寄存器地址 0x40011A08
#define GPIOG_IDR_Addr    (GPIOG_BASE+8)  //GPIOG输入数据寄存器地址 0x40011E08
 
//IO口位带操作宏,只对单一的IO口!
//确保n的值小于16(PFx只到F脚,PGx只到G脚)
//用法示例: PAout(0)=1;  //PA0输出高电平
//        PBout(5)=0;  //PB5输出低电平
//        val = PAin(3); //读取PA3电平
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //PAx引脚输出 (x=0~15)
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //PAx引脚输入读取

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //PBx引脚输出 (x=0~15)
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //PBx引脚输入读取

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //PCx引脚输出 (x=0~15)
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //PCx引脚输入读取

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //PDx引脚输出 (x=0~15)
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //PDx引脚输入读取

#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  //PEx引脚输出 (x=0~15)
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  //PEx引脚输入读取

#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  //PFx引脚输出 (x=0~15, 仅部分型号有)
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  //PFx引脚输入读取

#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  //PGx引脚输出 (x=0~15, 仅部分型号有)
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  //PGx引脚输入读取



/* 配置NVIC中断优先级分组（Group2: 2位抢占 + 2位响应） */
void NVIC_Configuration(void);



#endif
