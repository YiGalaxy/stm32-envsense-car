/******************************************************************************
 * @file    debug.h
 * @brief   蓝牙串口调试模块 - 头文件
 *          提供命令解析和周期调试输出的接口声明
 * @note    本模块处理查询类命令(help/status/sensors等)，
 *          控制类命令(goA/stop/menu等)由外部 motor 模块解析
 ******************************************************************************/
#ifndef __DEBUG_H
#define __DEBUG_H

#include "sys.h"

/**
 * @brief  处理调试命令(由 USART 接收中断或解析器调用)
 * @param  buf  输入的命令字符串(以 '\0' 结尾)
 * @return int8_t  0 表示命令已被本模块处理；
 *                 -1 表示未识别命令(由外部继续处理)
 * @note   同时处理 help/status/sensors/ping/uptime/reset/echo/debug on|off
 *         不处理 goA/stop/menu 等控制命令(外部 motor 模块处理)
 */
int8_t Debug_ProcessCmd(const char *buf);

/**
 * @brief  调试周期性任务(由 main loop 以 ~10Hz 频率调用)
 * @note   每调用一次递增 sys_tick 计数器；
 *         当 debug_on 使能时，每 ~1.5 秒(15 次调用)输出一次状态报告，
 *         包含温度、湿度、距离、速度、工作模式和 WiFi 连接状态
 */
void Debug_Task(void);

#endif /* __DEBUG_H */
