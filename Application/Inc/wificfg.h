#ifndef __WIFICFG_H
#define __WIFICFG_H

#include "sys.h"

/**
 * @defgroup WiFi配置全局变量
 * @brief   供OLED显示及其他模块读取的WiFi凭据
 * @{
 */
extern char g_wifi_ssid[32];        /* WiFi SSID字符串，当前使用的热点名称 */
extern char g_wifi_pass[64];        /* WiFi密码字符串 */
extern uint8_t g_wifi_connected;    /* WiFi连接状态：1=已连接(已验证)，0=未连接/断开 */
/** @} */

/**
 * @defgroup WiFi配置API
 * @brief   SSID/密码设置、连接触发、周期性检测与自动重连
 * @{
 */

/**
 * @brief  设置WiFi SSID并保存到Flash
 * @param  ssid  SSID字符串（串口接收，遇到'*'或'\\0'自动截断）
 */
void WifiCfg_SetSSID(const char *ssid);

/**
 * @brief  设置WiFi密码并保存到Flash
 * @param  pass  密码字符串（串口接收，遇到'*'或'\\0'自动截断）
 */
void WifiCfg_SetPass(const char *pass);

/**
 * @brief  发起WiFi连接（含OLED动画提示和MQTT初始化）
 * @note   内部状态机：ST_IDLE->ST_CONNECT->ST_DONE。
 *         连接成功后g_wifi_connected置1并再次保存Flash。
 */
void WifiCfg_Connect(void);

/**
 * @brief  WiFi配置任务（主循环中周期性调用，约500ms周期）
 * @note   首次调用自动从Flash加载配置；
 *         检查reconnect_pending标志触发重连；
 *         每500周期检测WiFi状态，断开自动重连；
 *         透传模式下跳过检测。
 */
void WifiCfg_Task(void);

/**
 * @brief  设置断网重连待处理标志
 * @note   外部模块（如MQTT心跳超时）调用此函数通知WiFi模块执行重连。
 *         WifiCfg_Task在下一个调度周期检测到此标志后自动发起连接。
 */
void WifiCfg_SetReconnect(void);

/** @} */

#endif
