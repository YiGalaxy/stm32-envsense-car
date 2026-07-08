/******************************************************************************
 * 文件名: dht11.h
 * 描  述: DHT11温湿度传感器驱动头文件
 * 说  明: 提供DHT11单总线通信协议的完整接口声明。
 *         硬件连接：DATA引脚 → PA1（双向GPIO）。
 *         通信速率约20Kbps，每次通信周期约5ms。
 ******************************************************************************/
#ifndef __DHT11_H__
#define __DHT11_H__

#include "stm32f10x.h"

/* 外部全局变量：WiFi模块发送缓冲区，用于组装温湿度上报数据 */
extern char txbuf[64];

/**
 * @brief  DHT11数据引脚初始化为浮空输入模式。
 *         在等待DHT11响应或读取数据时调用。
 */
void DHT_Init_InPut(void);

/**
 * @brief  DHT11数据引脚初始化为推挽输出模式。
 *         在主机发送起始信号时调用。
 */
void DHT_Init_OutPut(void);

/**
 * @brief  发送DHT11起始信号。
 *         主机拉低总线 > 18ms 再拉高 20~40us，唤醒DHT11。
 */
void DHT_STart(void);

/**
 * @brief  读取DHT11数据引脚的当前电平。
 * @return uint16_t 0=低电平，1=高电平。
 */
uint16_t DHT_Scan(void);

/**
 * @brief  读取DHT11的一位数据。
 *         DHT11数据位编码：'0'=低50us+高26us，'1'=低50us+高70us。
 *         通过延时40us后采样判断。
 * @return uint16_t 0或1，超时返回0。
 */
uint16_t DHT_ReadBit(void);

/**
 * @brief  读取DHT11的一个字节（8位）。
 *         MSB先行，逐位拼装。
 * @return uint16_t 读取到的字节值。
 */
uint16_t DHT_ReadByte(void);

/**
 * @brief  读取DHT11完整温湿度数据。
 *         返回5字节数据：湿度整/小、温度整/小、校验和。
 * @param  buffer[5] 输出缓冲区。
 * @return uint16_t 0=成功，1=校验失败，2=超时。
 */
uint16_t DHT_ReadData(uint8_t buffer[5]);

/**
 * @brief  （旧接口）读取DHT11温湿度。
 *         由 DHT_ReadData() 替代，保留兼容。
 * @param  temp 输出温度
 * @param  humi 输出湿度
 * @return u8   0=成功，1=失败
 */
u8 DHT11_Readdata(u8 *temp, u8 *humi);

#endif
