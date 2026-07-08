#ifndef __ESP8266_H__
#define __ESP8266_H__


/******************************************************************************
 * 调试开关
 ******************************************************************************/

/**
 * @def EN_DEBUG_ESP8266
 * @brief ESP8266模块调试输出开关
 *        0 = 关闭调试输出（生产模式）
 *        1 = 开启调试输出（可通过串口打印ESP8266通信数据）
 */
#define EN_DEBUG_ESP8266	0


/******************************************************************************
 * WiFi 连接配置
 ******************************************************************************/

/**
 * @def WIFI_SSID
 * @brief 目标WiFi热点名称（SSID）
 *        默认连接名为"scp"的WiFi网络
 * @note  可通过WiFi配置界面（WifiCfg模块）在运行时修改
 */
#define WIFI_SSID 			"scp"

/**
 * @def WIFI_PASSWORD
 * @brief WiFi密码
 *        默认为"999999999"
 * @note  可通过WiFi配置界面（WifiCfg模块）在运行时修改
 */
#define WIFI_PASSWORD		"999999999"


/******************************************************************************
 * 全局变量声明
 ******************************************************************************/

/**
 * @brief ESP8266发送缓冲区，512字节
 *        用于存储待发送的AT指令或MQTT报文
 */
extern uint8_t  g_esp8266_tx_buf[512];

/**
 * @brief ESP8266接收缓冲区，512字节（volatile，中断中修改）
 *        USART3接收中断将ESP8266的响应数据存入此缓冲区
 *        主循环和MQTT处理函数从该缓冲区读取数据进行解析
 */
extern volatile uint8_t  g_esp8266_rx_buf[512];

/**
 * @brief 接收字节计数器，由USART3中断累加
 *        表示当前已接收到的数据字节数
 */
extern volatile uint32_t g_esp8266_rx_cnt;

/**
 * @brief 接收完成标志，由USART3中断置位
 *        0 = 接收未完成
 *        1 = 一帧数据接收完毕
 */
extern volatile uint32_t g_esp8266_rx_end;

/**
 * @brief 透传模式状态标志
 *        0 = 非透传模式（AT指令模式）
 *        1 = 透传模式（数据直接转发到TCP连接）
 */
extern volatile uint32_t g_esp8266_transparent_transmission_sta;


/******************************************************************************
 * 函数声明
 ******************************************************************************/

/**
 * @brief   初始化ESP8266模块（初始化USART3串口通信）
 */
extern void 	esp8266_init(void);

/**
 * @brief   AT指令自检，验证ESP8266模块是否正常响应
 * @return  0=正常，-1=无响应
 */
extern int32_t  esp8266_self_test(void);

/**
 * @brief   查询当前WiFi/TCP连接状态
 * @return  0=WiFi+TCP已连，1=仅WiFi，-1=断开，-2=无响应
 */
extern int32_t 	esp8266_check_status(void);

/**
 * @brief   退出透传模式（发送+++）
 * @return  0=成功
 */
extern int32_t 	esp8266_exit_transparent_transmission(void);

/**
 * @brief   进入透传模式（AT+CIPMODE=1 + AT+CIPSEND）
 * @return  0=成功，-1/-2=失败
 */
extern int32_t 	esp8266_entry_transparent_transmission(void);

/**
 * @brief   连接WiFi热点（Station模式）
 * @param   ssid - WiFi名称
 * @param   pswd - WiFi密码
 * @return  0=成功，-1/-2=失败
 */
extern int32_t 	esp8266_connect_ap(char* ssid, char* pswd);

/**
 * @brief   使用TCP/UDP协议连接到远程服务器
 * @param   mode - 协议类型 "TCP" 或 "UDP"
 * @param   ip   - 目标服务器地址
 * @param   port - 目标端口号
 * @return  0=成功，-1=失败
 */
extern int32_t 	esp8266_connect_server(char* mode, char* ip, uint16_t port);

/**
 * @brief   断开服务器连接
 * @return  0=成功，-1=失败
 */
extern int32_t 	esp8266_disconnect_server(void);

/**
 * @brief   发送原始字节数据（用于MQTT透传或AT指令）
 * @param   buf - 数据缓冲区
 * @param   len - 数据长度
 */
extern void 	esp8266_send_bytes(uint8_t *buf, uint32_t len);

/**
 * @brief   发送字符串（用于MQTT透传）
 * @param   buf - 待发送字符串
 */
extern void 	esp8266_send_str(char *buf);

/**
 * @brief   发送AT指令（自动清空接收缓冲区）
 * @param   str - AT指令字符串（需含\r\n）
 */
extern void 	esp8266_send_at(char *str);

/**
 * @brief   使能/禁用多连接模式
 * @param   b - 0=单连接，1=多连接
 * @return  0=成功，-1=失败
 */
extern int32_t  esp8266_enable_multiple_id(uint32_t b);

/**
 * @brief   创建TCP服务器（需先使能多连接模式）
 * @param   port - 监听端口号
 * @return  0=成功，-1=失败
 */
extern int32_t 	esp8266_create_server(uint16_t port);

/**
 * @brief   关闭TCP服务器
 * @param   port - 端口号
 * @return  0=成功，-1=失败
 */
extern int32_t 	esp8266_close_server(uint16_t port);

/**
 * @brief   使能/关闭AT指令回显
 * @param   b - 0=关闭(ATE0)，1=开启(ATE1)
 * @return  0=成功，-1=失败
 */
extern int32_t 	esp8266_enable_echo(uint32_t b);

/**
 * @brief   复位ESP8266模块（AT+RST）
 * @return  0=成功，-1=失败
 */
extern int32_t 	esp8266_reset(void);

#endif
