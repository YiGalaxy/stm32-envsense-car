/******************************************************************************
 * 文件名: usart.h
 * 描  述: 串口通信模块头文件
 * 说  明: 声明USART1/USART2的初始化与收发接口
 ******************************************************************************/
#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"
#include <string.h>
#include <stdio.h>

void USART1_Init(uint32_t baud);
void USART1_SendByte(uint8_t ch);
void USART1_SendString(char *str);

void USART2_Init(uint32_t baudrate);
void USART2_SendChar(uint8_t ch);
void USART2_SendString(char *str);


void parse_cmd(void);

/* USART3 (ESP8266) */
void USART3_Init(uint32_t baudrate);
void USART3_SendChar(uint8_t ch);
void USART3_SendString(char *str);
void usart3_send_bytes(uint8_t *buf, uint32_t len);
void usart3_send_str(char *str);

/* Mode definitions */
#define MODE_REMOTE  0
#define MODE_AUTO    1
#define MODE_FOLLOW  2
extern uint8_t work_mode;
extern uint8_t avoid_dist;

/* Command buffer (from usart.c) */
extern u8 uart_buf[200];
extern u16 uart_cnt;
extern u8 uart_flag;

#endif
