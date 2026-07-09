# STM32 物联网环境感知智控车实训报告

---

**课程名称**：<待填写>

**专业班级**：<待填写，如：23级计算机科学与技术X班>

**设计题目**：STM32 物联网环境感知智控车

**指导老师**：<待填写>

**学生姓名**：<姓名A>

**学号**：<学号A>

**日期**：2026 年 7 月

---

## 项目成员分工

| 姓名 | 班级 | 学号 | 分工 | 分数 |
| --- | --- | --- | --- | --- |
| <姓名A> | <班级> | <学号> | ESP8266 驱动、MQTT 协议、WiFi 配置、空间扫描、自动避障状态机、跟随模式、串口通信、调试模块、主程序整合 | |
| <姓名B> | <班级> | <学号> | OLED 驱动、字库、UI 界面、UI 资源、WiFi 动画、Creeper 开机画面、菜单系统、贪吃蛇游戏 | |
| <姓名C> | <班级> | <学号> | 电机驱动、舵机驱动、超声波测距、红外避障传感器、DHT11 温湿度、SysTick 延时 | |
| <姓名D> | <班级> | <学号> | LED 指示灯、蜂鸣器、按键、系统初始化、位图资源 | |

---

## 目录

- 第一章 概述
- 第二章 相关技术介绍
- 第三章 需求分析与概要设计
- 第四章 详细设计与实现
- 第五章 总结和展望
- 第六章 致谢和总结
- 附录

---

## 第一章 概述

### 1.1 项目背景

随着物联网（IoT）技术的快速发展，智能家居、智慧城市等应用场景对设备的环境感知能力和远程控制能力提出了更高要求。环境感知智控车作为物联网技术的典型载体，集成了传感器数据采集、无线通信、运动控制和智能决策等核心技术，能够在复杂环境中实现自主导航、障碍规避和远程监控等功能。

在嵌入式系统教学领域，以 STM32 微控制器为核心的综合实训项目，能够帮助学生将理论知识与工程实践相结合。STM32F103 系列芯片基于 ARM Cortex-M3 内核，主频最高 72MHz，片内集成丰富的外设资源（GPIO、USART、I2C、SPI、定时器等），且拥有成熟的标准外设库生态，是嵌入式入门和进阶学习的首选平台。

本项目设计了一款基于 STM32F103ZET6 的物联网环境感知智控车，融合温湿度传感器（DHT11）、超声波测距模块（HC-SR04）、红外避障传感器、舵机云台（SG90）等多种传感器，通过 ESP8266 WiFi 模块接入华为云 IoT 平台，实现环境数据实时上报与远程指令下发。小车支持蓝牙遥控、自动避障和目标跟随三种工作模式，配备 128×64 OLED 显示屏提供图形化交互界面。

### 1.2 设计目的和意义

本项目的实现具有以下意义：

（1）技术整合价值：项目将多种传感器（温湿度、超声波、红外）、多种通信方式（蓝牙、WiFi、MQTT）、多种控制算法（避障状态机、空间扫描决策、目标跟随）集成于单一嵌入式平台，体现了物联网终端设备的典型架构。

（2）多传感器融合实践：超声波传感器配合舵机完成前方 180° 9 扇区空间扫描，红外传感器提供近距碰撞检测，DHT11 采集环境温湿度，多种传感器数据融合为避障决策和远程监控提供了可靠依据。

（3）多模式控制架构：小车支持蓝牙遥控、自动避障和自动跟随三种工作模式，通过全局 work_mode 变量进行模式切换和管理。自动避障采用三级递进策略（红外防撞 → 超声波空间扫描 → 试探转向/掉头），跟随模式通过超声波测距三段式控制保持 10~15cm 跟车距离。

（4）轻量级物联网通信：在资源受限的 STM32F103 平台上，采用 ESP8266 实现 MQTT 3.1.1 协议通信，不依赖 cJSON 等第三方库，通过逐字节组装 MQTT 报文完成数据上报与命令处理，有效降低内存占用。

（5）模块化软件架构：项目代码按驱动层与业务层分离，各模块职责清晰、接口明确，具有较好的可维护性和可复用性。

### 1.3 发展现状

近年来，基于 STM32 的智能小车项目在高校嵌入式教学中得到了广泛应用。在自主避障方面，常见方案包括基于超声波传感器的单点测距避障和基于红外传感器的近距离防撞。多传感器融合避障逐渐成为趋势——通过超声波获取中远距离信息、红外负责近距紧急制动，配合舵机云台实现多角度环境扫描，显著提升了避障可靠性。

在物联网接入方面，MQTT 协议因其轻量级、低带宽占用的特点，已成为 IoT 领域的首选通信协议。华为云 IoT 平台提供设备接入、数据存储和命令下发等完整功能，为嵌入式设备上云提供了便捷通道。

在人机交互方面，SSD1306 驱动的 128×64 单色 OLED 通过 I2C 接口与 MCU 通信，仅需两根信号线即可实现图形和文字的显示，适合资源受限的嵌入式应用场景。

本项目在现有研究基础之上，将多传感器融合避障、MQTT 云平台通信和 OLED 图形界面整合于同一平台，形成一套功能完整、架构清晰的物联网智控车系统。

---

## 第二章 相关技术介绍

### 2.1 编程语言和开发工具

（1）C 语言：本项目全部固件代码采用 C 语言编写。C 语言具有执行效率高、可移植性好、硬件操作能力强等优点，是嵌入式系统开发的主流语言。

（2）Keil MDK-ARM V5：ARM 公司官方推荐的 Cortex-M 系列 MCU 开发环境，集成了代码编辑、编译、调试和下载功能。编译器为 ARM Compiler V5/V6，下载调试工具为 ST-Link。

（3）SSCOM 串口调试助手：用于调试 USART 串口通信，监控蓝牙模块和 WiFi 模块的数据收发。

（4）华为云 IoT 平台：提供设备管理、数据转发和命令下发功能，使用 MQTT 协议与设备通信。

（5）蓝牙串口 APP：手机端安装 Serial Bluetooth Terminal，通过 HC-05 蓝牙模块与小车建立无线连接。

### 2.2 相关技术栈

（1）STM32F10x 标准外设库：ST 官方提供的标准外设库，封装了底层寄存器操作，提供统一的 API 接口。项目使用该库操作 GPIO、USART、I2C、定时器、EXTI、NVIC 等外设。

（2）CMSIS：Cortex 微控制器软件接口标准，提供统一的寄存器定义、系统初始化和中断管理接口。

（3）MQTT 3.1.1 协议：基于发布/订阅模式的轻量级消息传输协议。报文由固定报头（Fixed Header）、可变报头（Variable Header）和有效载荷（Payload）三部分组成。本项目中 STM32 通过 ESP8266 透传模式直接组包和解析 MQTT 报文。

（4）ESP8266 AT 指令集：乐鑫公司推出的低成本 WiFi SoC 模块，支持 AT 指令集进行 WiFi 连接和 TCP/IP 通信。主要指令包括：AT（自检）、ATE0（关闭回显）、AT+CWJAP（连接 WiFi）、AT+CIPSTART（建立 TCP 连接）、AT+CIPMODE=1（透传模式）、AT+CIPSEND（开始发送）等。

（5）HC-SR04 超声波测距原理：Trig 引脚接收 ≥10μs 的高电平触发脉冲后，模块发射 8 个 40kHz 方波；Echo 引脚输出与距离成正比的高电平脉宽。距离公式：距离(cm) = 高电平时间(μs) / 58。使用 STM32 定时器输入捕获模式精确测量 Echo 脉宽。

（6）I2C 通信协议：两线式串行总线，仅需 SCL 和 SDA 即可实现主机与多个从机的通信。本项目使用 GPIO 模拟 I2C 时序驱动 SSD1306 OLED 显示屏。

---

## 第三章 需求分析与概要设计

### 3.1 需求分析

本项目旨在设计一款具备环境感知、自主决策和物联网通信能力的智能小车，具体需求如下：

（1）运动控制需求：小车应能实现前进、后退、左转、右转和停止等基本运动，支持 PWM 调速和差速转向，具备左右轮独立微调功能以校正机械偏差。

（2）环境感知需求：小车需实时采集温湿度数据（DHT11，周期约 2 秒）；需测量前方障碍物距离（HC-SR04），支持多角度扫描；需检测近距障碍（4 路红外传感器）实现紧急制动。

（3）多模式控制需求：支持三种工作模式——蓝牙遥控模式（手机端实时控制）、自动避障模式（自主导航避开障碍物）、自动跟随模式（锁定并跟随前方移动目标）。模式间应能通过菜单或指令快速切换。

（4）物联网通信需求：通过 WiFi 连接互联网，使用 MQTT 协议接入华为云 IoT 平台；每 1 秒上报一次温湿度和距离数据；每 30 秒发送心跳保活；支持接收云端下发的控制命令。

（5）用户界面需求：通过 128×64 OLED 屏幕显示表情动画、温湿度数值、工作模式状态；提供 6 项配置菜单（模式切换、左右轮微调、基准速度、贪吃蛇游戏、WiFi 配网）；开机显示 Creeper 动画。

（6）系统调度需求：主循环周期约 100ms，各功能模块按时序分时调度，避免阻塞。

### 3.2 概要设计

#### 3.2.1 总体设计

系统采用四层架构，自上而下分为应用层、通信层、硬件驱动层和底层库：

```
┌─────────────────────────────────────────────┐
│              应用层 (Application)             │
│   UI 界面 │ 菜单系统 │ 贪吃蛇 │ WiFi配网      │
│   自动避障 │ 自动跟随 │ 调试模块              │
├─────────────────────────────────────────────┤
│              通信层 (Communication)           │
│       ESP8266 驱动 │  MQTT 协议栈            │
├─────────────────────────────────────────────┤
│             硬件驱动层 (Hardware)              │
│  电机 │ 舵机 │ 超声波 │ 红外 │ DHT11 │ OLED  │
│  LED │ 蜂鸣器 │ 按键 │ 串口 │ 延时           │
├─────────────────────────────────────────────┤
│              底层库 (Libraries)               │
│        CMSIS │  STM32F10x StdPeriph         │
└─────────────────────────────────────────────┘
```

#### 3.2.2 功能模块设计

根据团队分工，本人（成员A，组长）负责以下核心模块：

| 模块 | 功能描述 | 关键技术 |
| --- | --- | --- |
| 串口通信 (USART) | USART1/2/3 初始化、中断接收、数据发送 | 中断 + 查询接收 |
| ESP8266 WiFi 驱动 | AT 指令交互、WiFi 连接管理、透传模式切换 | AT 指令 + 超时重试 |
| MQTT 协议栈 | CONNECT/SUBSCRIBE/PUBLISH/PINGREQ 报文组包与解析 | MQTT 3.1.1 逐字节组装 |
| WiFi 配置管理 | SSID/密码运行时修改、WiFi 状态检测 | 标志位 + 周期轮询 |
| 空间扫描 | 9 扇区 180° 测距，最长连续可通行段算法决策 | 9 扇区扫描 + 连续段检测 |
| 自动避障 | 三级递进状态机：红外防撞→扫描→试探转向→掉头 | 有限状态机 (FSM) |
| 自动跟随 | 超声波正前方测距，10~15cm 范围内前进跟随，<10cm 后退拉开，>15cm 或无目标时停车等待 | 三段式距离控制 |
| 调试模块 | 周期状态输出、help/status/sensors 等命令解析 | 字符串解析 |
| 主程序整合 | 初始化顺序编排、主循环时序调度、中断优先级分配 | 时间片轮询 |

#### 3.2.3 硬件连接方式

| 外设 | 引脚 | 说明 |
| --- | --- | --- |
| USART1 (调试) | PA9 (TX) / PA10 (RX) | 波特率 9600，连接上位机串口 |
| USART2 (蓝牙) | PA2 (TX) / PA3 (RX) | 波特率 115200，连接 HC-05 蓝牙模块 |
| USART3 (WiFi) | PB10 (TX) / PB11 (RX) | 波特率 115200，连接 ESP8266 |
| 超声波 Trig | PA7 | 推挽输出 |
| 超声波 Echo | PA6 (TIM3_CH1) | 输入捕获测脉宽 |
| 红外避障 ×4 | PF5~PF8 | 上拉输入，低电平=有障碍 |
| OLED I2C | PG12(SCL) / PD5(SDA) / PD4(RES) | GPIO 模拟 I2C 驱动 SSD1306 |
| 舵机 PWM | PA0 (TIM2_CH1) | 50Hz，0.5ms~2.5ms→0°~180° |
| 电机 PWM | PB6 (TIM4_CH1) / PB7 (TIM4_CH2) | 右轮/左轮，1kHz |
| 电机方向 | PB12/PB13 (右轮) / PB14/PB15 (左轮) | TB6612 驱动 IN1~IN4 |
| DHT11 | PA1 | 单总线温湿度 |
| LED0 / LED1 | PB5 / PE5 | 推挽输出，低电平点亮 |
| 蜂鸣器 | PF0 | 有源蜂鸣器 |
| KEY0 / KEY1 | PE4 / PE3 | 上拉输入 + EXTI 中断 |

### 3.3 华为云 MQTT 设计

（1）平台接入参数：使用华为云 IoTDA，广州区域（cn-south-4）。设备通过 Broker 地址（project_id.iotda-device.cn-south-4.myhuaweicloud.com）、端口 1883、Client ID、Username、Password 连接平台。

（2）MQTT Topic 设计：

| Topic | 方向 | 说明 |
| --- | --- | --- |
| `$oc/devices/{id}/sys/properties/report` | 上报 | 设备属性上报（温湿度、游戏分数） |
| `$oc/devices/{id}/sys/messages/down` | 订阅 | 平台消息下发 |
| `$oc/devices/{id}/sys/commands/#` | 订阅 | 平台命令下发（通配符匹配） |
| `$oc/devices/{id}/sys/commands/response/request_id={id}` | 上报 | 命令执行响应 |

（3）数据上报格式（JSON）：

```json
{"services":[{"service_id":"smokeDetector","properties":{"temperature":25.3,"humidity":60},"event_time":"20260701T104112Z"}]}
```

（4）实现方案：MQTT 报文在 g_esp8266_tx_buf[512] 中逐字节组装。固定报头含报文类型和剩余长度（变长编码），可变报头含 Topic 长度/内容，有效载荷为 JSON 字符串。通过 ESP8266 透传模式发送。下行命令通过 mqtt_process_command() 解析 commands/request_id= 格式并回复响应。

---

## 第四章 详细设计与实现

### 4.1 开发环境介绍

| 项目 | 说明 |
| --- | --- |
| 操作系统 | Windows 11 |
| IDE | Keil MDK-ARM V5 |
| 编译器 | ARM Compiler V6 |
| 下载调试器 | ST-Link V2 (SWD) |
| 芯片型号 | STM32F103ZET6 |
| 标准库 | STM32F10x StdPeriph Driver V3.5.0 |
| 串口调试 | SSCOM V5.13 |
| 蓝牙调试 | Serial Bluetooth Terminal (Android) |
| 云平台 | 华为云 IoTDA (cn-south-4) |

### 4.2 串口通信驱动实现

**实现方法**：系统共使用 3 路 USART——USART1（调试串口，PA9/PA10，9600bps）、USART2（蓝牙，PA2/PA3，115200bps）、USART3（ESP8266，PB10/PB11，115200bps）。每路串口均配置为异步模式：1 位起始位、8 位数据位、1 位停止位、无校验。USART1 和 USART2 使用 NVIC 中断接收，USART3 使用中断接收配合大缓冲区处理 ESP8266 响应。所有串口提供统一的初始化、发送字节、发送字符串和 printf 重定向接口。

命令解析在 parse_cmd() 函数中统一完成：uart_flag 标志位指示是否有待处理命令，uart_buf 存储命令字符串。parse_cmd() 依次处理蓝牙指令（mb）、MQTT 下行路由指令（rb）和调试指令，通过 strstr() 匹配命令关键字并执行对应操作。支持的命令包括：goA/goB/goL/goR（运动）、stop（停止）、speedup/speeddown（速度 ±100）、lspeedup/lspeeddown（左轮微调 ±10）、rspeedup/rspeeddown（右轮微调 ±10）、remote/auto/follow（模式切换）等。

### 4.3 ESP8266 WiFi 驱动实现

**实现方法**：ESP8266 驱动封装了完整的 AT 指令交互流程，主要包括以下函数：

- esp8266_init()：初始化 USART3 串口通信
- esp8266_connect_ap(ssid, pswd)：发送 AT+CWJAP 连接 WiFi 热点，超时 10 秒，最多重试 3 次
- esp8266_connect_server(mode, ip, port)：发送 AT+CIPSTART 建立 TCP 连接
- esp8266_entry_transparent_transmission()：开启透传模式（AT+CIPMODE=1 + AT+CIPSEND）
- esp8266_exit_transparent_transmission()：发送 +++ 退出透传模式
- esp8266_send_bytes(buf, len)：原始字节发送接口，用于 MQTT 报文透传

核心设计要点：AT 指令发送前自动清空接收缓冲区；每条 AT 指令有超时重试机制（3 次）；+++ 退出透传时需间隔 1 秒避免被模块误认为数据；透传模式下所有 MQTT 报文直接通过 esp8266_send_bytes() 发送，无需 AT 指令前缀。

接收数据通过 USART3 中断存入 g_esp8266_rx_buf[512]，主循环通过 g_esp8266_rx_cnt 和 g_esp8266_rx_end 轮询接收状态。g_esp8266_transparent_transmission_sta 标志位标记当前是否处于透传模式。

**关键代码（连接 WiFi 热点）**：

```c
int32_t esp8266_connect_ap(char* ssid, char* pswd)
{
    char cmd[128];
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pswd);
    g_esp8266_rx_cnt = 0;
    g_esp8266_rx_end = 0;
    esp8266_send_at(cmd);
    for (int retry = 0; retry < 3; retry++) {
        for (int t = 0; t < 200; t++) {
            if (g_esp8266_rx_end) {
                if (strstr((char*)g_esp8266_rx_buf, "OK") ||
                    strstr((char*)g_esp8266_rx_buf, "WIFI CONNECTED"))
                    return 0;
                if (strstr((char*)g_esp8266_rx_buf, "FAIL")) break;
            }
            delay_ms(50);
        }
    }
    return -1;
}
```

### 4.4 MQTT 协议实现

**实现方法**：MQTT 协议栈完全自研，不依赖任何第三方库，所有报文在 g_esp8266_tx_buf[512] 中逐字节组装后通过透传发送。

CONNECT 报文：固定报头 0x10 + 剩余长度（变长编码）+ 协议名 "MQTT" + 协议级别 4 + 连接标志 0xC2 + Keep Alive 60s + Client ID/Username/Password。发送后等待 CONNACK（0x20 0x02 0x00 0x00），3 秒超时重试 2 次。

SUBSCRIBE 报文：固定报头 0x82 + 剩余长度 + 2 字节报文标识符（静态自增）+ Topic + QoS。等待 SUBACK（0x90 0x03）。

PUBLISH 报文：固定报头 0x30（QoS 0）+ 剩余长度 + Topic + Payload。

PINGREQ 心跳：2 字节 0xC0 0x00，每 30 秒发送一次。

下行命令处理（mqtt_process_command）：在接收缓冲区中扫描 PUBLISH 报文，解析 commands/request_id= 格式，提取 command_name 和 request_id，通过 uart_buf 中转执行命令，向响应 Topic 回发 {"result_code":0}。

状态上报（mqtt_report_devices_status）：每 ~1 秒调用一次。读取 DHT11 数据，格式化 JSON 上报。

**关键代码（MQTT CONNECT 报文组包）**：

```c
int32_t mqtt_connect(char *client_id, char *user_name, char *password)
{
    int32_t client_id_len = strlen(client_id);
    int32_t user_name_len = strlen(user_name);
    int32_t password_len = strlen(password);
    int32_t data_len = 10 + (client_id_len+2) + (user_name_len+2) + (password_len+2);
    int32_t cnt = 2, wait;
    g_mqtt_tx_len = 0;

    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0x10;  // CONNECT
    do {  // 剩余长度变长编码
        uint8_t encodedByte = data_len % 128;
        data_len /= 128;
        if (data_len > 0) encodedByte |= 128;
        g_esp8266_tx_buf[g_mqtt_tx_len++] = encodedByte;
    } while (data_len > 0);

    // 可变报头：协议名 MQTT + 级别4 + 标志0xC2 + KeepAlive 60s
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0; g_esp8266_tx_buf[g_mqtt_tx_len++] = 4;
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'M'; g_esp8266_tx_buf[g_mqtt_tx_len++] = 'Q';
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'T'; g_esp8266_tx_buf[g_mqtt_tx_len++] = 'T';
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 4;       // 协议级别
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0xc2;    // 连接标志
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0;
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 60;      // Keep Alive

    // Payload: Client ID + Username + Password
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(client_id_len);
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(client_id_len);
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len], client_id, client_id_len);
    g_mqtt_tx_len += client_id_len;
    // Username 和 Password 同理...

    while (cnt--) {
        memset((void *)g_esp8266_rx_buf, 0, sizeof(g_esp8266_rx_buf));
        g_esp8266_rx_cnt = 0;
        mqtt_send_bytes(g_esp8266_tx_buf, g_mqtt_tx_len);
        wait = 30;
        while (wait--) {
            if (g_esp8266_rx_buf[0] == 0x20 && g_esp8266_rx_buf[1] == 0x02)
                return 0;
            delay_ms(100);
        }
    }
    return -1;
}
```

### 4.5 自动避障功能实现

**实现方法**：自动避障采用四级有限状态机，在主循环 Mode_Avoid() 中周期调用。

状态0（前进）：正常行驶时 Motor_Forward(speed)，舵机回正 90°。每周期检测红外传感器（PF7=左红外, PF8=右红外，低电平触发）和前向超声波距离。红外具有最高优先级——左右同时障碍时急停→后退500ms→左转400ms→恢复前进；单侧障碍时转向300ms后恢复。红外未触发且前向距离 < avoid_dist（20cm）时进入状态1。

状态1（空间扫描）：后退 200ms 腾出空间 → Spatial_Scan(avoid_dist) 9 扇区扫描 → 返回最优方向。PATH_ESCAPE 进入状态3，否则进入状态2。

状态2（试探转向）：按推荐方向旋转 300ms → 测距确认。每方向最多尝试 3 次，不通则换反方向，两侧都失败进入状态3。

状态3（掉头脱困）：连续左转 350ms × 6 次（约 180°），一旦通畅立即回到状态0。

**关键代码（避障状态机主框架）**：

```c
void Mode_Avoid(void)
{
    static uint8_t state = 0;
    static uint8_t dir = 0, cnt = 0, tries = 0;

    distance = SR04_GetDistance();
    uint8_t irL = PFin(7), irR = PFin(8);  // 红外检测

    // 红外优先
    if (irL == 0 && irR == 0) {
        Motor_Stop(); Motor_Backward(speed); delay_ms(500);
        Motor_Left(speed); delay_ms(400);
        state = 0; cnt = 0; return;
    }
    if (irL == 0) { Motor_Right(speed); delay_ms(300); state = 0; return; }
    if (irR == 0) { Motor_Left(speed); delay_ms(300); state = 0; return; }

    if (distance < 1 || distance > avoid_dist) {
        Motor_Forward(speed); state = 0; return;
    }

    if (state == 0) {
        Motor_Backward(speed); delay_ms(200); Motor_Stop();
        uint8_t path = Spatial_Scan(avoid_dist);
        if (path == PATH_ESCAPE) state = 3;
        else { dir = (path==PATH_RIGHT)?1:0; cnt=0; tries=0; state=2; }
    } else if (state == 2) {
        if (dir==0) Motor_Left(speed); else Motor_Right(speed);
        delay_ms(300); Motor_Stop(); delay_ms(60); cnt++;
        if (SR04_GetDistance() > avoid_dist) { state=0; }
        else if (cnt>=3) { dir = !dir; cnt=0; tries++;
            if (tries>=2) state=3; }
    } else if (state == 3) {
        Motor_Left(speed); delay_ms(350); Motor_Stop(); delay_ms(60); cnt++;
        if (cnt>=6 || SR04_GetDistance()>avoid_dist) { state=0; cnt=0; }
    }
}
```

### 4.6 自动跟随功能实现

**实现方法**：跟随模式通过超声波正前方测距实现。舵机固定在 90° 正前方，每主循环周期（~100ms）测距一次，根据距离范围执行三段式控制：

- **10~15cm**：前进跟随，以全局基准速度 speed 向前行驶，显示正常表情（笑脸）
- **<10cm**：后退拉开，以 speed×3/4 的速度后退保持安全距离，显示警戒表情
- **>15cm 或无效数据（超时）**：停车等待，显示悲伤表情

模式切换时自动复位——从其他模式切入跟随时舵机回正 90°，延时 200ms 后开始工作。

**实现结果**：跟随逻辑简洁可靠，在直行场景下能有效保持 10~15cm 的跟车距离。目标丢失或距离过远时自动停车，防止误撞。

**关键代码**：

```c
void Follow_Task(void)
{
    static u8 init = 0;
    float d;

    /* 模式切换检测：从其他模式切回跟随时复位 */
    { static u8 last_mode = 0xFF;
      if (last_mode != work_mode) { init = 0; last_mode = work_mode; } }

    /* 首次进入：舵机回正 */
    if (!init) { Servo_SetAngle(90); delay_ms(200); init = 1; return; }

    d = SR04_GetDistance();

    /* 无效数据(超时) → 停车 */
    if (d <= 0) { Motor_Stop(); UI_SetMode(FACE_SAD); return; }

    if (d >= 10 && d <= 15) {
        /* 10~15cm → 前进跟随 */
        Motor_Forward(speed);
        UI_SetMode(FACE_NORMAL);
    } else if (d < 10) {
        /* <10cm → 后退拉开 */
        Motor_Backward(speed * 3 / 4);
        UI_SetMode(FACE_ALERT);
    } else {
        /* >15cm → 停车 */
        Motor_Stop();
        UI_SetMode(FACE_SAD);
    }
}
```

### 4.7 空间扫描算法实现

**实现方法**：Spatial_Scan() 完成前方 180° 9 扇区测距与路径决策。扫描范围 30°~150°，步进 15°，角度通过静态查表数组 g_angles[9] 定义。每扇区舵机到位后延时 150ms，超声测距结果记录在 g_map[9] 中。扫描完成后舵机回正 90°。

最长连续可通行段决策算法：找到距离 > threshold 或 < 1（无回波）的最长连续扇区段。若该段覆盖中央扇区（索引 4=90°）则直行，中心偏左则左转，偏右则右转，无可通行段则返回脱困。

**关键代码**：

```c
uint8_t Spatial_Scan(uint16_t threshold)
{
    static const uint8_t g_angles[9] = {30,45,60,75,90,105,120,135,150};
    uint8_t best_start=0, best_len=0, cur_start=0, cur_len=0;

    for (uint8_t i = 0; i < 9; i++) {
        Servo_SetAngle(g_angles[i]);
        delay_ms(150);
        g_map[i] = (uint16_t)SR04_GetDistance();
    }
    Servo_SetAngle(90);

    for (uint8_t i = 0; i < 9; i++) {
        if (g_map[i] > threshold || g_map[i] < 1) {
            if (cur_len == 0) cur_start = i;
            cur_len++;
            if (cur_len > best_len) { best_len=cur_len; best_start=cur_start; }
        } else { cur_len = 0; }
    }
    if (best_len == 0) return PATH_ESCAPE;

    uint8_t center = best_start + best_len / 2;
    if (best_start <= 4 && (best_start+best_len-1) >= 4) return PATH_FORWARD;
    if (center < 4) return PATH_LEFT;
    return PATH_RIGHT;
}
```

### 4.8 调试模块实现

**实现方法**：调试模块提供双通道输出——同时发送到 USART1（有线串口）和 USART2（蓝牙）。支持的命令包括：

- help：列出所有命令
- status：系统状态（模式/速度/微调/距离/WiFi/运行时间）
- sensors：实时温湿度 + 超声波距离
- ping：返回 pong
- uptime：运行秒数
- echo <text>：环回测试
- debug on/off：开启/关闭周期自动报告

周期自动报告每 ~1.5 秒输出一次，格式：[D] T:xx.xC H:xx.x% D:xxxcm SPD:xxx MODE W:ON/OFF。报告前自动重新读取 DHT11 和超声波传感器。

### 4.9 主程序调度与整合

**实现方法**：main.c 中的主循环采用时间片轮询调度（~100ms/周期），各功能模块通过静态计数器分频实现不同周期：

```text
while(1):
  ├── DHT11 采集 (每15次≈1.5s)
  ├── MQTT 上报 (每10次≈1s)
  ├── MQTT 心跳 (每300次≈30s)
  ├── delay_ms(20)
  ├── parse_cmd()      // 命令解析
  ├── WifiCfg_Task()   // WiFi 配置
  ├── Debug_Task()     // 调试输出
  ├── Snake_Task() / UI_Task()
  ├── Mode_Avoid() / Follow_Task()
  └── delay_ms(80)
```

中断优先级配置：TIM3 输入捕获（超声波）抢占优先级 1 子优先级 1（最高），USART1/2/3 和 KEY1(EXTI3) 优先级 2/2，KEY0(EXTI4) 优先级 3/3（最低）。

### 4.10 WiFi + MQTT 连接初始化

esp8266_mqtt_init_animated() 封装了完整的 WiFi 连接 + MQTT 建连流程：

1. esp8266_init() → 退出透传模式
2. AT+CIFSR 检测已有 WiFi 连接（STM32 软复位时 ESP 不断电）
3. 若已连接则跳过复位和连 AP，否则 AT+RST → ATE0 → AT+CWJAP
4. AT+CIPSTART 建立 TCP 连接（华为云 1883 端口）
5. 进入透传模式
6. mqtt_connect() → 订阅 Topic → 置位 g_wifi_connected

初始化期间 OLED 全屏播放 WiFi 信号动画（4 帧循环），连接成功显示 "Success"。

---

## 第五章 实习总结与收获

### 5.1 实习总结

本次毕业实习期间，我以嵌入式软件开发工程师的身份参与了 STM32 物联网环境感知智控车项目的全流程开发。作为一名即将毕业的计算机科学与技术专业学生，这次实习为我提供了将四年所学理论知识转化为实际工程能力的宝贵机会。作为项目组长，我负责 ESP8266 WiFi 驱动、MQTT 协议栈、自动避障与跟随算法、串口通信、调试模块的开发以及整个系统的架构设计与整合。

通过本次实习，我将在校所学的嵌入式系统理论知识与工程实践紧密结合，完成了从需求分析、方案设计、模块开发到系统联调的完整项目周期，在技术能力和职业素养两方面均获得了显著提升。

**（1）嵌入式软件开发能力提升**

在实习中，我深入掌握了 STM32F103 平台的开发流程，包括 Keil MDK 工程配置、标准外设库使用、中断优先级管理、定时器 PWM 与输入捕获等外设的编程方法。在实际开发中，我独立完成了以下工作：

- 实现了三路 USART 串口的初始化和中断接收处理，分别用于调试串口、蓝牙通信和 WiFi 模块通信，深入理解了中断优先级配置对系统实时性的影响。
- 独立完成了 ESP8266 WiFi 模块的 AT 指令驱动封装，包括 WiFi 连接、TCP 连接、透传模式切换等功能，掌握了 AT 指令交互的超时重试机制和状态管理方法。
- 实现了完整的 MQTT 3.1.1 协议栈，从 CONNECT、SUBSCRIBE、PUBLISH 到 PINGREQ 报文全部逐字节组装和解析，不依赖任何第三方库。成功对接华为云 IoT 平台，实现了温湿度数据周期上报和平台命令下发响应。
- 设计了下行命令解析机制，能够从 MQTT PUBLISH 报文中提取 commands/request_id 格式的命令并回复执行结果。

**（2）算法设计与实现能力**

在自动避障和跟随模块的开发中，我设计并实现了多种嵌入式控制算法：

- 空间扫描最长连续可通行段决策算法：通过舵机配合超声波完成 180° 9 扇区测距，基于最长连续可通行段策略选择最优行进方向，为避障决策提供可靠的环境数据。
- 四级递进避障状态机：采用有限状态机架构，实现了红外防撞、空间扫描决策、试探转向和掉头脱困四级递进策略，有效应对正面碰撞、单侧障碍和死角困局等多种场景。
- 跟随模式三段式距离控制：通过超声波正前方测距，根据距离范围执行前进跟随、后退拉开和停车等待三种状态，逻辑简洁可靠。

这些算法的设计需要在资源受限的嵌入式环境下运行，对代码效率和内存占用有严格要求。通过优化算法结构、合理使用静态变量和状态机设计，有效控制了代码体积和执行时间。

**（3）系统架构与团队协作能力**

作为项目组长，我负责主程序架构设计和时序调度，将团队成员开发的 30 余个模块统一整合到 main.c 中。主循环采用时间片轮询调度架构（约 100ms/周期），各功能模块通过静态计数器分频实现不同周期的任务调度。在模块接口定义、时序协调、中断优先级分配等方面积累了宝贵的实践经验。

同时，通过制定模块接口规范、分配开发任务和跟踪进度，我锻炼了项目管理和团队协作能力。在联调阶段，面对各模块之间接口对接的兼容性问题，我组织团队成员逐个排查，确保系统整体运行稳定。

### 5.2 不足与展望

虽然项目达到了预期目标，但从工程实践的角度来看，仍存在以下可改进之处：

（1）MQTT 安全性：当前使用非加密 MQTT（端口 1883），设备凭证以明文存储在代码中。在工业级应用中，这种方式存在安全隐患。后续可考虑使用 TLS/SSL 加密（端口 8883）或硬件加密芯片来增强通信安全。

（2）OTA 升级：当前固件烧录依赖有线 SWD 接口，设备部署后升级不便。后续可利用 ESP8266 的网络能力实现远程固件更新，便于设备的持续维护和功能迭代。

（3）避障算法优化：当前仅在二维平面做扇区扫描，未考虑地形高度差异。后续可引入 IMU 陀螺仪或红外测距阵列实现更精准的 3D 环境建模，提升在复杂地形中的避障能力。

（4）云平台深度集成：后续可利用华为云规则引擎实现数据异常告警功能，将温湿度历史数据存入云数据库，通过手机 App 实现远程数据可视化和设备控制。

（5）跟随模式增强：当前跟随逻辑仅为简单的三段式距离控制，无平滑加减速和横向追踪能力。后续可引入 PID 控制算法实现速度平滑调节，或配合舵机扫描实现目标跟踪。

## 第六章 致谢

时光荏苒，大学四年的学习生活即将画上句号。在这段宝贵的实习经历中，我收获的不仅仅是专业知识和工程经验，更是对职业道路的清晰认知和对自身能力的客观评估。

首先，感谢实习单位为我提供了宝贵的实践平台和良好的工作环境。在实习期间，我得以将课堂上学到的理论知识应用到实际项目中，真正体会到了"纸上得来终觉浅，绝知此事要躬行"的含义。

特别感谢指导老师在实习期间给予的耐心指导和宝贵建议。在项目遇到技术难题时，老师总能及时为我指明方向，帮助我理清思路。从 MQTT 协议栈的实现到避障算法的优化，每一次指导都让我受益匪浅。老师严谨的治学态度和丰富的工程经验，为我树立了学习的榜样。

感谢项目团队成员的积极配合。嵌入式开发涉及硬件、驱动、算法、通信等多个领域，任何一个环节的疏漏都可能影响整个系统的稳定运行。正是团队成员在各自模块开发中的认真负责和密切协作，才使得项目能够按时高质量完成。

同时，感谢大学四年间所有教导过我的老师们。是你们的悉心教导，为我打下了扎实的专业基础，让我能够在实习中快速上手实际项目开发。感谢同学们的陪伴和帮助，我们一起学习、一起成长的时光将是我人生中最珍贵的回忆。

通过本次毕业实习，我不仅巩固了嵌入式系统开发的专业知识，更培养了独立解决技术问题的能力、严谨的工作态度和团队协作精神。这段经历让我对未来的职业生涯充满信心，我将以此为新的起点，在今后的工作中不断学习、持续进步，努力成为一名优秀的软件工程师。


---

## 附录

### 附录 A：引脚分配总表

| 引脚 | 外设 | 功能 |
| --- | --- | --- |
| PA0 | TIM2_CH1 | SG90 舵机 PWM |
| PA1 | GPIO | DHT11 数据线 |
| PA2/PA3 | USART2 | HC-05 蓝牙 |
| PA6 | TIM3_CH1 | SR04 Echo 输入捕获 |
| PA7 | GPIO | SR04 Trig 触发 |
| PA9/PA10 | USART1 | 调试串口 |
| PB5 | GPIO | LED0 |
| PB6 | TIM4_CH1 | 电机 PWM（物理右轮） |
| PB7 | TIM4_CH2 | 电机 PWM（物理左轮） |
| PG12/PD5/PD4 | I2C (模拟) | OLED SCL/SDA/RES |
| PB10/PB11 | USART3 | ESP8266 WiFi |
| PB12/PB13 | GPIO | 电机方向（物理右轮 IN1/IN2） |
| PB14/PB15 | GPIO | 电机方向（物理左轮 IN3/IN4） |
| PE3/PE4 | EXTI | KEY1/KEY0 |
| PE5 | GPIO | LED1 |
| PF0 | GPIO | 蜂鸣器 |
| PF5~8 | GPIO | 红外避障 ×4 |

### 附录 B：主要源代码文件清单

（成员A 负责的文件以粗体标注）

| 文件 | 说明 |
| --- | --- |
| **main.c** | 主程序入口与调度 |
| **usart.h/c** | 三路串口驱动 + 命令解析 |
| **esp8266.h/c** | ESP8266 AT 指令驱动 |
| **esp8266_mqtt.h/c** | MQTT 协议与华为云对接 |
| **wificfg.h/c** | WiFi 配置管理 |
| **spatial.h/c** | 空间扫描算法 |
| **avoid.h/c** | 自动避障状态机 |
| **follow.h/c** | 自动跟随控制 |
| **debug.h/c** | 调试输出模块 |
| motor.h/c | 电机驱动（成员C） |
| servo.h/c | 舵机驱动（成员C） |
| sr04.h/c | 超声波测距（成员C） |
| dht11.h/c | 温湿度传感器（成员C） |
| oled.h/c | OLED 显示驱动（成员B） |
| ui.h/c | UI 界面（成员B） |
| menu.h/c | 菜单系统（成员B） |
| snake.h/c | 贪吃蛇游戏（成员B） |
| led.h/c | LED 指示灯（成员D） |
| beep.h/c | 蜂鸣器（成员D） |
| key.h/c | 按键驱动（成员D） |
| delay.h/c | SysTick 延时（成员C） |
| sys.c | 系统初始化（成员D） |

---

> **排版说明（转 Word 时应用）**：
>
> - 正文：宋体小四（12pt），首行缩进 2 字符，行间距 22 磅（固定值）
> - 一级标题（第一章）：黑体三号（16pt），居中，加粗
> - 二级标题（1.1）：黑体四号（14pt），左对齐，加粗
> - 三级标题（1.1.1）：黑体小四（12pt），左对齐，加粗
> - 表格：宋体五号（10.5pt），居中
> - 代码块：Courier New 五号（10.5pt），单倍行距
> - 页边距：上 2.5cm，下 2.5cm，左 3cm，右 2.5cm
> - 页码：底部居中
