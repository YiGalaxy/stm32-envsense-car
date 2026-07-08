#include "stm32f10x.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "esp8266.h"
#include "esp8266_mqtt.h"
#include "wificfg.h"
#include "oled.h"
#include "ui_wifi.h"
#include "dht11.h"
#include "snake.h"
//#include <includes.h>



// 连接成功应答包 20 02 00 00
// 客户端主动断开包 e0 00
const uint8_t g_packet_connect_ack[4] = {0x20,0x02,0x00,0x00};
const uint8_t g_packet_disconnect[2]  = {0xe0,0x00};
const uint8_t g_packet_heart[2]       = {0xc0,0x00};
const uint8_t g_packet_heart_reply[2] = {0xc0,0x00};
const uint8_t g_packet_sub_ack[2]     = {0x90,0x03};


char  g_mqtt_msg[526];

uint16_t g_mqtt_tx_len;

//MQTT发送字节
void mqtt_send_bytes(uint8_t *buf,uint32_t len)
{
    esp8266_send_bytes(buf,len);
}

//发送心跳包
void mqtt_send_heart(void)
{
    mqtt_send_bytes((uint8_t *)g_packet_heart,sizeof(g_packet_heart));
}

//MQTT断开连接
void mqtt_disconnect()
{
    mqtt_send_bytes((uint8_t *)g_packet_disconnect,sizeof(g_packet_disconnect));
}

//MQTT初始化
void mqtt_init(uint8_t *prx,uint16_t rxlen,uint8_t *ptx,uint16_t txlen)
{
    memset(g_esp8266_tx_buf,0,sizeof(g_esp8266_tx_buf)); //清空发送缓冲区
    memset((void *)g_esp8266_rx_buf,0,sizeof(g_esp8266_rx_buf)); //清空接收缓冲区

    //强制断开之前连接
    mqtt_disconnect();
    delay_ms(100);
	
    mqtt_disconnect();
    delay_ms(100);
}

//MQTT连接服务器的处理函数
int32_t mqtt_connect(char *client_id,char *user_name,char *password)
{
    int32_t client_id_len = strlen(client_id);
    int32_t user_name_len = strlen(user_name);
    int32_t password_len = strlen(password);
    int32_t data_len;
    uint32_t cnt=2;
    uint32_t wait;
    g_mqtt_tx_len=0;
	
    //可变报头+Payload 每个字段包含两个字节的长度标识
    data_len = 10 + (client_id_len+2) + (user_name_len+2) + (password_len+2);

    //固定报头
    //控制报文类型
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0x10;		//MQTT Message Type CONNECT
    //剩余长度(不包含固定报头)
    do
    {
        uint8_t encodedByte = data_len % 128;
        data_len = data_len / 128;
        // if there are more data to encode, set the top bit of this byte
        if ( data_len > 0 )
            encodedByte = encodedByte | 128;
        g_esp8266_tx_buf[g_mqtt_tx_len++] = encodedByte;
    } while ( data_len > 0 );

    //可变报头
    //协议名
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0;        		// Protocol Name Length MSB
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 4;        		// Protocol Name Length LSB
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'M';        	// ASCII Code for M
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'Q';        	// ASCII Code for Q
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'T';        	// ASCII Code for T
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'T';        	// ASCII Code for T
    //协议级别
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 4;        		// MQTT Protocol version = 4
    //连接标志
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0xc2;        	// conn flags
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0;        		// Keep-alive Time Length MSB
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 60;        	// Keep-alive Time Length LSB  60S 心跳间隔

    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(client_id_len);// Client ID length MSB
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(client_id_len);// Client ID length LSB
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],client_id,client_id_len);
    g_mqtt_tx_len += client_id_len;

    if(user_name_len > 0)
    {
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(user_name_len);		//user_name length MSB
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(user_name_len);    	//user_name length LSB
        memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],user_name,user_name_len);
        g_mqtt_tx_len += user_name_len;
    }

    if(password_len > 0)
    {
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(password_len);		//password length MSB
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(password_len);    	//password length LSB
        memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],password,password_len);
        g_mqtt_tx_len += password_len;
    }

	
    while(cnt--)
    {
        memset((void *)g_esp8266_rx_buf,0,sizeof(g_esp8266_rx_buf));
		g_esp8266_rx_cnt=0;
		
        mqtt_send_bytes(g_esp8266_tx_buf,g_mqtt_tx_len);
		
        wait=30;//等待3s超时
		
        while(wait--)
        {
            //CONNECT
            if((g_esp8266_rx_buf[0]==g_packet_connect_ack[0]) && (g_esp8266_rx_buf[1]==g_packet_connect_ack[1])) //连接成功
            {
                return 0;//连接成功
            }
			
            delay_ms(100);
        }
    }
	
    return -1;
}

//MQTT订阅/取消订阅数据处理函数
//topic       主题
//qos         消息等级
//whether     订阅/取消订阅标志位
int32_t mqtt_subscribe_topic(char *topic,uint8_t qos,uint8_t whether)
{
    
	
    uint32_t cnt=2;
    uint32_t wait;
	
    int32_t topiclen = strlen(topic);

    int32_t data_len = 2 + (topiclen+2) + (whether?1:0);//可变报头的长度(2字节)+有效载荷的长度
	
	g_mqtt_tx_len=0;
	
    //固定报头
    //控制报文类型
    if(whether) 
		g_esp8266_tx_buf[g_mqtt_tx_len++] = 0x82; // 消息类型和标志位
    else	
		g_esp8266_tx_buf[g_mqtt_tx_len++] = 0xA2; // 获取剩余长度

    //剩余长度
    do
    {
        uint8_t encodedByte = data_len % 128;
        data_len = data_len / 128;
        // if there are more data to encode, set the top bit of this byte
        if ( data_len > 0 )
            encodedByte = encodedByte | 128;
        g_esp8266_tx_buf[g_mqtt_tx_len++] = encodedByte;
    } while ( data_len > 0 );

    //可变报头 — 每次调用使用不同包标识符
    { static uint16_t sub_pkt_id = 1;
    g_esp8266_tx_buf[g_mqtt_tx_len++] = (sub_pkt_id >> 8) & 0xFF;
    g_esp8266_tx_buf[g_mqtt_tx_len++] = sub_pkt_id & 0xFF;
    sub_pkt_id++; }
	
    //有效载荷
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(topiclen);//主题长度 MSB
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(topiclen);//主题长度 LSB
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],topic,topiclen);
	
    g_mqtt_tx_len += topiclen;

    if(whether)
    {
        g_esp8266_tx_buf[g_mqtt_tx_len++] = qos;//QoS等级
    }


    while(cnt--)
    {
		g_esp8266_rx_cnt=0;
        memset((void *)g_esp8266_rx_buf,0,sizeof(g_esp8266_rx_buf));
        mqtt_send_bytes(g_esp8266_tx_buf,g_mqtt_tx_len);
		
        wait=30;//等待3s超时
        while(wait--)
        {
            if(g_esp8266_rx_buf[0]==g_packet_sub_ack[0] && g_esp8266_rx_buf[1]==g_packet_sub_ack[1]) //订阅成功
            {
                return 0;//订阅成功
            }
            delay_ms(100);
        }
    }
	
    if(cnt) 
		return 0;	//订阅成功
	
    return -1;
}

//MQTT发布数据处理函数
//topic   主题
//message 消息
//qos     消息等级
uint16_t mqtt_publish_data(char *topic, char *message, uint8_t qos)
{
static 
	uint16_t id=0;	
    int32_t topicLength = strlen(topic);
    int32_t messageLength = strlen(message);

    int32_t data_len;
	uint8_t encodedByte;
    g_mqtt_tx_len=0;
    //有效载荷长度计算：用固定报头中的剩余长度字段的值减去可变报头的长度
    //QOS为0时没有标识符
    //数据长度             报头     报文标识符   有效载荷
    if(qos)	data_len = (2+topicLength) + 2 + messageLength;
    else	data_len = (2+topicLength) + messageLength;

    //固定报头
    //控制报文类型
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0x30;    // MQTT Message Type PUBLISH

    //剩余长度
    do
    {
        encodedByte = data_len % 128;
        data_len = data_len / 128;
        // if there are more data to encode, set the top bit of this byte
        if ( data_len > 0 )
            encodedByte = encodedByte | 128;
        g_esp8266_tx_buf[g_mqtt_tx_len++] = encodedByte;
    } while ( data_len > 0 );

    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(topicLength);//主题长度 MSB
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(topicLength);//主题长度 LSB
	
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],topic,topicLength);//复制主题
	
    g_mqtt_tx_len += topicLength;

    //报文标识符
    if(qos)
    {
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(id);
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(id);
        id++;
    }
	
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],message,messageLength);
	
    g_mqtt_tx_len += messageLength;

    mqtt_send_bytes(g_esp8266_tx_buf,g_mqtt_tx_len);
	
    return g_mqtt_tx_len;
}



/******************************************************************************
 * 函数: mqtt_report_devices_status
 * 描述: MQTT设备状态上报 - 读取DHT11实时温湿度并上报IoT平台
 * 说明: 在主循环中每~1秒调用一次(main.c mqtt_tmr>=10)
 *       上报JSON格式: {services:[{service_id:smokeDetector,
 *                       properties:{temperature, humidity}}]}
 * 参数: 无（内部调用DHT_ReadData读取温湿度）
 * 返回: 无
 ******************************************************************************/
void mqtt_report_devices_status(void)
{
    uint8_t mqtt_buf[5] = {0};
    float temperature = 0.0;
    int humidity = 0;

    if (DHT_ReadData(mqtt_buf) == 0) {
        temperature = (float)mqtt_buf[2] + (float)mqtt_buf[3] / 10.0f;
        humidity = mqtt_buf[0];
    }
	    uint16_t s_score = Snake_GetScore();
	    if (s_score > 0) {
	        sprintf(g_mqtt_msg,
	                "{\"services\":[{\n			\"service_id\":\"smokeDetector\",\n			\"properties\":{\n			\"temperature\":%.1f,\n			\"humidity\":%d,\n			\"snake_score\":%d\n			},\"event_time\":\"20260701T104112Z\"}]}",
	                temperature, humidity, s_score);
	    } else {
	        sprintf(g_mqtt_msg,
	                "{\"services\":[{\n			\"service_id\":\"smokeDetector\",\n			\"properties\":{\n			\"temperature\":%.1f,\n			\"humidity\":%d\n			},\"event_time\":\"20260701T104112Z\"}]}",
	                temperature, humidity);
	    }
    mqtt_publish_data(MQTT_PUBLISH_TOPIC, g_mqtt_msg, 0);
}

/******************************************************************************
 * 函数: mqtt_process_downlink
 * 描述: 处理华为云MQTT下行命令 - 解析PUBLISH报文提取命令字符串
 * 说明: 在主循环中周期性调用，解析g_esp8266_rx_buf中的MQTT PUBLISH数据包，
 *       将命令负载复制到uart_buf交由parse_cmd()统一处理
 * 注意: 仅当uart_flag==0时写入，避免与串口命令冲突
 ******************************************************************************/
void mqtt_process_downlink(void)
{
    static uint32_t scan_idx = 0;
    uint32_t cnt = g_esp8266_rx_cnt;
    uint8_t *buf = (uint8_t *)g_esp8266_rx_buf;
    uint32_t pos, remaining, mult, rl_bytes, packet_end, plen;
    uint16_t topic_len;
    uint8_t digit, qos;

    /* 缓冲区被外部重置过，复位扫描位置 */
    if (cnt < scan_idx || cnt > sizeof(g_esp8266_rx_buf)) {
        scan_idx = 0;
        return;
    }

    while (scan_idx + 4 < cnt) {
        /* 查找MQTT PUBLISH报文（类型0x30） */
        if ((buf[scan_idx] & 0xF0) != 0x30) { scan_idx++; continue; }

        pos = scan_idx + 1;
        remaining = 0; mult = 1;
        rl_bytes = 0;

        /* 解码剩余长度（变长编码） */
        do {
            if (pos >= cnt) return;
            digit = buf[pos++];
            remaining += (digit & 0x7F) * mult;
            mult *= 128;
            rl_bytes++;
            if (rl_bytes > 4) { scan_idx = pos; break; }
        } while (digit & 0x80);
        if (rl_bytes > 4) continue;

        packet_end = pos + remaining;
        if (packet_end > cnt) return;  /* 报文不完整，等下次 */

        /* 解析可变报头：Topic长度(2B) + Topic */
        if (pos + 2 > packet_end) { scan_idx = packet_end; continue; }
        topic_len = (buf[pos] << 8) | buf[pos + 1];
        pos += 2 + topic_len;
        if (pos > packet_end) { scan_idx = packet_end; continue; }

        /* 跳过报文标识符（QoS > 0 时存在） */
        qos = (buf[scan_idx] & 0x06) >> 1;
        if (qos > 0) {
            if (pos + 2 > packet_end) { scan_idx = packet_end; continue; }
            pos += 2;
        }

        /* 提取负载（即命令字符串） */
        plen = packet_end - pos;
        if (plen > 0 && plen < 199) {
            if (uart_flag == 0) {  /* 串口命令缓冲区空闲 */
                memcpy(uart_buf, &buf[pos], plen);
                uart_buf[plen] = '\0';
                uart_cnt = plen;
                uart_flag = 1;
            }
        }

        scan_idx = packet_end;  /* 跳过已处理的报文 */
    }

    /* 防止scan_idx落后太多导致溢出 */
    if (cnt > scan_idx + 512)
        scan_idx = cnt > 256 ? cnt - 256 : 0;
}

/******************************************************************************
 * 函数: mqtt_process_command
 * 描述: 解析华为云命令下发 — 从PUBLISH报文提取command_name + request_id
 *       执行命令后向响应topic回 {"result_code":0}
 * 说明: 与mqtt_process_downlink类似但需要解析JSON和提取topic中的request_id
 ******************************************************************************/
void mqtt_process_command(void)
{
    static uint32_t scan_idx = 0;
    uint32_t cnt = g_esp8266_rx_cnt;
    uint8_t *buf = (uint8_t *)g_esp8266_rx_buf;
    uint32_t pos, remaining, mult, rl_bytes, packet_end, plen;
    uint16_t topic_len;
    uint8_t digit, qos;
    const char *cmd_name, *rid_start;
    char resp_topic[160];
    uint8_t i;

    if (cnt < scan_idx || cnt > sizeof(g_esp8266_rx_buf)) { scan_idx = 0; return; }

    while (scan_idx + 4 < cnt) {
        if ((buf[scan_idx] & 0xF0) != 0x30) { scan_idx++; continue; }

        pos = scan_idx + 1;
        remaining = 0; mult = 1; rl_bytes = 0;
        do {
            if (pos >= cnt) return;
            digit = buf[pos++];
            remaining += (digit & 0x7F) * mult;
            mult *= 128;
            rl_bytes++;
            if (rl_bytes > 4) { scan_idx = pos; break; }
        } while (digit & 0x80);
        if (rl_bytes > 4) continue;

        packet_end = pos + remaining;
        if (packet_end > cnt) return;

        /* 提取topic */
        if (pos + 2 > packet_end) { scan_idx = packet_end; continue; }
        topic_len = (buf[pos] << 8) | buf[pos + 1];
        pos += 2;
        if (pos + topic_len > packet_end) { scan_idx = packet_end; continue; }

        /* 只处理命令下发 topic (含 commands/request_id=) */
        if (!strstr((const char *)&buf[pos], "commands/request_id=")) {
            scan_idx = packet_end;
            continue;
        }

        /* 提取request_id (位于"request_id="之后) */
        rid_start = strstr((const char *)&buf[pos], "request_id=");
        if (!rid_start) { scan_idx = packet_end; continue; }
        rid_start += 11; /* 跳过"request_id=" */
        {
            char req_id[64];
            char json[256];
            uint8_t ri = 0;

            while (rid_start[ri] && rid_start[ri] != '/' && ri < 63)
                { req_id[ri] = rid_start[ri]; ri++; }
            req_id[ri] = '\0';

            /* 跳过topic */
            pos += topic_len;

            /* 跳过报文标识符 (QoS>0) */
            qos = (buf[scan_idx] & 0x06) >> 1;
            if (qos > 0) { if (pos + 2 > packet_end) { scan_idx = packet_end; continue; } pos += 2; }

            /* 提取payload (JSON) */
            plen = packet_end - pos;
            if (plen == 0 || plen > 200) { scan_idx = packet_end; continue; }

            memcpy(json, &buf[pos], plen);
            json[plen] = '\0';

            /* 解析 "command_name":"xxx" */
            cmd_name = strstr(json, "\"command_name\":\"");
            if (cmd_name) {
                cmd_name += 16; /* 跳过 "command_name":" */
                char cmd[32];
                uint8_t ci = 0;
                while (cmd_name[ci] && cmd_name[ci] != '"' && ci < 31)
                    { cmd[ci] = cmd_name[ci]; ci++; }
                cmd[ci] = '\0';

                /* 执行命令 — 复用现有逻辑，通过uart_buf中转 */
                if (cmd[0]) {
                    if (uart_flag == 0) {
                        for (i = 0; cmd[i] && i < 199; i++)
                            uart_buf[i] = (u8)cmd[i];
                        uart_buf[i] = '\0';
                        uart_cnt = i;
                        uart_flag = 1;
                    }
                }
            }

            /* 发送响应: topic = prefix + request_id */
            resp_topic[0] = '\0';
            {
                uint8_t pi;
                for (pi = 0; MQTT_CMD_RESP_TOPIC[pi]; pi++) resp_topic[pi] = MQTT_CMD_RESP_TOPIC[pi];
                for (i = 0; req_id[i]; i++) resp_topic[pi++] = req_id[i];
                resp_topic[pi] = '\0';
            }
            mqtt_publish_data(resp_topic, "{\"result_code\":0}", 0);
        }

        scan_idx = packet_end;
    }

    if (cnt > scan_idx + 512)
        scan_idx = cnt > 256 ? cnt - 256 : 0;
}

static void anim_wait(uint16_t ms)
{
    static const unsigned char *frames[] = {wifi1_bmp, wifi2_bmp, wifi3_bmp, wifi4_bmp};
    static uint8_t f = 0;
    uint16_t step = 250;
    for (uint16_t t = 0; t < ms; t += step) {
        OLED_Clear();
        OLED_ShowPicture(0, 0, 128, 64, (const u8 *)frames[f], 1);
        OLED_Refresh();
        delay_ms(step);
        if (++f > 3) f = 0;
    }
}

/******************************************************************************
 * 辅助: 在RX缓冲区中查找字符串，同时播放全屏WiFi动画
 * 返回: 0=找到, -1=超时
 ******************************************************************************/
static int32_t find_with_anim(const char *str, uint32_t timeout_ms)
{
    static const unsigned char *wf[] = {wifi1_bmp, wifi2_bmp, wifi3_bmp, wifi4_bmp};
    static uint8_t fi = 0;
    uint32_t t = 0;

    while (t < timeout_ms) {
        if (strstr((const char *)g_esp8266_rx_buf, str))
            return 0;
        if ((t % 250) == 0) {
            OLED_Clear();
            OLED_ShowPicture(0, 0, 128, 64, (const u8 *)wf[fi], 1);
            OLED_Refresh();
            if (++fi > 3) fi = 0;
        }
        delay_ms(1);
        t++;
    }
    return -1;
}

/******************************************************************************
 * 函数: esp8266_mqtt_init_animated
 * 描述: WiFi+MQTT连接初始化 — 全屏动画循环播放，连上显示Success
 * 说明: 优化流程 — 先检查ESP是否已有WiFi连接（STM32软复位时ESP不断电），
 *       有则跳过复位和连AP，直接走TCP连接；无则完整走复位→连AP→TCP
 ******************************************************************************/
int32_t esp8266_mqtt_init_animated(void)
{
    int32_t rt;
    int need_full_init = 1;

    USART2_SendString("init start\r\n");
    esp8266_init();

    /* 退出透传(如果处于透传模式) */
    rt = esp8266_exit_transparent_transmission();
    if (rt) { USART2_SendString("exit_transparent fail\r\n"); return -1; }

    /* 检测ESP是否已有WiFi连接（STM32软复位时ESP不断电） */
    esp8266_send_at("AT+CIFSR\r\n");
    if (esp8266_find_str_in_rx_packet("OK", 2000) == 0) {
        if (strstr((const char *)g_esp8266_rx_buf, "STAIP")) {
            USART2_SendString("WiFi OK, skip AP\r\n");
            need_full_init = 0;
        }
    }

    if (need_full_init) {
        /* 复位ESP8266后完整初始化 */
        esp8266_send_at("AT+RST\r\n");
        anim_wait(2000);

        /* 关闭回显 */
        rt = esp8266_enable_echo(0);
        if (rt) { USART2_SendString("ATE0 fail\r\n"); return -2; }

        /* 连接AP热点 */
        rt = esp8266_connect_ap(g_wifi_ssid, g_wifi_pass);
        if (rt) { USART2_SendString("AP fail\r\n"); return -3; }
        anim_wait(1000);
    }

    /* TCP连接华为云 */
    rt = esp8266_connect_server("TCP", MQTT_BROKERADDRESS, 1883);
    if (rt) { USART2_SendString("TCP fail\r\n"); return -4; }
    anim_wait(1000);

    /* 进入透传 */
    rt = esp8266_entry_transparent_transmission();
    if (rt) { USART2_SendString("transparent fail\r\n"); return -5; }

    /* MQTT连接 */
    if (mqtt_connect(MQTT_CLIENTID, MQTT_USARNAME, MQTT_PASSWD)) {
        USART2_SendString("MQTT fail\r\n");
        return -6;
    }

    /* 订阅消息下行 + 命令下发 */
    if (mqtt_subscribe_topic(MQTT_SUBSCRIBE_TOPIC, 0, 1)) {
        USART2_SendString("MsgSub fail\r\n");
        return -6;
    }
    if (mqtt_subscribe_topic(MQTT_COMMAND_TOPIC, 0, 1)) {
        USART2_SendString("CmdSub fail\r\n");
        return -6;
    }

    /* 通知WiFi模块连接成功 */
    g_wifi_connected = 1;

    /* 成功 */
    OLED_Clear();
    OLED_ShowString(40, 28, (const u8 *)"Success", 2);
    OLED_Refresh();
    USART2_SendString("All OK!\r\n");
    return 0;
}


int32_t esp8266_mqtt_init(void)
{
	int32_t rt;

	//esp8266初始化
	esp8266_init();


	//退出透传模式,以便发送AT指令
	rt=esp8266_exit_transparent_transmission();
	
	if(rt)
	{
		printf("esp8266_exit_transparent_transmission fail\r\n");
		return -1;
	}	
	printf("esp8266_exit_transparent_transmission success\r\n");
	delay_ms(2000);
	
	esp8266_send_at("AT+RST\r\n");
	//关闭回显
	rt=esp8266_enable_echo(0);
	
	if(rt)
	{
		printf("esp8266_enable_echo fail\r\n");
		return -2;
	}	
	printf("esp8266_enable_echo success\r\n");
	delay_ms(2000);	
		
	//连接AP热点
	rt = esp8266_connect_ap(g_wifi_ssid, g_wifi_pass);
	if(rt)
	{
		printf("esp8266_connect_ap fail\r\n");
		return -3;
	}	
	printf("esp8266_connect_ap success\r\n");
	
	//AP连接后适当延时,再发送下一个AT指令
	delay_ms(2000);
	
	rt =esp8266_connect_server("TCP",MQTT_BROKERADDRESS,1883);
	if(rt)
	{
		printf("esp8266_connect_server fail\r\n");
		return -4;
	}	
	printf("esp8266_connect_server success\r\n");
	delay_ms(2000);
	
	//进入透传模式
	rt =esp8266_entry_transparent_transmission();
	
	if(rt)
	{
		printf("esp8266_entry_transparent_transmission fail\r\n");
		return -5;
	}	
	printf("esp8266_entry_transparent_transmission success\r\n");
	delay_ms(2000);
	if(mqtt_connect(MQTT_CLIENTID, MQTT_USARNAME, MQTT_PASSWD))
	{
		printf("mqtt_connect fail\r\n");
		return -5;	
	
	}
	printf("mqtt_connect success\r\n");
	delay_ms(2000);		
	if(mqtt_subscribe_topic(MQTT_SUBSCRIBE_TOPIC,0,1))
	{
		printf("mqtt_subscribe_topic fail\r\n");
		return -6;
	}	
	printf("mqtt_subscribe_topic success\r\n");

	if(mqtt_subscribe_topic(MQTT_COMMAND_TOPIC,0,1))
	{
		printf("mqtt_subscribe_command_topic fail\r\n");
		return -7;
	}
	printf("mqtt_subscribe_command_topic success\r\n");
	return 0;
		
}
