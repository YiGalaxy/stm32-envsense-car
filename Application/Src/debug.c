/******************************************************************************
 * @file    debug.c
 * @brief   蓝牙串口调试模块
 *          实现双通道输出(USART1 有线 + USART2 蓝牙)，
 *          提供 help/status/sensors/ping/uptime/reset/echo/debug 等命令，
 *          支持周期自动报告模式(debug on)和 DHT11 传感器数据缓存
 * @note    控制命令(goA/stop/menu 等)由外部 motor 模块解析，
 *          本模块只处理查询类命令
 ******************************************************************************/
#include "debug.h"
#include "usart.h"
#include "motor.h"
#include "wificfg.h"
#include "sr04.h"
#include "dht11.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

/******************************************************************************
 * @brief  双通道字符串输出
 *         同时将字符串发送到 USART1(有线串口) 和 USART2(蓝牙模块)
 * @param  s  待发送的字符串指针(以 '\0' 结尾)
 * @note    内部静态函数，仅本文件使用；
 *          这是本模块所有输出命令的统一出口
 ******************************************************************************/
static void dbg_out(const char *s)
{
    USART1_SendString((char *)s);   /* 输出到有线串口(调试/上位机) */
    USART2_SendString((char *)s);   /* 同时输出到蓝牙模块(手机端) */
}

/* ---------- 全局/静态变量 ---------- */

/**
 * @brief 调试模式使能标志
 *        0: 关闭周期自动报告; 1: 开启(~1.5 秒间隔输出状态)
 */
static uint8_t debug_on = 0;

/**
 * @brief 周期输出定时器计数器
 *        每调用 Debug_Task() 递增一次，达到阈值(=15)时输出一次
 */
static uint16_t dbg_tmr = 0;

/**
 * @brief 主循环计数器
 *        ~10Hz 递增(由 Debug_Task() 在 main loop 中调用时更新)
 *        用于计算系统运行时间(秒 = sys_tick / 10)
 */
static uint32_t sys_tick = 0;

/**
 * @brief DHT11 温湿度传感器读取缓存
 *        last_temp_i/last_temp_d: 温度整数部分/小数部分
 *        last_humi_i/last_humi_d: 湿度整数部分/小数部分
 * @note  由 read_sensors() 更新，避免每次输出都重新读取传感器
 */
static uint8_t last_temp_i = 0, last_temp_d = 0;
static uint8_t last_humi_i = 0, last_humi_d = 0;

/* ---------- 外部引用 ---------- */
extern volatile uint16_t speed;     /* 当前电机速度值，定义于 motor.c */
extern uint8_t work_mode;           /* 当前工作模式(0=遥控/1=自动/2=跟随)，定义于 motor.c */

/******************************************************************************
 * @brief  从 DHT11 传感器读取温湿度数据并缓存到静态变量
 * @note    调用 DHT_ReadData() 从 DHT11 获取 5 字节原始数据
 *          数据格式(共 5 字节): 湿度整/小 温度整/小 校验和
 *          DHT11 单总线协议，每次读取间隔需 >= 1 秒
 *          读取失败时保持上次缓存值不变
 ******************************************************************************/
static void read_sensors(void)
{
    uint8_t buf[5] = {0};
    /* DHT11 原始数据格式: buf[0]=湿度整, buf[1]=湿度小, buf[2]=温度整, buf[3]=温度小, buf[4]=校验 */
    if (DHT_ReadData(buf) == 0) {           /* 读取成功(返回 0) */
        last_temp_i = (uint8_t)buf[2];      /* 温度整数部分 */
        last_temp_d = (uint8_t)buf[3];      /* 温度小数部分 */
        last_humi_i = (uint8_t)buf[0];      /* 湿度整数部分 */
        last_humi_d = (uint8_t)buf[1];      /* 湿度小数部分 */
    }
    /* 读取失败时不更新缓存，保持上次有效值 */
}

/******************************************************************************
 * @brief  命令处理: help
 *         列出所有支持的命令列表及简要说明
 * @note    包含查询类命令和外部控制命令的提示信息
 ******************************************************************************/
static void cmd_help(void)
{
    dbg_out("\r\n=== Commands ===\r\n");
    dbg_out("help          this list\r\n");
    dbg_out("status        system status\r\n");
    dbg_out("sensors       read DHT11 + distance\r\n");
    dbg_out("debug on/off  toggle auto report\r\n");
    dbg_out("ping          connectivity test\r\n");
    dbg_out("uptime        seconds since boot\r\n");
    dbg_out("echo <text>   echo back\r\n");
    dbg_out("reset         soft reset MCU\r\n");
    dbg_out("=== Control ===\r\n");
    dbg_out("goA/goB/goL/goR  move\r\n");
    dbg_out("stop             stop motors\r\n");
    dbg_out("speedup/down     +/-100\r\n");
    dbg_out("lspeedup/down    left trim +/-10\r\n");
    dbg_out("rspeedup/down    right trim +/-10\r\n");
    dbg_out("remote/auto/follow  mode\r\n");
    dbg_out("menu         enter menu\r\n");
    dbg_out("ssid <name>  set WiFi SSID\r\n");
    dbg_out("pass <pw>    set WiFi password\r\n");
    dbg_out("connect      reconnect WiFi\r\n");
    dbg_out("====================\r\n");
}

/******************************************************************************
 * @brief  命令处理: status
 *         输出当前系统运行状态摘要
 * @note    包含: 工作模式、速度、左右轮修正量、超声波距离、
 *          WiFi 连接状态、调试模式状态、运行时间
 ******************************************************************************/
static void cmd_status(void)
{
    char buf[100];
    /* 工作模式字符串映射: 0=REMOTE(遥控), 1=AUTO(自动避障), 2=FOLLOW(跟随) */
    const char *mode_str[] = {"REMOTE", "AUTO", "FOLLOW"};
    uint8_t m = work_mode > 2 ? 0 : work_mode;     /* 防越界保护 */

    sprintf(buf, "\r\n=== Status ===\r\n");
    dbg_out(buf);

    sprintf(buf, "Mode:   %s\r\n", mode_str[m]);        /* 当前工作模式 */
    dbg_out(buf);
    sprintf(buf, "Speed:  %d\r\n", speed);              /* 电机PWM速度值 */
    dbg_out(buf);
    sprintf(buf, "L-Trim: %+d\r\n", left_trim);         /* 左轮修正量(纠偏) */
    dbg_out(buf);
    sprintf(buf, "R-Trim: %+d\r\n", right_trim);        /* 右轮修正量(纠偏) */
    dbg_out(buf);
    sprintf(buf, "Distance: %d cm\r\n", (int)SR04_GetDistance());   /* 超声波测距(cm) */
    dbg_out(buf);
    sprintf(buf, "WiFi:   %s (%s)\r\n", g_wifi_ssid,           /* WiFi SSID */
            g_wifi_connected ? "connected" : "disconnected");   /* WiFi 连接状态 */
    dbg_out(buf);
    sprintf(buf, "Debug:  %s\r\n", debug_on ? "ON" : "OFF");    /* 调试模式开关 */
    dbg_out(buf);
    sprintf(buf, "Uptime: %lu s\r\n", (unsigned long)(sys_tick / 10)); /* 运行秒数 */
    dbg_out(buf);
    dbg_out("================\r\n");
}

/******************************************************************************
 * @brief  命令处理: sensors
 *         实时读取并输出 DHT11 温湿度和超声波距离值
 * @note    调用 read_sensors() 触发一次新的 DHT11 读取，
 *          超声波距离通过 SR04_GetDistance() 获取
 ******************************************************************************/
static void cmd_sensors(void)
{
    char buf[80];
    read_sensors();                             /* 重新读取 DHT11 传感器 */
    uint16_t d = (uint16_t)SR04_GetDistance();  /* 读取超声波测距(cm) */

    sprintf(buf, "\r\n=== Sensors ===\r\n");
    dbg_out(buf);
    sprintf(buf, "Temp:   %d.%d C\r\n", last_temp_i, last_temp_d);   /* 温度(整数.小数) */
    dbg_out(buf);
    sprintf(buf, "Humi:   %d.%d %%RH\r\n", last_humi_i, last_humi_d); /* 湿度(整数.小数) */
    dbg_out(buf);
    sprintf(buf, "Dist:   %d cm\r\n", d);       /* 距离 */
    dbg_out(buf);
    dbg_out("===============\r\n");
}

/******************************************************************************
 * @brief  命令处理: echo
 *         将用户输入的参数原样返回(环回测试)
 * @param  args  用户输入的命令参数(echo 之后的文本)
 * @note    用于测试串口通信是否正常
 ******************************************************************************/
static void cmd_echo(const char *args)
{
    dbg_out("Echo: ");
    dbg_out((char *)args);      /* 原样返回参数 */
    dbg_out("\r\n");
}

/******************************************************************************
 * @brief  命令处理: debug on|off
 *         开启或关闭周期自动报告模式
 * @param  args  命令参数，包含 "on" 或 "off"
 * @note    开启时重置 dbg_tmr 定时器使首次输出立即生效；
 *          周期报告间隔约 1.5 秒(Debug_Task 被 ~10Hz 调用，
 *          每 15 次触发一次输出)
 ******************************************************************************/
static void cmd_debug(const char *args)
{
    if (strstr(args, "on")) {           /* 开启周期报告 */
        debug_on = 1;
        dbg_tmr = 0;                    /* 重置定时器，使首次输出不等待 */
        dbg_out("Debug ON\r\n");
    } else if (strstr(args, "off")) {   /* 关闭周期报告 */
        debug_on = 0;
        dbg_out("Debug OFF\r\n");
    } else {
        dbg_out("Usage: debug on|off\r\n");
    }
}

/******************************************************************************
 * @brief  调试命令入口：解析并派发命令到对应的处理函数
 * @param  buf  从串口接收到的完整命令字符串
 * @return int8_t  0 = 命令已由本模块处理；
 *                 -1 = 未识别的命令(由外部 motor 模块继续解析)
 * @note    本模块处理: help/status/sensors/echo/debug/ping/uptime/reset
 *          未识别命令返回 -1 供外部调用者继续派发(如 goA/stop/menu 等)
 *          命令提取: 取第一个空白字符前的单词作为命令名，之后作为参数
 ******************************************************************************/
int8_t Debug_ProcessCmd(const char *buf)
{
    /* 提取命令名 (第一个 word，遇到空格或 '\0' 或 '*' 结束) */
    char cmd[16];
    const char *args = buf;         /* args 初始指向命令开头 */
    uint8_t i = 0;
    /* 逐个字符复制到 cmd[]，直到遇到空格/结束符/通配符，最多 15 字符 */
    while (*args && *args != ' ' && *args != '*' && i < 15)
        cmd[i++] = *args++;
    cmd[i] = 0;                     /* 字符串结束符 */

    /* 跳过命令名后的空格，得到参数起始位置 */
    while (*args == ' ') args++;

    /* ---------- 查询类命令(本模块处理) ---------- */
    if (strcmp(cmd, "help") == 0)       { cmd_help(); return 0; }      /* 命令列表 */
    if (strcmp(cmd, "status") == 0)     { cmd_status(); return 0; }    /* 系统状态 */
    if (strcmp(cmd, "sensors") == 0)    { cmd_sensors(); return 0; }   /* 传感器数据 */
    if (strcmp(cmd, "echo") == 0)       { cmd_echo(args); return 0; }  /* 环回测试 */
    if (strcmp(cmd, "debug") == 0)      { cmd_debug(args); return 0; } /* 周期报告开关 */
    if (strcmp(cmd, "ping") == 0)       { dbg_out("pong\r\n"); return 0; }  /* 连通性测试 */
    if (strcmp(cmd, "uptime") == 0) {                              /* 系统运行时间 */
        char buf2[32];
        sprintf(buf2, "Uptime: %lu s\r\n", (unsigned long)(sys_tick / 10));
        dbg_out(buf2);
        return 0;
    }
    if (strcmp(cmd, "reset") == 0) {                               /* 软件复位 MCU */
        dbg_out("Resetting...\r\n");
        delay_ms(100);                  /* 等待串口数据发送完成 */
        NVIC_SystemReset();             /* 调用 CMSIS 软复位函数 */
        return 0;
    }

    return -1;  /* 未识别命令，返回 -1 供外部调用者继续处理 */
}

/******************************************************************************
 * @brief  调试周期性任务
 *         在 main loop 中以 ~10Hz 频率调用
 * @note    功能:
 *          - 递增 sys_tick(主循环计数器)
 *          - 当 debug_on = 1 时，每 15 次调用(~1.5 秒)输出一次状态报告
 *          - 状态报告格式:
 *            [D] T:xx.xC H:xx.x% D:xxxcm SPD:xxx MODE W:ON/OFF
 *          - 报告前会重新读取 DHT11 和超声波传感器
 ******************************************************************************/
void Debug_Task(void)
{
    sys_tick++;                     /* 主循环计数器递增(~10Hz) */

    if (!debug_on) return;          /* 未开启周期报告，直接返回 */

    dbg_tmr++;                      /* 周期输出定时器递增 */
    if (dbg_tmr < 15) return;       /* 未达到输出间隔(~1.5s = 15 ticks @ 10Hz) */
    dbg_tmr = 0;                    /* 达到间隔，重置定时器 */

    char buf[80];
    /* 工作模式缩写: 0=RMT(遥控), 1=AUTO(自动), 2=FOLW(跟随) */
    const char *mode_str[] = {"RMT", "AUTO", "FOLW"};
    uint8_t m = work_mode > 2 ? 0 : work_mode;  /* 防越界保护 */

    read_sensors();                             /* 刷新温湿度缓存 */
    uint16_t d = (uint16_t)SR04_GetDistance();  /* 读取超声波距离 */

    /* 格式化周期报告: [D] 温度 湿度 距离 速度 模式 WiFi状态 */
    sprintf(buf, "[D] T:%d.%dC H:%d.%d%% D:%dcm SPD:%d %s W:%s\r\n",
            last_temp_i, last_temp_d, last_humi_i, last_humi_d,
            d, speed, mode_str[m], g_wifi_connected ? "ON" : "OFF");
    dbg_out(buf);
}
