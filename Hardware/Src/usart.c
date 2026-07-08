/******************************************************************************
 * 文件名: usart.c
 * 描  述: 串口通信模块实现文件
 * 说  明: 包含USART1/USART2/USART3初始化、收发函数、命令解析(parse_cmd)
 ******************************************************************************/
#include "usart.h"
#include "debug.h"
#include <stdio.h>
#include <sys.h>
#include <beep.h>
#include <delay.h>
#include <motor.h>
#include <oled.h>
#include <ui.h>
#include <menu.h>
#include "esp8266.h"
#include "esp8266_mqtt.h"
#include "wificfg.h"
#include <snake.h>

volatile u16 speed=400;
uint8_t avoid_dist = 20;		// 避障探测距离阈值 (cm)

#define MODE_REMOTE  0
#define MODE_AUTO    1
uint8_t work_mode = MODE_REMOTE;	// 默认遥控模式

uint8_t USART2_RX_BUF[128]; // USART2 接收缓冲区
uint8_t USART2_RX_CNT = 0;

u8 uart_buf[200];			// 命令缓冲区
u16 uart_cnt = 0;
u8 uart_flag = 0;


int fputc(int ch, FILE *f)
{
    USART_SendData(USART1, (uint8_t)ch);

    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    return ch;
}

/*
    USART1

    TX ------ PA9
    RX ------ PA10
*/

void USART1_Init(uint32_t baud)
{
    GPIO_InitTypeDef GPIO_InitStructure;   // GPIO
    USART_InitTypeDef USART_InitStructure; // USART
    NVIC_InitTypeDef NVIC_InitStructure;   // NVIC 中断

    /*----------------------------------------------------
        1. 开启 GPIOA、USART1、AFIO 时钟

        GPIOA: PA9, PA10
        USART1: 外设 1
        AFIO: 复用功能
    -----------------------------------------------------*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_USART1 |
                           RCC_APB2Periph_AFIO,
                           ENABLE);

    /*----------------------------------------------------
        2. 配置 PA9(TX)

        推挽复用输出
    -----------------------------------------------------*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(GPIOA,&GPIO_InitStructure);

    /*----------------------------------------------------
        3. 配置 PA10(RX)

        浮空输入
    -----------------------------------------------------*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;

    GPIO_Init(GPIOA,&GPIO_InitStructure);

    /*----------------------------------------------------
        4. USART 参数配置
    -----------------------------------------------------*/

    USART_InitStructure.USART_BaudRate = baud; // 波特率

    // 8 位数据位
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;

    // 1 位停止位
    USART_InitStructure.USART_StopBits = USART_StopBits_1;

    // 无校验位
    USART_InitStructure.USART_Parity = USART_Parity_No;

    // 无硬件流控
    USART_InitStructure.USART_HardwareFlowControl =
            USART_HardwareFlowControl_None;

    // 收发模式
    USART_InitStructure.USART_Mode =
            USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART1,&USART_InitStructure);

    /*----------------------------------------------------
        5. 开启接收中断

        每收到一个字节触发一次 USART1_IRQHandler()
    -----------------------------------------------------*/
    USART_ITConfig(USART1,
                   USART_IT_RXNE,
                   ENABLE);			// 使能接收中断

    /*----------------------------------------------------
        6. NVIC 配置
    -----------------------------------------------------*/
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;

    // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;

    // 响应优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;

    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    /*----------------------------------------------------
        7. 使能 USART1
    -----------------------------------------------------*/
    USART_Cmd(USART1,ENABLE);
}

/*
 * 函数名称: USART2_Init
 * 功    能: USART2 初始化 PA2---TX, PA3---RX
 * 参    数: baudrate 波特率
 */
/******************************************************************************
 * 函数: USART2_Init
 * 描述: USART2初始化（蓝牙通信）
 * 参数: baudrate - 波特率（如115200）
 ******************************************************************************/
void USART2_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    // 2. 配置 TX PA2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 RX PA3
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. USART 参数设置
    USART_InitStructure.USART_BaudRate = baudrate;				// 波特率
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;	// 数据位
    USART_InitStructure.USART_StopBits = USART_StopBits_1;		// 停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;			// 校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	// 流控
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;					// 收发同时使能

    USART_Init(USART2, &USART_InitStructure);										// 初始化串口参数

    // 5. 使能接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    // 6. NVIC 配置
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;			// 中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    // 7. 使能串口
    USART_Cmd(USART2, ENABLE);
}


/*
    USART1 发送一个字节
*/
void USART1_SendByte(uint8_t ch)
{
    USART_SendData(USART1,ch);

    while(USART_GetFlagStatus(USART1,USART_FLAG_TXE) == RESET);
}

/*
 * 函数名称: USART2_SendChar
 * 功    能: 发送一个字符
 */
void USART2_SendChar(uint8_t ch)
{


	USART_SendData(USART2, ch);
	while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}





/*
    USART1 发送字符串
*/
void USART1_SendString(char *str)
{
    while(*str)
    {
        USART1_SendByte(*str++);
    }
}

/*
 * 函数名称: USART2_SendString
 * 功    能: 发送字符串
 */
/******************************************************************************
 * 函数: USART2_SendString
 * 描述: 通过USART2发送字符串
 * 参数: str - 待发送的字符串
 ******************************************************************************/
void USART2_SendString(char *str)
{
    while(*str)
    {
        USART2_SendChar(*str++);
    }
}





/*
    USART1 接收中断

    每收到 1 个字节触发一次
*/
void USART1_IRQHandler(void)
{
    uint8_t data;

    // 判断是否收到数据
    if(USART_GetITStatus(USART1,
                         USART_IT_RXNE) != RESET)
    {
        // 读取数据
        data = USART_ReceiveData(USART1);

        // 可以在这里写自己的处理函数
        // 回传数据
        USART1_SendByte(data);

        // 清除中断标志
        USART_ClearITPendingBit(USART1,
                                USART_IT_RXNE);
    }
}

// USART2 中断接收函数
/******************************************************************************
 * 函数: USART2_IRQHandler
 * 描述: USART2接收中断服务函数
 * 说明: 每收到一个字节存入uart_buf，收到*或缓冲区满时置位uart_flag
 ******************************************************************************/
void USART2_IRQHandler(void)
{
    uint8_t res;

    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        res = USART_ReceiveData(USART2);
		uart_buf[uart_cnt++] = res;		// 将接收到的数据存入缓冲区
        // 回传
        USART_SendData(USART2, res);


		if(uart_buf[uart_cnt-1]=='*'||uart_cnt>=sizeof(uart_buf)-1)
		{
			uart_flag = 1;											// 收到 * 就把处理标志位置 1
		}

        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

/******************************************************************************
 * 函数: parse_cmd
 * 描述: 命令解析与执行
 * 说明: 检查uart_flag，解析uart_buf中的命令字符串并执行对应操作
 *      支持命令: test/ledon/beepon/goA~goR/stop/speedup/remote/auto 等
 ******************************************************************************/
void parse_cmd(void)
{
    /*
     * 参考项目模式：MQTT解析和命令执行在同一块中。
     * uart_flag 或 g_esp8266_rx_end 任一置位即进入处理，
     * MQTT PUBLISH 优先解析，payload 或 uart_buf 统一做 strstr 匹配。
     *
     * 华为云命令响应延迟到 clear_buffers 之后发送，
     * 避免透传模式下 ESP8266 收发冲突导致响应包丢失。
     */
    static char  s_cmd_req_id[64];   /* 待响应的 request_id */
    static uint8_t s_cmd_pending;    /* 是否有待发送的响应 */

    if (uart_flag || g_esp8266_rx_end) {

        /* ---- 确保缓冲区以 '\0' 结尾 ---- */
        if (g_esp8266_rx_cnt < sizeof(g_esp8266_rx_buf))
            g_esp8266_rx_buf[g_esp8266_rx_cnt] = '\0';
        else
            g_esp8266_rx_buf[sizeof(g_esp8266_rx_buf) - 1] = '\0';

        if (uart_cnt < sizeof(uart_buf))
            uart_buf[uart_cnt] = '\0';
        else
            uart_buf[sizeof(uart_buf) - 1] = '\0';

        /* ---- 忽略 MQTT PINGRESP (心跳响应) ---- */
        if (g_esp8266_rx_cnt >= 2 &&
            g_esp8266_rx_buf[0] == 0xD0 && g_esp8266_rx_buf[1] == 0x00) {
            goto clear_buffers;
        }

        /* ================================================================
         * MQTT PUBLISH 优先解析 (内联 mqtt_get_remaining_length)
         * ================================================================ */
        if (g_esp8266_rx_cnt >= 2 && (g_esp8266_rx_buf[0] & 0xF0) == 0x30) {
            int remaining_len = 0;
            int remaining_len_bytes = 0;
            {
                int multiplier = 1;
                int idx = 1;
                uint8_t eb;
                do {
                    if (idx >= (int)g_esp8266_rx_cnt || idx > 4) {
                        remaining_len = -1;
                        break;
                    }
                    eb = g_esp8266_rx_buf[idx++];
                    remaining_len += (eb & 0x7F) * multiplier;
                    multiplier *= 128;
                    remaining_len_bytes = idx - 1;
                } while (eb & 0x80);
            }

            if (remaining_len >= 0) {
                int total_len = 1 + remaining_len_bytes + remaining_len;
                if ((uint32_t)total_len <= g_esp8266_rx_cnt) {
                    int pos = 1 + remaining_len_bytes;
                    int qos;

                    /* 读取 topic 长度（大端序2字节） */
                    if (pos + 2 <= total_len) {
                        int topic_len = (g_esp8266_rx_buf[pos] << 8)
                                      | g_esp8266_rx_buf[pos + 1];
                        pos += 2;

                        if (pos + topic_len <= total_len) {
                            char payload[256];
                            int payload_len;

                            /*
                             * 华为云命令下发：检查 topic 含 request_id=
                             * 不在此处发响应，先保存 request_id，
                             * 等命令执行完、缓冲区清理后再发 (避免透传冲突)
                             */
                            if (strstr((const char *)&g_esp8266_rx_buf[pos],
                                       "commands/request_id=")) {
                                const char *rid = strstr(
                                    (const char *)&g_esp8266_rx_buf[pos],
                                    "request_id=");
                                if (rid) {
                                    uint8_t ri;
                                    int max_rid_len;
                                    rid += 11;
                                    /* 限定提取范围: topic 剩余长度,
                                       防止读到 payload 中的 JSON 数据 */
                                    max_rid_len = (int)(topic_len
                                        - (rid - (const char *)
                                           &g_esp8266_rx_buf[pos]));
                                    ri = 0;
                                    while (ri < max_rid_len
                                           && rid[ri] && rid[ri] != '/'
                                           && ri < 63)
                                        { s_cmd_req_id[ri]=rid[ri]; ri++; }
                                    s_cmd_req_id[ri] = '\0';
                                    s_cmd_pending = 1;
                                }
                            }

                            /* 跳过 topic 内容 */
                            pos += topic_len;

                            /* 跳过 QoS 报文标识符 (QoS > 0) */
                            qos = (g_esp8266_rx_buf[0] >> 1) & 0x03;
                            if (qos > 0) {
                                if (pos + 2 > total_len)
                                    goto mqtt_skip;
                                pos += 2;
                            }

                            /* 提取 payload */
                            payload_len = total_len - pos;
                            if (payload_len > 0
                                && payload_len < (int)sizeof(payload)) {
                                memcpy(payload,
                                       (const void *)&g_esp8266_rx_buf[pos],
                                       payload_len);
                                payload[payload_len] = '\0';
                                USART2_SendString("[MQTT] ");
                                USART2_SendString(payload);
                                USART2_SendString("\r\n");

                                /* 将 payload 写入 uart_buf
                                   后续统一走 strstr 匹配 */
                                if (payload_len < 199) {
                                    memcpy(uart_buf, payload, payload_len);
                                    uart_buf[payload_len] = '\0';
                                    uart_cnt = payload_len;
                                    uart_flag = 1;
                                }
                            }
                        mqtt_skip: ;
                        }
                    }
                }
            }
        }

        /* ================================================================
         * 统一命令匹配 (来自 MQTT payload 或 UART 串口)
         * 参考项目模式: 同时匹配 uart_buf 和 g_esp8266_rx_buf
         * ================================================================ */
        {
            const char *mb = (const char *)uart_buf;
            const char *rb = (const char *)g_esp8266_rx_buf;

            if (strstr(mb, "test") || strstr(rb, "test")) {
                USART2_SendString("test command ok\r\n");
                UI_ShowMsg("OK");
            }
            else if (strstr(mb, "ledon") || strstr(rb, "ledon")) {
                PBout(5) = 0;
                USART2_SendString("LED ON OK\r\n");
                UI_ShowMsg("LED On");
            }
            else if (strstr(mb, "ledoff") || strstr(rb, "ledoff")) {
                PBout(5) = 1;
                USART2_SendString("LED OFF OK\r\n");
                UI_ShowMsg("LED Off");
            }
            else if (strstr(mb, "beepon") || strstr(rb, "beepon")) {
                beep_on(500);
                USART2_SendString("BEEP on OK\r\n");
                UI_ShowMsg("BEEP");
            }
            else if (strstr(mb, "goA") || strstr(rb, "goA")) {
                Motor_Forward(speed);
                USART2_SendString("Go ahead OK\r\n");
                UI_ShowMsg("Go A");
            }
            else if (strstr(mb, "goB") || strstr(rb, "goB")) {
                Motor_Backward(speed);
                USART2_SendString("Go back OK\r\n");
                UI_ShowMsg("Go B");
            }
            else if (strstr(mb, "goL") || strstr(rb, "goL")) {
                Motor_Left(speed);
                USART2_SendString("Go Left OK\r\n");
                UI_ShowMsg("Go L");
            }
            else if (strstr(mb, "goR") || strstr(rb, "goR")) {
                Motor_Right(speed);
                USART2_SendString("Go Right OK\r\n");
                UI_ShowMsg("Go R");
            }
            else if (strstr(mb, "stop") || strstr(rb, "stop")) {
                Motor_Stop();
                USART2_SendString("Stop OK\r\n");
                UI_ShowMsg("Stop");
            }
            /* ---- 左轮微调 (在 speedup 之前) ---- */
            else if (strstr(mb, "lspeedup")) {
                if (left_trim < 300) left_trim += 10;
                sprintf((char *)uart_buf, "Left:%d\r\n",
                        speed + left_trim);
                USART2_SendString((char *)uart_buf);
                goto clear_buffers;
            }
            else if (strstr(mb, "lspeeddown")) {
                if (left_trim > -300) left_trim -= 10;
                sprintf((char *)uart_buf, "Left:%d\r\n",
                        speed + left_trim);
                USART2_SendString((char *)uart_buf);
                goto clear_buffers;
            }
            /* ---- 右轮微调 ---- */
            else if (strstr(mb, "rspeedup")) {
                if (right_trim < 300) right_trim += 10;
                sprintf((char *)uart_buf, "Right:%d\r\n",
                        speed + right_trim);
                USART2_SendString((char *)uart_buf);
                goto clear_buffers;
            }
            else if (strstr(mb, "rspeeddown")) {
                if (right_trim > -300) right_trim -= 10;
                sprintf((char *)uart_buf, "Right:%d\r\n",
                        speed + right_trim);
                USART2_SendString((char *)uart_buf);
                goto clear_buffers;
            }
            /* ---- 速度 ---- */
            else if (strstr(mb, "speedup")) {
                if (speed < 1000) speed += 100;
                sprintf((char *)uart_buf, "speed:%d\r\n", speed);
                USART2_SendString((char *)uart_buf);
                UI_ShowMsg((char *)uart_buf);
            }
            else if (strstr(mb, "speeddown")) {
                if (speed > 0) speed -= 100;
                sprintf((char *)uart_buf, "speed:%d\r\n", speed);
                USART2_SendString((char *)uart_buf);
                UI_ShowMsg((char *)uart_buf);
            }
            /* ---- 模式切换 ---- */
            else if (strstr(mb, "remote")) {
                work_mode = MODE_REMOTE;
                Motor_Stop();
                USART2_SendString("Remote Mode\r\n");
                UI_ShowMsg("Remote");
            }
            else if (strstr(mb, "auto")) {
                work_mode = MODE_AUTO;
                USART2_SendString("Auto Mode\r\n");
                UI_ShowMsg("Auto");
            }
            else if (strstr(mb, "follow")) {
                work_mode = MODE_FOLLOW;
                Motor_Stop();
                USART2_SendString("Follow Mode\r\n");
                UI_ShowMsg("Follow");
            }
            /* ---- UI ---- */
            else if (strstr(mb, "ui2")) {
                UI_ShowTest();
                goto clear_buffers;
            }
            else if (strstr(mb, "wifi")) {
                UI_ShowWiFi(3, 400);
                goto clear_buffers;
            }
            /* ---- 菜单 / 贪吃蛇 ---- */
            else if (!Snake_IsActive() && !Menu_IsActive()
                     && strstr(mb, "menu")) {
                Menu_Enter();
                goto clear_buffers;
            }
            else if (!Snake_IsActive() && strstr(mb, "snake")) {
                Snake_Enter();
                goto clear_buffers;
            }
            /* ---- 方向键 (上下文感知) ---- */
            else if (strstr(mb, "up")) {
                if (Snake_IsActive())
                    Snake_SetDir(SNAKE_UP);
                else if (Menu_IsActive())
                    Menu_Up();
                else {
                    Motor_Forward(speed);
                    UI_ShowMsg("Go A");
                }
                goto clear_buffers;
            }
            else if (strstr(mb, "down")) {
                if (Snake_IsActive())
                    Snake_SetDir(SNAKE_DOWN);
                else if (Menu_IsActive())
                    Menu_Down();
                else {
                    Motor_Backward(speed);
                    UI_ShowMsg("Go B");
                }
                goto clear_buffers;
            }
            else if (strstr(mb, "left")) {
                if (Snake_IsActive())
                    Snake_SetDir(SNAKE_LEFT);
                else if (Menu_IsActive())
                    Menu_Left();
                else {
                    Motor_Left(speed);
                    UI_ShowMsg("Go L");
                }
                goto clear_buffers;
            }
            else if (strstr(mb, "right")) {
                if (Snake_IsActive())
                    Snake_SetDir(SNAKE_RIGHT);
                else if (Menu_IsActive())
                    Menu_Right();
                else {
                    Motor_Right(speed);
                    UI_ShowMsg("Go R");
                }
                goto clear_buffers;
            }
            else if (strstr(mb, "ok")) {
                if (Snake_IsActive())
                    Snake_Exit();
                else
                    Menu_Ok();
                goto clear_buffers;
            }
            else if (strstr(mb, "exit")) {
                if (Snake_IsActive())
                    Snake_Exit();
                else
                    Menu_Exit();
                goto clear_buffers;
            }
            /* ---- WiFi 配置 ---- */
            else if (strstr(mb, "ssid ")) {
                WifiCfg_SetSSID((char *)uart_buf + 5);
                goto clear_buffers;
            }
            else if (strstr(mb, "pass ")) {
                WifiCfg_SetPass((char *)uart_buf + 5);
                goto clear_buffers;
            }
            else if (strstr(mb, "connect")) {
                USART2_SendString("Connecting...\r\n");
                WifiCfg_Connect();
                goto clear_buffers;
            }
            /* ---- 调试命令 ---- */
            else if (Debug_ProcessCmd(mb) == 0) {
                goto clear_buffers;
            }
            else {
                USART2_SendString("Unknown Command\r\n");
                UI_ShowMsg("?");
            }
        }

    clear_buffers:
        /* 统一清理所有缓冲区 (参考项目模式) */
        memset((char *)g_esp8266_rx_buf, 0, sizeof(g_esp8266_rx_buf));
        g_esp8266_rx_cnt = 0;
        g_esp8266_rx_end = 0;

        memset((char *)uart_buf, 0, sizeof(uart_buf));
        uart_cnt = 0;
        uart_flag = 0;
    }

    /*
     * 延迟发送华为云命令响应 ——
     *   在缓冲区清理、MQTT 处理完成之后再发，
     *   避免 ESP8266 透传模式下收发冲突导致响应包被丢弃
     */
    if (s_cmd_pending) {
        char resp_topic[160];
        uint8_t pi, ri;

        for (pi = 0; MQTT_CMD_RESP_TOPIC[pi]; pi++)
            resp_topic[pi] = MQTT_CMD_RESP_TOPIC[pi];
        for (ri = 0; s_cmd_req_id[ri]; ri++)
            resp_topic[pi++] = s_cmd_req_id[ri];
        resp_topic[pi] = '\0';

        mqtt_publish_data(resp_topic,
                          "{\"result_code\":0,"
                          "\"response_name\":\"COMMAND_RESPONSE\","
                          "\"paras\":{\"result\":\"success\"}}",
                          0);
        USART2_SendString("[CMD] response sent\r\n");
        s_cmd_pending = 0;
    }
}

void USART3_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    /* TX PB10 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* RX PB11 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART3, &USART_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);
}

void USART3_SendChar(uint8_t ch)
{
    while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    USART_SendData(USART3, ch);
}

void USART3_SendString(char *str)
{
    while(*str)
        USART3_SendChar(*str++);
}

void USART3_IRQHandler(void)
{
    uint8_t data;
    static uint32_t cnt = 0;

    /* MQTT 剩余长度解码（参照参考代码） */
    static int mqtt_rem = 0;
    static int mqtt_rl_bytes = 0;
    static int mqtt_mult = 1;
    static int mqtt_done = 0;
    static int mqtt_pkt_len = 0;

    if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
    {
        data = USART_ReceiveData(USART3);

        /* JSON 括号计数 */
        if(data == '{') cnt++;
        else if(data == '}') cnt--;

        /* 写入 ESP8266 接收缓冲 */
        if (g_esp8266_rx_cnt < sizeof(g_esp8266_rx_buf) - 1)
            g_esp8266_rx_buf[g_esp8266_rx_cnt++] = data;
        else
            g_esp8266_rx_end = 1;

        /* 首个字节：初始化 MQTT 包长度解析 */
        if (g_esp8266_rx_cnt == 1) {
            mqtt_rem = 0; mqtt_rl_bytes = 0;
            mqtt_mult = 1; mqtt_done = 0; mqtt_pkt_len = 0;
        }

        /* 从第2字节开始解码 MQTT 剩余长度 */
        if (!mqtt_done && g_esp8266_rx_cnt > 1 && mqtt_rl_bytes < 4) {
            mqtt_rem += (data & 0x7F) * mqtt_mult;
            mqtt_mult *= 128;
            mqtt_rl_bytes++;
            if (!(data & 0x80)) {
                mqtt_done = 1;
                mqtt_pkt_len = 1 + mqtt_rl_bytes + mqtt_rem;
            }
        }

        /* MQTT 包完整接收 → 设结束标志 */
        if (mqtt_done && mqtt_pkt_len > 0 && g_esp8266_rx_cnt >= (uint32_t)mqtt_pkt_len)
            g_esp8266_rx_end = 1;

        /* 透传模式下 JSON 括号闭合作为补充判断 */
        if (g_esp8266_transparent_transmission_sta && data == '}' && cnt == 0)
            g_esp8266_rx_end = 1;

        if (g_esp8266_rx_cnt >= sizeof(g_esp8266_rx_buf) - 1)
            g_esp8266_rx_end = 1;

    #if EN_DEBUG_ESP8266
        USART_SendData(USART1, data);
        while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    #endif

        /* 原路返回 — 写入命令缓冲供 parse_cmd 处理 */
        USART2_SendChar(data);
        if (uart_cnt < sizeof(uart_buf) - 1) {
            uart_buf[uart_cnt++] = data;
            if (uart_buf[uart_cnt - 1] == '*' || uart_cnt >= sizeof(uart_buf) - 1)
                uart_flag = 1;
        }

        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
}
void usart3_send_bytes(uint8_t *buf, uint32_t len)
{
    uint8_t *p = buf;
    while(len--)
    {
        USART_SendData(USART3, *p++);
        while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    }
}

void usart3_send_str(char *str)
{
    char *p = str;
    while(*p != '\0')
    {
        USART_SendData(USART3, *p++);
        while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    }
}
