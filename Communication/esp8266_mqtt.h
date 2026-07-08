#ifndef __ES8266_MQTT_H
#define __ES8266_MQTT_H

#include "stm32f10x.h"


/******************************************************************************
 * 华为云 IoT 平台配置
 * 说明：以下配置需替换为在华为云IoT平台上注册设备时获取的实际参数
 *       （平台地址、设备ID、密钥等）
 ******************************************************************************/

/**
 * @def MQTT_BROKERADDRESS
 * @brief 华为云IoT平台MQTT Broker地址（设备接入域名）
 *        格式为：{project_id}.iotda-device.{region}.myhuaweicloud.com
 * @note  当前区域为华南-广州（cn-south-4）
 *        端口固定为1883（非加密MQTT）
 */
#define MQTT_BROKERADDRESS      "c175d9a296.iotda-device.cn-south-4.myhuaweicloud.com"

/**
 * @def MQTT_CLIENTID
 * @brief MQTT客户端ID，用于标识设备与云平台的连接
 *        格式：{device_id}_{node_id}_{pwd_type}_{timestamp}_{ext_info}
 *        华为云MQTT连接时使用此ID标识设备身份
 */
#define MQTT_CLIENTID           "6a4b65fa6b6c4d5f8d66e99f_myNodeId_0_0_2026070608"

/**
 * @def MQTT_USARNAME
 * @brief MQTT连接用户名，对应华为云IoT平台的设备ID
 *        格式：{device_id}_{node_id}
 */
#define MQTT_USARNAME           "6a4b65fa6b6c4d5f8d66e99f_myNodeId"

/**
 * @def MQTT_PASSWD
 * @brief MQTT连接密码，华为云IoT平台生成的设备密钥哈希值
 *        用于设备接入认证，确保只有合法设备可连接平台
 */
#define MQTT_PASSWD             "618780b5e7ce11d0d3d473994d86fe605afdc7f0446b69ac70b5c68f15bcc1f8"

/**
 * @def MQTT_DEVICE_ID
 * @brief 华为云设备ID（从MQTT_USARNAME提取）
 *        用于构造Topic中的设备路径
 */
#define MQTT_DEVICE_ID          "6a4b65fa6b6c4d5f8d66e99f_myNodeId"


/******************************************************************************
 * MQTT Topic 定义
 * 说明：华为云IoT平台使用$oc/devices/{device_id}/sys/ 作为系统Topic前缀
 *       设备通过发布/订阅这些Topic与平台通信
 ******************************************************************************/

/**
 * @def MQTT_PUBLISH_TOPIC
 * @brief 设备属性上报Topic
 *        设备向此Topic发布JSON格式的属性数据（温湿度、游戏分数等）
 *        平台收到后转发给应用侧
 *        格式：$oc/devices/{device_id}/sys/properties/report
 */
#define MQTT_PUBLISH_TOPIC      "$oc/devices/" MQTT_DEVICE_ID "/sys/properties/report"

/**
 * @def MQTT_SUBSCRIBE_TOPIC
 * @brief 消息下行Topic（设备订阅）
 *        平台通过此Topic向设备下发消息
 *        格式：$oc/devices/{device_id}/sys/messages/down
 */
#define MQTT_SUBSCRIBE_TOPIC    "$oc/devices/" MQTT_DEVICE_ID "/sys/messages/down"

/**
 * @def MQTT_COMMAND_TOPIC
 * @brief 平台命令下发Topic（设备订阅）
 *        平台通过此Topic向设备下发控制命令（如转向、停止等）
 *        使用通配符#匹配所有命令ID
 *        格式：$oc/devices/{device_id}/sys/commands/#
 */
#define MQTT_COMMAND_TOPIC      "$oc/devices/" MQTT_DEVICE_ID "/sys/commands/#"

/**
 * @def MQTT_CMD_RESP_TOPIC
 * @brief 命令响应Topic前缀
 *        设备执行命令后向此Topic发送响应结果（result_code）
 *        完整的Topic需要附加request_id={request_id}
 *        格式：$oc/devices/{device_id}/sys/commands/response/request_id=
 */
#define MQTT_CMD_RESP_TOPIC     "$oc/devices/" MQTT_DEVICE_ID "/sys/commands/response/request_id="


/******************************************************************************
 * 字节提取宏
 * 说明：用于将16位/32位变量按字节拆分，方便MQTT协议的大端字节序组包
 ******************************************************************************/

/** @brief 提取uint16/uint32变量的低字节（Byte 0） */
#define BYTE0(dwTemp)       (*( char *)(&dwTemp))

/** @brief 提取uint16/uint32变量的第1字节（Byte 1） */
#define BYTE1(dwTemp)       (*((char *)(&dwTemp) + 1))

/** @brief 提取uint32变量的第2字节（Byte 2） */
#define BYTE2(dwTemp)       (*((char *)(&dwTemp) + 2))

/** @brief 提取uint32变量的第3字节（Byte 3） */
#define BYTE3(dwTemp)       (*((char *)(&dwTemp) + 3))


/******************************************************************************
 * MQTT 功能函数声明
 ******************************************************************************/

/**
 * @brief   建立MQTT连接（发送MQTT CONNECT报文到华为云平台）
 * @details 按照MQTT 3.1.1协议构造CONNECT报文，包含可变报头（协议名/版本/
 *          连接标志/心跳间隔）和有效载荷（Client ID/用户名/密码）
 *          使用g_esp8266_tx_buf作为报文缓冲区，通过透传通道发送
 * @param   client_id - 设备客户端ID（对应MQTT_CLIENTID）
 * @param   user_name - MQTT用户名（对应MQTT_USARNAME）
 * @param   password  - MQTT密码（对应MQTT_PASSWD）
 * @return  0  - MQTT连接成功（收到CONNACK报文，字节0x20 0x02）
 *          -1 - 连接失败（重试2次后仍未收到CONNACK，共等待约6秒）
 * @note    调用此函数前需确保已进入透传模式
 * @note    心跳间隔设置为60秒
 */
extern int32_t mqtt_connect(char *client_id, char *user_name, char *password);

/**
 * @brief   订阅或取消订阅MQTT主题
 * @details 构造MQTT SUBSCRIBE/UNSUBSCRIBE报文并发送
 *          订阅时使用报文类型0x82，取消订阅使用0xA2
 *          每次调用使用递增的报文标识符
 * @param   topic   - 要订阅/取消订阅的主题字符串
 * @param   qos     - 服务质量等级（0/1/2），通常使用0（最多一次）
 * @param   whether - 1=订阅，0=取消订阅
 * @return  0  - 订阅成功（收到SUBACK报文，字节0x90 0x03）
 *          -1 - 订阅失败（重试2次后超时）
 */
extern int32_t mqtt_subscribe_topic(char *topic, uint8_t qos, uint8_t whether);

/**
 * @brief   发布MQTT消息
 * @details 构造MQTT PUBLISH报文（类型0x30）并发送到指定主题
 *          支持QoS 0/1，QoS 1时包含2字节报文标识符
 * @param   topic   - 目标发布主题
 * @param   message - 消息内容（字符串）
 * @param   qos     - 服务质量等级（0=最多一次，1=至少一次）
 * @return  发送的报文总长度（字节数）
 * @note    此函数为"发后即忘"模式，不等待PUBACK确认
 */
extern uint16_t mqtt_publish_data(char *topic, char *message, uint8_t qos);

/**
 * @brief   发送MQTT心跳包（PINGREQ）
 * @details 发送类型0xC0的空报文，维持与MQTT Broker的长连接
 *          华为云心跳保活建议间隔为KeepAlive时间的1/2（即30秒）
 * @param   无
 * @return  无
 */
extern void mqtt_send_heart(void);

/**
 * @brief   WiFi + MQTT 初始化（无动画版本）
 * @details 依次执行：ESP8266初始化 -> 退出透传 -> 复位 -> 关闭回显 ->
 *          连接AP -> 连接TCP服务器 -> 进入透传 -> MQTT CONNECT ->
 *          订阅消息下行Topic和命令下发Topic
 * @param   无
 * @return  0  - 全部初始化成功
 *          -1~-7 - 各步骤对应的错误码
 */
extern int32_t esp8266_mqtt_init(void);

/**
 * @brief   WiFi + MQTT 初始化（带动画版本）
 * @details 与esp8266_mqtt_init功能相同，但初始化过程中OLED播放WiFi连接动画
 *          连接成功后显示"Success"画面
 *          如果检测到已连接WiFi（通过AT+CIFSR检查STAIP），跳过AP连接步骤
 * @param   无
 * @return  0  - 全部初始化成功
 *          -1~-6 - 各步骤对应的错误码
 */
extern int32_t esp8266_mqtt_init_animated(void);

/**
 * @brief   上报设备状态到华为云平台
 * @details 读取DHT11温湿度传感器数据，构造JSON格式的properties报文，
 *          通过mqtt_publish_data发布到MQTT_PUBLISH_TOPIC
 *          同时附带贪吃蛇游戏得分（snake_score > 0时）
 *          在主循环中大约每1秒调用一次
 * @param   无
 * @return  无
 */
extern void mqtt_report_devices_status(void);

/**
 * @brief   处理MQTT下行消息
 * @details 扫描g_esp8266_rx_buf缓冲区，查找MQTT PUBLISH报文（类型0x30）
 *          解析可变报头获取Topic长度和负载数据
 *          将命令字符串复制到uart_buf，交由parse_cmd()统一处理
 * @param   无
 * @return  无
 * @note    仅在uart_flag==0（串口命令缓冲区空闲）时写入
 *          用static scan_idx记录扫描位置，避免重复解析
 */
extern void mqtt_process_downlink(void);

/**
 * @brief   处理MQTT平台命令下发
 * @details 解析华为云命令下发Topic（含commands/request_id=）的PUBLISH报文
 *          提取command_name和request_id
 *          将命令通过uart_buf中转执行，执行完成后回响应
 *          响应内容：{"result_code":0}
 * @param   无
 * @return  无
 */
extern void mqtt_process_command(void);

#endif
