/******************************************************************************
 * 文件名: ui.h
 * 描  述: 用户界面模块头文件
 * 说  明: 封装OLED显示的人脸表情、温湿度信息、开机画面等功能
 *         上层应用只需调用UI_*接口，无需直接操作OLED驱动
 ******************************************************************************/

#ifndef __UI_H
#define __UI_H

#include "sys.h"

/* 表情模式定义 */
#define FACE_NORMAL  0    /* 正常笑脸 — 抛物线嘴角上扬 */
#define FACE_ALERT   1    /* 避障警戒 — 方形嘴（边框描点） */
#define FACE_SAD     2    /* 撞墙/卡住 — 抛物线嘴角下垂 */

/**
 * @brief  界面初始化
 * @note   初始化OLED，显示Creeper开机画面（5秒），之后切回正常表情
 *         - 调用 OLED_Init() 初始化OLED硬件
 *         - 调用 OLED_ShowImage() 显示64x64的Creeper位图
 *         - delay_ms(5000) 保持开机画面5秒
 *         - Face_Set(FACE_NORMAL) 恢复默认表情
 */
void UI_Init(void);

/**
 * @brief  界面定时刷新任务（主循环中周期调用）
 * @note   若菜单处于激活状态则调用 Menu_Task() 并返回；
 *         否则调用 Face_Task() 刷新表情、眨眼动画和温湿度显示
 */
void UI_Task(void);

/**
 * @brief  设置表情模式
 * @param  mode 表情模式：FACE_NORMAL / FACE_ALERT / FACE_SAD
 * @note   委托给 Face_Set()，实际写入全局 face_expr 变量
 */
void UI_SetMode(u8 mode);

/**
 * @brief  更新温湿度显示数据
 * @param  ti 温度整数部分（如 25）
 * @param  td 温度小数部分（如 3 → 显示为 25.3C）
 * @param  hi 湿度整数部分（如 60）
 * @param  hd 湿度小数部分（当前未使用，仅保留占位）
 * @note   数据由 DHT11 读取后调用，委托给 OLED_UpdateTH()
 */
void UI_UpdateTH(u8 ti, u8 td, u8 hi, u8 hd);

/**
 * @brief  在OLED底部显示临时状态消息
 * @param  str 消息字符串（最长21字符，超长截断）
 * @note   消息持续约25个刷新周期后自动消失；底层调用 Face_ShowMsg()
 *         消息显示在屏幕底部第54行
 */
void UI_ShowMsg(const char *str);

/**
 * @brief  全屏显示UI2测试位图（含圆形边框画面，5秒后自动退出）
 * @note   用于显示效果验证，调用后阻塞5秒
 *         使用 OLED_ShowPicture() 显示 ui2_bmp 位图
 */
void UI_ShowTest(void);

/**
 * @brief  WiFi信号强度动画循环播放
 * @param  cycles 动画重复次数（每次播放4帧）
 * @param  delay  帧间延时(ms)，建议300~500ms
 * @note   依次显示 wifi1~wifi4 四帧位图模拟信号由弱到强增长
 *         动画执行期间阻塞主循环，结束后由 Face_Task 恢复小脸显示
 *         例：UI_ShowWiFi(3, 400) → 播放3轮，每帧停留400ms
 */
void UI_ShowWiFi(u8 cycles, u16 delay);

#endif
