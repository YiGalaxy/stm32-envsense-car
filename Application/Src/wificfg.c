/********************************************************
 * 文件: wificfg.c
 * 描述: WiFi配置模块
 *       管理WiFi SSID/密码的Flash断电保存、连接状态机、
 *       断网自动重连，配合OLED显示WiFi连接进度。
 ********************************************************/
#include "wificfg.h"
#include "delay.h"
#include "usart.h"
#include "esp8266.h"
#include "esp8266_mqtt.h"
#include "oled.h"
#include <string.h>
#include <stdio.h>
#include "stm32f10x_flash.h"

/* ---------- Flash存储地址与布局 ---------- */
#define CFG_ADDR    0x0807F800      /* Flash最后2KB页地址，存放WiFi配置 */
#define CFG_MAGIC   0xAA55          /* 有效标志，用于判断Flash是否已写入 */

/* 存储布局说明（以半字16位为单位写入Flash，地址偏移按字节计）：
 * 偏移0x00: Magic(2B)
 * 偏移0x04: SSID(32B)
 * 偏移0x24: 密码(64B)
 * 偏移0x64: 校验和(1B)
 */
#define OFF_MAGIC   0               /* Magic标志偏移(字节) */
#define OFF_SSID    4               /* SSID起始偏移 */
#define OFF_PASS    36              /* 密码起始偏移 (4+32) */
#define OFF_SUM     100             /* 校验和偏移 (4+32+64) */

/* ---------- 连接状态机 ---------- */
#define ST_IDLE     0               /* 空闲状态：未发起连接 */
#define ST_CONNECT  1               /* 连接中：正在调用ESP8266初始化 */
#define ST_DONE     2               /* 完成状态：连接成功或失败 */

/* 内部状态机变量 */
static uint8_t state = ST_IDLE;     /* 当前连接状态 */
static uint8_t result;              /* 连接结果(0成功/非0失败)，供状态机内部传递 */

/* ---------- 全局变量 ---------- */
char g_wifi_ssid[32]  = WIFI_SSID;  /* WiFi SSID字符串，供OLED显示及其他模块读取 */
char g_wifi_pass[64]  = WIFI_PASSWORD; /* WiFi密码字符串 */
uint8_t g_wifi_connected = 0;       /* WiFi连接状态标志：1已连接，0未连接 */

/********************************************************
 * Flash 读写（内部函数）
 ********************************************************/

/**
 * @brief  计算配置数据的校验和
 * @note   对Magic(2B) + SSID(32B) + 密码(64B) 逐字节异或求和
 * @return uint8_t 校验和字节
 */
static uint8_t calc_sum(void)
{
    uint8_t s = 0;
    /* 累加Magic标志(2字节) */
    s ^= (CFG_MAGIC >> 8) & 0xFF;   /* 高字节 */
    s ^=  CFG_MAGIC & 0xFF;         /* 低字节 */
    /* flags预留字节(当前未使用，填0占位) */
    s ^= 0;
    s ^= 0;
    /* 逐字节累加SSID */
    for (uint8_t i = 0; i < 32; i++) s ^= g_wifi_ssid[i];
    /* 逐字节累加密码 */
    for (uint8_t i = 0; i < 64; i++) s ^= g_wifi_pass[i];
    return s;
}

/**
 * @brief  将当前SSID/密码保存到Flash（带校验和）
 * @note   先擦除整页再写入，按半字(16位)编程。
 *         STM32F103最后一次写入需擦除整页(2KB)，故每次保存会先擦后写。
 */
static void save_to_flash(void)
{
    /* ---- 解锁Flash控制寄存器（允许擦写操作） ---- */
    FLASH_Unlock();
    /* 清除可能残留的标志位，避免后续操作失败 */
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP |
                    FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    /* ---- 擦除目标页（整页2KB清零，必须擦除后才能写入） ---- */
    FLASH_ErasePage(CFG_ADDR);

    /* ---- 写入Magic标志：用于读取时判断Flash是否有效 ---- */
    FLASH_ProgramHalfWord(CFG_ADDR + OFF_MAGIC, CFG_MAGIC);

    /* ---- 写入SSID字符串(32B)：每2字节拼成一个半字写入 ---- */
    for (uint8_t i = 0; i < 32; i += 2) {
        uint16_t w = g_wifi_ssid[i] | (g_wifi_ssid[i+1] << 8); /* 低字节|高字节 */
        FLASH_ProgramHalfWord(CFG_ADDR + OFF_SSID + i, w);
    }

    /* ---- 写入密码字符串(64B) ---- */
    for (uint8_t i = 0; i < 64; i += 2) {
        uint16_t w = g_wifi_pass[i] | (g_wifi_pass[i+1] << 8);
        FLASH_ProgramHalfWord(CFG_ADDR + OFF_PASS + i, w);
    }

    /* ---- 写入校验和 ---- */
    FLASH_ProgramHalfWord(CFG_ADDR + OFF_SUM, calc_sum());

    /* ---- 锁定Flash（防止误写） ---- */
    FLASH_Lock();
}

/**
 * @brief  从Flash加载WiFi配置（带校验验证）
 * @note   若Magic不匹配或校验失败，保留当前默认值不变。
 *         由于Flash按半字对齐，通过指针转换直接读取。
 */
static void load_from_flash(void)
{
    const uint16_t *p = (const uint16_t *)CFG_ADDR; /* 半字指针读取Flash */

    /* ---- 检查Magic标志是否有效 ---- */
    if (p[OFF_MAGIC / 2] != CFG_MAGIC) return;  /* Flash未写入过配置，直接返回使用默认值 */

    /* ---- 从Flash读取SSID ---- */
    for (uint8_t i = 0; i < 32; i += 2) {
        uint16_t w = p[(OFF_SSID + i) / 2];
        g_wifi_ssid[i]     = w & 0xFF;           /* 取出低字节 */
        g_wifi_ssid[i + 1] = (w >> 8) & 0xFF;   /* 取出高字节 */
    }
    g_wifi_ssid[31] = 0;  /* 强制末尾置0，保证C字符串终止 */

    /* ---- 从Flash读取密码 ---- */
    for (uint8_t i = 0; i < 64; i += 2) {
        uint16_t w = p[(OFF_PASS + i) / 2];
        g_wifi_pass[i]     = w & 0xFF;
        g_wifi_pass[i + 1] = (w >> 8) & 0xFF;
    }
    g_wifi_pass[63] = 0;

    /* ---- 校验和验证：防止Flash数据损坏导致错误的配置 ---- */
    uint8_t saved = p[OFF_SUM / 2] & 0xFF;       /* 读取存储的校验和 */
    if (saved != calc_sum()) {
        /* 校验不一致，说明数据损坏，回退到默认值 */
        strcpy(g_wifi_ssid, WIFI_SSID);
        strcpy(g_wifi_pass, WIFI_PASSWORD);
    }
}

/********************************************************
 * 对外API
 ********************************************************/

/**
 * @brief  设置WiFi SSID并保存到Flash
 * @param  ssid  传入的SSID字符串（以'*'或'\\0'结束）
 * @note   最多截取31个字符，末尾补'\\0'。遇到'*'提前终止，
 *         这是为避免从串口接收时包含多余字符。
 */
void WifiCfg_SetSSID(const char *ssid)
{
    uint8_t i = 0;
    /* 逐字符复制，遇'\\0'或'*'终止，最多31字符 */
    while (*ssid && *ssid != '*' && i < 31)
        g_wifi_ssid[i++] = *ssid++;
    g_wifi_ssid[i] = 0;       /* 字符串终止符 */
    save_to_flash();          /* 写入Flash持久化 */
    USART2_SendString("SSID set OK\r\n");
}

/**
 * @brief  设置WiFi密码并保存到Flash
 * @param  pass  传入的密码字符串（以'*'或'\\0'结束）
 * @note   最多截取63个字符，末尾补'\\0'。与SetSSID逻辑一致。
 */
void WifiCfg_SetPass(const char *pass)
{
    uint8_t i = 0;
    while (*pass && *pass != '*' && i < 63)
        g_wifi_pass[i++] = *pass++;
    g_wifi_pass[i] = 0;
    save_to_flash();
    USART2_SendString("PASS set OK\r\n");
}

/**
 * @brief  发起WiFi连接（状态机驱动）
 * @note   内部状态机: ST_IDLE -> ST_CONNECT -> ST_DONE
 *         - 若已处于ST_CONNECT状态则直接返回，避免重复触发
 *         - 连接过程中通过OLED显示"Connecting WiFi..."和SSID
 *         - 调用esp8266_mqtt_init_animated()执行实际配网（含动画）
 *         - 连接成功后再次保存Flash确认持久化
 *         - 无论成败，状态最终切至ST_DONE
 */
void WifiCfg_Connect(void)
{
    if (state == ST_CONNECT) return;    /* 防止重复进入连接流程 */
    state = ST_CONNECT;                 /* 进入连接中状态 */
    g_wifi_connected = 0;              /* 先重置连接标志 */

    /* OLED提示用户正在连接 */
    OLED_Clear();
    OLED_ShowString(0, 20, (const u8 *)"Connecting WiFi...", 1);
    OLED_ShowString(0, 36, (const u8 *)g_wifi_ssid, 1);
    OLED_Refresh();

    /* 执行实际WiFi初始化（含MQTT），返回0表示成功 */
    result = esp8266_mqtt_init_animated();

    if (result == 0) {
        g_wifi_connected = 1;          /* 标记已连接 */
        save_to_flash();               /* 连接成功再保存一次，确保Flash存储 */
        OLED_Clear();
        OLED_ShowString(0, 28, (const u8 *)"WiFi OK!", 1);
        OLED_Refresh();
        delay_ms(1000);
    } else {
        OLED_Clear();
        OLED_ShowString(0, 20, (const u8 *)"WiFi FAIL!", 1);
        OLED_Refresh();
        delay_ms(1000);
    }
    state = ST_DONE;                   /* 状态机结束 */
}

/* 模块初始化标志：模块上电后首次调用WifiCfg_Task时从Flash加载配置 */
static uint8_t inited = 0;

/**
 * @brief  模块内部初始化（首次调用时从Flash加载WiFi配置）
 * @note   利用inited标志保证只执行一次，放在WifiCfg_Task中调用
 *         避免在模块启动顺序不确定时过早访问Flash
 */
static void wificfg_init(void)
{
    if (!inited) { load_from_flash(); inited = 1; }
}

/* 断网重连待处理标志：由外部模块（如MQTT心跳检测）置1，触发自动重连 */
static uint8_t reconnect_pending = 0;

/**
 * @brief  设置断网重连标志
 * @note   外部模块（如esp8266_mqtt心跳超时）在检测到WiFi断开时调用此函数，
 *         WifiCfg_Task将在下一个周期检测标志并执行重连。
 */
void WifiCfg_SetReconnect(void) { reconnect_pending = 1; }

/**
 * @brief  WiFi配置任务（需在主循环中周期性调用）
 * @note   功能：
 *         - 首次调用时初始化并从Flash加载配置
 *         - 检查断网重连标志（reconnect_pending），在非透传模式下执行重连
 *         - 每500个检测周期（约500ms，取决于主循环周期）检查WiFi连接状态
 *         - 透传模式下跳过检测，避免干扰透传数据流
 * @note   检测周期数500是基于主循环约1ms周期的经验值，即约500ms检测一次。
 *         通过g_esp8266_transparent_transmission_sta判断是否处于透传模式。
 */
void WifiCfg_Task(void)
{
    static uint16_t chk_tmr = 0;       /* 连接状态检测定时器计数器 */

    wificfg_init();                    /* 首次调用时从Flash加载配置 */

    /* ---- 断网重连处理 ---- */
    /* reconnect_pending由外部模块(如MQTT心跳)在检测到断开时置1，
     * 仅在非透传模式下执行重连，避免中途打断透传通信 */
    if (reconnect_pending && !g_esp8266_transparent_transmission_sta) {
        reconnect_pending = 0;         /* 清除待处理标志 */
        WifiCfg_Connect();             /* 执行重连 */
        return;
    }

    /* ---- 周期性连接状态检测 ---- */
    chk_tmr++;
    if (chk_tmr >= 500 && !g_esp8266_transparent_transmission_sta) {
        chk_tmr = 0;                   /* 计数器归零 */
        if (g_wifi_connected) {        /* 当前处于已连接状态，需验证连通性 */
            int32_t s = esp8266_check_status();  /* 发送AT检测ESP8266响应 */
            if (s < 0) {
                /* 检测失败，说明WiFi已断开 */
                printf("WiFi lost, reconnect...\r\n");
                g_wifi_connected = 0;
                g_esp8266_transparent_transmission_sta = 0; /* 退出透传模式 */
                WifiCfg_Connect();     /* 自动重连 */
            }
        }
    }
}
