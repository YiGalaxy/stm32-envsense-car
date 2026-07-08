#include "stm32f10x.h"
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/******************************************************************************
 * 全局变量
 ******************************************************************************/

/**
 * @brief ESP8266 发送缓冲区，用于暂存待发送的AT指令或MQTT报文数据
 *        512字节，足够容纳完整MQTT CONNECT/PUBLISH/SUBSCRIBE报文
 */
uint8_t  g_esp8266_tx_buf[512];

/**
 * @brief ESP8266 接收缓冲区（volatile，被USART3中断写入）
 *        存储从ESP8266串口收到的AT响应数据
 *        主循环轮询该缓冲区以判断AT指令执行结果
 */
volatile uint8_t  g_esp8266_rx_buf[512];

/**
 * @brief 接收计数器，由USART3接收中断累加，记录已接收字节数
 */
volatile uint32_t g_esp8266_rx_cnt = 0;

/**
 * @brief 接收结束标志，由USART3接收中断置位，表示一帧数据接收完毕
 */
volatile uint32_t g_esp8266_rx_end = 0;

/**
 * @brief 透传模式状态标志
 *        0 = 非透传模式（可发送AT指令）
 *        1 = 透传模式（数据透传发送，不可发送AT指令）
 */
volatile uint32_t g_esp8266_transparent_transmission_sta = 0;


/******************************************************************************
 * 函数实现
 ******************************************************************************/

/**
 * @brief   ESP8266 模块初始化
 * @details 初始化与ESP8266连接的USART3串口，波特率115200
 *          ESP8266模块默认AT固件波特率为115200
 * @param   无
 * @return  无
 * @note    ESP8266的GPIO初始化（RST/CH_PD/GPIO0）在硬件初始化阶段完成
 */
void esp8266_init(void)
{
    USART3_Init(115200);  /* 初始化USART3串口，波特率115200 */
}


/**
 * @brief   发送AT指令并清空接收缓冲区
 * @details 先将接收缓冲区清空并复位计数器，然后发送AT指令字符串
 *          这样后续调用esp8266_find_str_in_rx_packet()时只检查本次响应
 * @param   str - 要发送的AT指令字符串（需包含\r\n结束符）
 * @return  无
 * @note    发送前强制清空缓冲区，避免旧数据干扰指令响应判断
 */
void esp8266_send_at(char *str)
{
    /* 发送AT指令前清空接收缓冲区，避免上次残留数据干扰 */
    memset((void *)g_esp8266_rx_buf, 0, sizeof g_esp8266_rx_buf);

    /* 复位接收计数器，从零开始记录本次响应数据 */
    g_esp8266_rx_cnt = 0;

    /* 通过USART3发送AT指令给ESP8266 */
    USART3_SendString(str);
}


/**
 * @brief   发送原始字节数据（用于MQTT透传）
 * @details 直接通过USART3发送指定长度的原始数据，不添加任何额外字符
 *          通常用于MQTT协议报文发送（透传模式下直接发送二进制报文）
 * @param   buf - 待发送的数据缓冲区指针
 * @param   len - 待发送的数据长度（字节数）
 * @return  无
 */
void esp8266_send_bytes(uint8_t *buf, uint32_t len)
{
    usart3_send_bytes(buf, len);
}


/**
 * @brief   发送字符串（用于MQTT透传）
 * @details 通过USART3发送ASCII字符串
 *          与esp8266_send_at()不同，此函数不清空接收缓冲区
 * @param   buf - 待发送的字符串指针
 * @return  无
 */
void esp8266_send_str(char *buf)
{
    usart3_send_str(buf);
}


/**
 * @brief   在接收缓冲区中查找指定字符串（带超时）
 * @details 在主循环中轮询esp8266_rx_buf，查找目标字符串是否出现
 *          每1ms轮询一次，直到超时或找到目标字符串
 * @param   str     - 要查找的目标字符串
 * @param   timeout - 超时时间（毫秒）
 * @return  0  - 找到目标字符串
 *          -1 - 超时未找到
 * @note    由于ESP8266的AT响应是异步的，需轮询等待响应
 */
int32_t esp8266_find_str_in_rx_packet(char *str, uint32_t timeout)
{
    while (timeout--) {
        /* 在接收缓冲区中查找目标字符串 */
        if (strstr((const char *)g_esp8266_rx_buf, str))
            return 0;     /* 找到，返回成功 */
        delay_ms(1);      /* 每1ms轮询一次 */
    }
    return -1;            /* 超时未找到，返回失败 */
}


/**
 * @brief   AT指令自检
 * @details 发送AT基本测试指令，检测ESP8266模块是否正常响应
 *          若模块正常工作，会回复"OK"
 * @param   无
 * @return  0  - 模块响应正常
 *          -1 - 模块无响应（超时未收到OK）
 * @note    这是最基本的ESP8266通信检测，其他所有功能都依赖于此
 */
int32_t esp8266_self_test(void)
{
    /* 发送AT测试指令 */
    esp8266_send_at("AT\r\n");

    /* 等待模块回复"OK"，超时1000ms */
    return esp8266_find_str_in_rx_packet("OK", 1000);
}


/**
 * @brief   检查WiFi/TCP连接状态
 * @details 发送AT+CIPSTATUS指令，解析返回的STATUS字段判断模块状态
 *          STATUS字段含义：
 *            2 - 已连接WiFi但未建立TCP/UDP连接
 *            3 - WiFi和TCP/UDP均已连接
 *            4 - WiFi已断开
 *            5 - WiFi未连接（未配置AP或连接失败）
 * @param   无
 * @return  0  - WiFi + TCP/UDP 均已连接（STATUS:3）
 *          1  - 仅WiFi已连接，无TCP/UDP连接（STATUS:2）
 *          -1 - WiFi断开或未连接（STATUS:4/5）
 *          -2 - 模块无响应（AT+CIPSTATUS无OK回复）
 */
int32_t esp8266_check_status(void)
{
    char *p;

    /* 发送AT+CIPSTATUS查询连接状态 */
    esp8266_send_at("AT+CIPSTATUS\r\n");
    /* 等待模块回复OK，超时3000ms */
    if (esp8266_find_str_in_rx_packet("OK", 3000))
        return -2;      /* 模块无响应 */

    /* 解析"STATUS:"字段 */
    p = strstr((const char *)g_esp8266_rx_buf, "STATUS:");
    if (!p) return -1;  /* 未找到STATUS字段，视为断开 */

    p += 7;             /* 跳过"STATUS:"（7个字符），指向数字 */

    /* 根据状态码返回对应状态 */
    switch (*p) {
    case '3': return 0;  /* STATUS:3  WiFi和TCP连接均正常 */
    case '2': return 1;  /* STATUS:2  仅WiFi已连接 */
    default:  return -1; /* STATUS:4(断开) 或 5(未连接AP) */
    }
}


/**
 * @brief   连接到WiFi热点
 * @details 先将ESP8266设置为Station模式，再发送AT+CWJAP_CUR连接指定WiFi
 *          为避免sprintf栈溢出，采用逐段发送方式构建AT指令
 * @param   ssid - WiFi热点名称（SSID）
 * @param   pswd - WiFi密码
 * @return  0  - 连接成功
 *          -1 - Station模式设置失败
 *          -2 - 连接AP超时（SSID/密码错误或DHCP分配IP失败）
 * @note    连接超时设为15秒（常规家用路由通常在3~8秒内完成连接）
 * @note    连接失败原因：
 *          - SSID或密码错误
 *          - 路由器DHCP故障，未给ESP8266分配IP
 *          - 信号强度不足
 */
int32_t esp8266_connect_ap(char* ssid, char* pswd)
{
#if 0
    /* 弃用实现：sprintf使用栈空间过大，改为逐段发送方式 */
    char buf[128]={0};
    sprintf(buf,"AT+CWJAP_CUR=\"%s\",\"%s\"\r\n",ssid,pswd);
#endif

    /* 设置ESP8266为Station模式（客户端模式） */
    esp8266_send_at("AT+CWMODE_CUR=1\r\n");

    /* 等待模式设置完成，超时1000ms */
    if (esp8266_find_str_in_rx_packet("OK", 1000))
        return -1;

    /* 逐段发送AT+CWJAP_CUR指令以避免sprintf栈溢出 */
    /* 指令格式: AT+CWJAP_CUR="SSID","PASSWORD"\r\n */
    esp8266_send_at("AT+CWJAP_CUR=");
    esp8266_send_at("\""); esp8266_send_at(ssid); esp8266_send_at("\"");
    esp8266_send_at(",");
    esp8266_send_at("\""); esp8266_send_at(pswd); esp8266_send_at("\"");
    esp8266_send_at("\r\n");

    /* 等待连接结果，连续查两次（OK + CONNECT），总超时约30秒 */
    if (esp8266_find_str_in_rx_packet("OK", 15000))
        if (esp8266_find_str_in_rx_packet("CONNECT", 15000))
            return -2;  /* 连接超时 */

    return 0;           /* 连接成功 */
}


/**
 * @brief   退出透传模式
 * @details 向ESP8266发送"+++"（特殊退出序列）退出数据透传模式
 *          ESP8266收到"+++"后会退出透传回到AT指令模式
 *          退出后需等待至少1秒才能发送下一条AT指令
 * @param   无
 * @return  0 - 操作成功
 * @note    透传模式下模块将所有串口数据直接转发到TCP连接，
 *         "+++"是ESP8266定义的退出透传的特殊序列（非AT指令）
 */
int32_t esp8266_exit_transparent_transmission(void)
{
    /* 发送"+++"退出透传模式（ESP8266特殊退出序列） */
    esp8266_send_at("+++");

    /* 退出透传后需等待1秒再发送AT指令（ESP8266固件要求） */
    delay_ms(1000);

    /* 等待模块回复OK，确认已退出透传 */
    if (esp8266_find_str_in_rx_packet("OK", 3000)) {
        /* 超时未收到OK，可能不在透传模式，不影响后续操作 */
        g_esp8266_transparent_transmission_sta = 0;
        return 0;
    }

    /* 标记当前为非透传状态 */
    g_esp8266_transparent_transmission_sta = 0;

    return 0;
}


/**
 * @brief   进入透传模式
 * @details 设置ESP8266进入TCP数据透传模式
 *          先发送AT+CIPMODE=1开启透传模式，再发送AT+CIPSEND进入发送状态
 *          透传模式下所有串口数据自动转发到已建立的TCP连接
 * @param   无
 * @return  0  - 成功进入透传模式
 *          -1 - AT+CIPMODE=1设置失败
 *          -2 - AT+CIPSEND进入发送状态失败（未收到'>'提示符）
 * @note    进入透传前必须先建立TCP连接（通过esp8266_connect_server）
 *         AT+CIPSEND成功后模块返回'>'提示符表示可以发送数据
 */
int32_t esp8266_entry_transparent_transmission(void)
{
    /* 开启透传模式：AT+CIPMODE=1 */
    esp8266_send_at("AT+CIPMODE=1\r\n");
    if (esp8266_find_str_in_rx_packet("OK", 5000))
        return -1;      /* 透传模式设置失败 */

    delay_ms(2000);

    /* 执行AT+CIPSEND命令，进入数据发送状态 */
    esp8266_send_at("AT+CIPSEND\r\n");
    /* 等待模块返回'>'提示符，表示可以发送数据 */
    if (esp8266_find_str_in_rx_packet(">", 5000))
        return -2;      /* 未收到提示符 */

    /* 标记当前为透传状态 */
    g_esp8266_transparent_transmission_sta = 1;
    return 0;
}


/**
 * @brief   使用指定协议连接到远程服务器
 * @details 发送AT+CIPSTART指令建立TCP或UDP连接
 *          为避免sprintf栈溢出，采用逐段发送方式构建指令
 * @param   mode - 协议类型，如"TCP"或"UDP"
 * @param   ip   - 目标服务器IP地址或域名
 * @param   port - 目标服务器端口号
 * @return  0  - 连接成功
 *          -1 - 连接失败（超时10秒未收到CONNECT或OK）
 * @note    连接失败原因：
 *          - 远程服务器IP或端口错误
 *          - 未连接AP（必须先调用esp8266_connect_ap）
 *          - 防火墙阻止（通常不会发生）
 * @note    MQTT连接通常使用TCP协议，端口1883
 */
int32_t esp8266_connect_server(char* mode, char* ip, uint16_t port)
{
#if 0
    /* 弃用实现：使用MQTT连接IP地址，字符串"AT+CIPSTART=..."占用大量栈空间 */
    char buf[128]={0};
    sprintf((char*)buf,"AT+CIPSTART=\"%s\",\"%s\",%d\r\n",mode,ip,port);
    esp8266_send_at(buf);
#else
    char buf[16] = {0};

    /* 逐段发送AT+CIPSTART指令：AT+CIPSTART="TCP","IP",PORT\r\n */
    esp8266_send_at("AT+CIPSTART=");
    esp8266_send_at("\""); esp8266_send_at(mode); esp8266_send_at("\"");
    esp8266_send_at(",");
    esp8266_send_at("\""); esp8266_send_at(ip); esp8266_send_at("\"");
    esp8266_send_at(",");
    sprintf(buf, "%d", port);   /* 将端口号转为字符串 */
    esp8266_send_at(buf);
    esp8266_send_at("\r\n");
#endif

    /* 等待连接结果，先查CONNECT再查OK，总超时约15秒 */
    if (esp8266_find_str_in_rx_packet("CONNECT", 10000))
        if (esp8266_find_str_in_rx_packet("OK", 5000))
            return -1;      /* 连接失败 */
    return 0;               /* 连接成功 */
}


/**
 * @brief   断开服务器连接
 * @details 发送AT+CIPCLOSE指令关闭当前TCP/UDP连接
 * @param   无
 * @return  0  - 断开成功
 *          -1 - 断开失败（超时未收到CLOSED或OK）
 */
int32_t esp8266_disconnect_server(void)
{
    /* 发送AT+CIPCLOSE指令关闭连接 */
    esp8266_send_at("AT+CIPCLOSE\r\n");

    /* 等待断开完成，先查CLOSED再查OK，总超时约10秒 */
    if (esp8266_find_str_in_rx_packet("CLOSED", 5000))
        if (esp8266_find_str_in_rx_packet("OK", 5000))
            return -1;      /* 断开失败 */

    return 0;               /* 断开成功 */
}


/**
 * @brief   使能/禁用多连接模式
 * @details 发送AT+CIPMUX指令设置ESP8266是否支持多路连接
 *          多连接模式（AT+CIPMUX=1）下最多支持4路连接
 *          创建TCP服务器前必须先使能多连接模式
 * @param   b - 0=单连接模式（只能创建一个连接）
 *              1=多连接模式（支持最多4个连接）
 * @return  0  - 设置成功
 *          -1 - 设置失败（超时未收到OK）
 */
int32_t esp8266_enable_multiple_id(uint32_t b)
{
    char buf[32] = {0};

    /* 构建AT+CIPMUX指令并发送 */
    sprintf(buf, "AT+CIPMUX=%d\r\n", b);
    esp8266_send_at(buf);

    /* 等待指令执行完成 */
    if (esp8266_find_str_in_rx_packet("OK", 5000))
        return -1;      /* 设置失败 */

    return 0;           /* 设置成功 */
}


/**
 * @brief   创建TCP服务器
 * @details 发送AT+CIPSERVER=1指令在ESP8266上创建TCP服务器
 *          ESP8266作为SoftAP或Station+SoftAP模式时，其他设备可连接到此服务器
 *          前提：必须先使能多连接模式（AT+CIPMUX=1）
 * @param   port - 服务器监听端口号
 * @return  0  - 服务器创建成功
 *          -1 - 服务器创建失败（超时未收到OK）
 * @note    关闭服务器需调用esp8266_close_server()
 */
int32_t esp8266_create_server(uint16_t port)
{
    char buf[32] = {0};

    /* 构建AT+CIPSERVER指令并发送 */
    sprintf(buf, "AT+CIPSERVER=1,%d\r\n", port);
    esp8266_send_at(buf);

    /* 等待服务器创建完成 */
    if (esp8266_find_str_in_rx_packet("OK", 5000))
        return -1;      /* 创建失败 */

    return 0;           /* 创建成功 */
}


/**
 * @brief   关闭TCP服务器
 * @details 发送AT+CIPSERVER=0指令关闭ESP8266上的TCP服务器
 * @param   port - 要关闭的服务器端口号
 * @return  0  - 服务器关闭成功
 *          -1 - 服务器关闭失败（超时未收到OK）
 */
int32_t esp8266_close_server(uint16_t port)
{
    char buf[32] = {0};

    /* 构建AT+CIPSERVER=0指令并发送 */
    sprintf(buf, "AT+CIPSERVER=0,%d\r\n", port);
    esp8266_send_at(buf);

    /* 等待服务器关闭完成 */
    if (esp8266_find_str_in_rx_packet("OK", 5000))
        return -1;      /* 关闭失败 */

    return 0;           /* 关闭成功 */
}


/**
 * @brief   使能/关闭AT指令回显
 * @details 发送ATE1或ATE0指令控制ESP8266的AT指令回显功能
 *          回显开启时，ESP8266会将收到的AT指令再回传一遍，方便调试
 *          回显关闭时，ESP8266只返回执行结果，减少数据量
 * @param   b - 0=关闭回显（ATE0）
 *              1=开启回显（ATE1）
 * @return  0  - 设置成功
 *          -1 - 设置失败（超时未收到OK）
 */
int32_t esp8266_enable_echo(uint32_t b)
{
    if (b)
        esp8266_send_at("ATE1\r\n");  /* 开启回显 */
    else
        esp8266_send_at("ATE0\r\n");  /* 关闭回显 */

    if (esp8266_find_str_in_rx_packet("OK", 5000))
        return -1;      /* 设置失败 */

    return 0;           /* 设置成功 */
}


/**
 * @brief   复位ESP8266模块
 * @details 发送AT+RST指令使ESP8266执行软复位
 *          复位后模块会重新启动并打印启动信息（约3秒）
 *          复位后WiFi配置保持不变（保存在Flash中）
 * @param   无
 * @return  0  - 复位成功
 *          -1 - 复位失败（超时10秒未收到OK）
 * @note    复位后需重新进行WiFi连接和TCP连接（透传模式也会退出）
 */
int32_t esp8266_reset(void)
{
    /* 发送AT+RST指令复位模块 */
    esp8266_send_at("AT+RST\r\n");

    /* 等待模块重启完成，超时10000ms */
    if (esp8266_find_str_in_rx_packet("OK", 10000))
        return -1;      /* 复位失败 */

    return 0;           /* 复位成功 */
}
