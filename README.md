# STM32 物联网环境感知智控车

基于 STM32F103 的物联网智能小车，集成环境感知、自动避障、目标跟随、蓝牙遥控、WiFi 联网与华为云 IoT 平台数据上报，配备 OLED 图形界面与贪吃蛇游戏。

---

## 目录

- [硬件平台](#硬件平台)
- [功能特性](#功能特性)
- [系统架构](#系统架构)
- [硬件连接](#硬件连接)
- [开发环境](#开发环境)
- [项目结构](#项目结构)
- [工作模式](#工作模式)
- [用户界面](#用户界面)
- [团队分工](#团队分工)
- [编译与烧录](#编译与烧录)
- [使用说明](#使用说明)

---

## 硬件平台

| 组件 | 型号 / 说明 |
| --- | --- |
| 主控芯片 | STM32F103ZET6 (Cortex-M3) |
| OLED 显示屏 | SSD1306 128×64 单色 I2C |
| 温湿度传感器 | DHT11 |
| 超声波测距模块 | HC-SR04 |
| 舵机 | SG90 (0°~180°) |
| 红外避障传感器 | ×4 (前方防撞检测) |
| 电机驱动 | TB6612FNG (双路直流电机) |
| 蓝牙模块 | HC-05 (串口透传) |
| WiFi 模块 | ESP8266 (AT 指令) |
| 蜂鸣器 | 有源蜂鸣器（支持变频） |
| 按键 | KEY0 / KEY1 (EXTI 中断) |
| LED 指示灯 | LED0 (PB5) / LED1 (PE5) |

---

## 功能特性

### 三大工作模式

- **蓝牙遥控模式 (Remote)** — 手机通过蓝牙发送指令实时控制
- **自动避障模式 (Auto)** — 红外 + 超声波三级递进避障状态机
- **跟随模式 (Follow)** — 超声波锁定前方物体自动跟随

### 环境感知

- DHT11 每 1.5~2 秒采集一次温湿度，同步更新 OLED 显示和串口输出
- HC-SR04 超声波测距，9 扇区 180° 空间扫描（配合 SG90 舵机）
- 4 路红外传感器近距碰撞检测

### 物联网

- ESP8266 连接华为云 IoT 平台（MQTT 3.1.1 协议）
- 温湿度数据每 1 秒上报一次到云端
- 支持平台命令下发（远程遥控、属性读取）
- MQTT 心跳保活（30 秒间隔）
- WiFi 自动重连
- 运行时可配置 SSID/密码

### OLED 图形界面

- Creeper（Minecraft 苦力怕）开机动画
- 表情交互系统：笑脸 / 警戒 / 撞墙
- 眨眼动画 + 温湿度数值显示
- 多级菜单系统（6 项配置菜单）
- WiFi 信号连接动画

### 贪吃蛇游戏

- 内置于菜单系统，可通过按键或蓝牙指令进入
- 16×8 网格，最大蛇长 128
- 蓝牙方向控制
- 分数可通过 MQTT 上报到云端

### 其他

- 蜂鸣器洒水车音乐播放
- 按键 EXTI 中断响应（支持菜单操作）
- 调试串口（USART2）状态输出
- 左右轮差速微调（菜单可配）
- 基准速度可调

---

## 系统架构

```text
┌─────────────────────────────────────────────────┐
│                   Application                    │
│  UI (表情/温湿度) │ Menu │ Snake │ WifiCfg │ Debug │
├─────────────────────────────────────────────────┤
│                   Hardware                       │
│  Motor │ Servo │ SR04 │ DHT11 │ IR │ LED │ Beep │
│  OLED  │ Spatial │ Follow │ Avoid │ Key         │
├─────────────────────────────────────────────────┤
│                 Communication                    │
│           ESP8266 │ ESP8266 MQTT                 │
├─────────────────────────────────────────────────┤
│                    System                        │
│          sys.c │ delay │ usart                   │
├─────────────────────────────────────────────────┤
│                   Libraries                      │
│     CMSIS │ STM32F10x StdPeriph Driver           │
└─────────────────────────────────────────────────┘
```

### 主循环调度（约 100ms/周期）

```text
DHT11 读取(2s) → MQTT 上报(1s) → 心跳(30s) → 命令解析
→ WiFi 配置任务 → 调试输出 → UI/贪吃蛇 → 避障/跟随
```

### 避障状态机

```text
状态0: 前进（正常行驶，持续测距）
  ↓ 红外触发 或 超声波距离 < 阈值
状态1: 空间扫描（后退 → 9扇区180°扫描 → 路径决策）
  ↓ 决策完成
状态2: 试探转向（按推荐方向旋转，每方向最多3次）
  ↓ 试探失败
状态3: 掉头（连续左转至脱困）
```

---

## 硬件连接

### 电机驱动 (TB6612)

| 引脚 | 功能 |
| --- | --- |
| PB6 | 左轮 PWM (TIM4_CH1) |
| PB7 | 右轮 PWM (TIM4_CH2) |
| PB12 | 左轮 IN1 |
| PB13 | 左轮 IN2 |
| PB14 | 右轮 IN3 |
| PB15 | 右轮 IN4 |

### 传感器

| 引脚 | 功能 |
| --- | --- |
| PA1 | DHT11 DATA |
| PA6 | SR04 Echo (TIM3_CH1) |
| PA7 | SR04 Trig |
| PF5~8 | 红外避障 ×4 |

### 舵机

| 引脚 | 功能 |
| --- | --- |
| PA0 | SG90 PWM (TIM2_CH1) |

### 串口

| 外设 | 引脚 | 用途 |
| --- | --- | --- |
| USART1 | PA9/PA10 | 蓝牙 HC-05 |
| USART2 | PA2/PA3 | 调试串口 |
| USART3 | PB10/PB11 | ESP8266 |

### OLED (I2C)

| 引脚 | 功能 |
| --- | --- |
| PB8 | SCL |
| PB9 | SDA |

### 按键与指示

| 引脚 | 功能 |
| --- | --- |
| PE3 | KEY1 (DOWN) |
| PE4 | KEY0 (UP) |
| PB5 | LED0 |
| PE5 | LED1 |
| PF0 | 蜂鸣器 |

---

## 开发环境

| 工具 | 版本 / 说明 |
| --- | --- |
| IDE | Keil MDK-ARM V5 |
| 编译器 | ARM Compiler V5 / V6 |
| 标准库 | STM32F10x StdPeriph Driver |
| CMSIS | V3.x |
| 调试/下载 | ST-Link / J-Link (SWD) |
| 串口调试工具 | SSCOM / MobaXterm / Putty |

---

## 项目结构

```text
stm32-envsense-car/
├── User/                       # 用户代码
│   ├── main.c                  # 主程序入口 + 主循环调度
│   └── stm32f10x_it.c          # 中断服务函数
├── Hardware/
│   ├── Inc/                    # 硬件驱动头文件
│   │   ├── motor.h             # 电机驱动（TB6612, PWM）
│   │   ├── servo.h             # 舵机控制（SG90, 0~180°）
│   │   ├── sr04.h              # 超声波测距（HC-SR04）
│   │   ├── dht11.h             # 温湿度传感器（DHT11）
│   │   ├── avoid.h             # 自动避障（红外+超声波状态机）
│   │   ├── follow.h            # 超声波跟随
│   │   ├── spatial.h           # 180° 空间扫描（9扇区）
│   │   ├── oled.h              # OLED 驱动（SSD1306 I2C）
│   │   ├── led.h               # LED 指示灯
│   │   ├── beep.h              # 蜂鸣器（含音乐旋律）
│   │   ├── key.h               # 按键（EXTI 中断 + 查询）
│   │   ├── usart.h             # 串口驱动（UART1/2/3）
│   │   ├── delay.h             # SysTick 毫秒延时
│   │   └── bmp.h               # 内置位图数据
│   └── Src/                    # 硬件驱动源文件
│       ├── motor.c / servo.c / sr04.c / dht11.c
│       ├── avoid.c / follow.c / spatial.c
│       ├── oled.c / led.c / beep.c / key.c
│       ├── usart.c / delay.c
│       └── bmp.c
├── Application/
│   ├── Inc/                    # 应用层头文件
│   │   ├── ui.h                # UI 界面（表情/温湿度/动画）
│   │   ├── menu.h              # 菜单系统（6项配置）
│   │   ├── snake.h             # 贪吃蛇游戏
│   │   ├── wificfg.h           # WiFi 配置管理
│   │   ├── debug.h             # 调试输出模块
│   │   ├── creeper.h           # Creeper 开机画面（64×64）
│   │   ├── oledfont.h          # OLED 字库
│   │   ├── ui_res.h            # UI 资源（位图集合）
│   │   └── ui_wifi.h           # WiFi 动画位图
│   └── Src/                    # 应用层源文件
│       ├── ui.c / menu.c / snake.c
│       ├── wificfg.c / debug.c
│       └── oledfont.c
├── Communication/              # 通信层
│   ├── esp8266.h / esp8266.c           # ESP8266 AT 指令驱动
│   └── esp8266_mqtt.h / esp8266_mqtt.c # MQTT 协议（华为云 IoT）
├── System/
│   └── sys.c                   # 系统初始化工具
├── Libraries/
│   ├── CMSIS/                  # ARM Cortex-M3 CMSIS
│   └── STM32F10x_StdPeriph_Driver/  # STM32 标准外设库
└── Project/
    ├── STM32-EnvSense.uvprojx  # Keil 工程文件
    └── STM32-EnvSense.uvoptx   # Keil 工程配置
```

---

## 工作模式

| 模式 | 宏定义 | 值 | 说明 |
| --- | --- | --- | --- |
| 蓝牙遥控模式 | MODE_REMOTE | 0 | 手机通过蓝牙发送方向/速度指令 |
| 自动避障模式 | MODE_AUTO | 1 | 红外+超声波三级递进避障自主行驶 |
| 跟随模式 | MODE_FOLLOW | 2 | 锁定前方物体，保持约 20cm 距离跟随 |

模式切换方式：

- **菜单**：进入菜单 → 光标选中"工作模式" → 左右键切换
- **蓝牙指令**：发送对应模式切换命令

### 蓝牙遥控指令

| 指令 | 功能 |
| --- | --- |
| goA | 前进 |
| backA | 后退 |
| rightA | 右转 |
| leftA | 左转 |
| stopA | 停止 |

### 调试指令（USART2 / 蓝牙）

| 指令 | 功能 |
| --- | --- |
| help | 显示帮助信息 |
| status | 状态报告 |
| sensors | 传感器数据 |
| ping | 连接测试 |
| uptime | 运行时间 |
| reset | 系统复位 |
| debug on/off | 调试输出开关 |

---

## 用户界面

### 表情系统

| 模式 | 触发条件 | 显示效果 |
| --- | --- | --- |
| FACE_NORMAL | 正常行驶/空闲 | 笑脸（抛物线嘴角） |
| FACE_ALERT | 避障警戒 | 方形嘴（边框描点） |
| FACE_SAD | 撞墙/卡住 | 哭脸（嘴角下垂） |

### 菜单系统（6 项）

| 序号 | 菜单项 | 操作 |
| --- | --- | --- |
| 0 | 工作模式 | 左右切换 Remote/Auto/Follow |
| 1 | 左轮微调 | 左右增减 |
| 2 | 右轮微调 | 左右增减 |
| 3 | 基准速度 | 左右增减 |
| 4 | 贪吃蛇 | OK 进入游戏 |
| 5 | WiFi 配网 | OK 进入配网界面 |

菜单支持光标闪烁、空闲超时自动退出。

---

## 编译与烧录

1. 用 Keil MDK-ARM V5 打开 `Project/STM32-EnvSense.uvprojx`
2. 确认芯片型号为 STM32F103ZE，选择下载器（ST-Link / J-Link）
3. `Project → Build Target`（或按 F7）编译
4. `Flash → Download`（或按 F8）烧录到芯片

编译前确保已安装 STM32F10x 标准外设库的 Pack 包。

---

## 使用说明

### 上电启动

1. 接通电源，OLED 显示 Creeper 开机画面（5 秒）
2. ESP8266 自动连接 WiFi → 连接华为云 MQTT Broker → 订阅 Topic
3. 进入默认模式（蓝牙遥控），OLED 显示笑脸 + 温湿度
4. 蜂鸣器播放启动提示音

### 按键操作

| 按键 | 功能 |
| --- | --- |
| KEY0 | 进入/退出菜单、游戏中方向 |
| KEY1 | 游戏中方向控制 |

### 蓝牙遥控

1. 手机安装蓝牙串口 APP（如 Serial Bluetooth Terminal）
2. 配对连接 HC-05 蓝牙模块
3. 发送方向指令控制小车

### 华为云 IoT

设备属性上报 JSON 格式（Topic: `$oc/devices/{device_id}/sys/properties/report`）：

```json
{
  "temperature": 25.3,
  "humidity": 60.0,
  "distance": 45.2,
  "snake_score": 5
}
```

云端可通过 `$oc/devices/{device_id}/sys/commands/#` 下发控制命令。

---

## 团队分工

| 成员 | 角色 | 负责模块 | 文件清单 |
| --- | --- | --- | --- |
| **成员A**（组长） | 通信+算法+整合 | ESP8266 驱动、MQTT 协议、WiFi 配置、空间扫描、自动避障状态机、跟随模式、串口通信、调试模块、主程序整合 | `esp8266.h/c` `esp8266_mqtt.h/c` `wificfg.h/c` `spatial.h/c` `avoid.h/c`(状态机) `follow.h/c` `usart.h/c` `debug.h/c` `main.c` |
| **成员B** | 显示与交互 | OLED 驱动、字库、UI 界面、UI 资源、WiFi 动画、Creeper 开机画面、菜单系统、贪吃蛇游戏 | `oled.h/c` `oledfont.h/c` `ui.h/c` `ui_res.h/c` `ui_wifi.h/c` `creeper.h/c` `menu.h/c` `snake.h/c` |
| **成员C** | 传感器与执行器 | 电机驱动、舵机驱动、超声波测距、红外避障传感器、DHT11 温湿度、SysTick 延时 | `motor.h/c` `servo.h/c` `sr04.h/c` `avoid.h/c`(红外) `dht11.h/c` `delay.h/c` |
| **成员D** | 基础外设驱动 | LED 指示灯、蜂鸣器、按键、系统初始化、位图资源 | `led.h/c` `beep.h/c` `key.h/c` `sys.c` `bmp.h/c` |

### 分工说明

- **成员A（组长，任务最多、难度最高）**：9 个模块，涵盖 ESP8266 AT 指令交互、MQTT 协议栈实现、空间扫描最长扇区决策算法、三级递进避障状态机、跟随控制逻辑，以及最终 main.c 全系统整合。涉及网络协议、实时控制算法和多模块联调，是整个项目技术难度最高的部分。
- **成员B**：8 个模块，基于 SSD1306 I2C 的 OLED 底层驱动及上层 UI 应用（表情动画、菜单状态机、贪吃蛇游戏），需要理解帧缓冲机制和界面状态管理，独立性强。
- **成员C**：6 个模块，电机 PWM 控制、舵机角度映射、超声波输入捕获测距、红外 GPIO 检测、DHT11 单总线协议、SysTick 延时。各模块独立，难度适中。
- **成员D（技术较弱，任务最少）**：5 个模块，全部为单一 GPIO 操作或静态数据——LED（GPIO 输出）、蜂鸣器（GPIO 输出+延时）、按键（GPIO 输入+EXTI 中断）、系统时钟配置、位图数组。每个模块独立、逻辑极简，无需理解任何通信协议或算法。建议最先完成，为其他成员提供 `sys`、`key` 等基础接口。

### 模块依赖关系

```text
成员D (基础: sys/key/led/beep/bmp)
  │
  ├──→ 成员C (传感器+执行器: motor/servo/sr04/IR/dht11/delay)
  │       │
  │       ├── motor/servo/sr04 ──→ 成员A (避障/跟随/空间扫描)
  │       │
  │       └── delay ──→ 成员A (主循环时序)
  │
  └──→ 成员B (OLED/字库 → UI/菜单/贪吃蛇)
          │
          └──→ 成员A (main.c 整合显示与主循环)
```

- 成员D 最先完成（约 1 天），提供 `sys`、`key` 基础接口
- 成员C 与成员B 拿到基础接口后即可并行开发
- 成员A 拿到 `usart`（自己负责）后开始 ESP8266/MQTT 调试
- 成员A 拿到 `motor`、`servo`、`sr04` 后调试避障/跟随/空间扫描
- 成员A 最后汇总 B、C、D 的所有模块到 `main.c`，统一联调

---

## 许可证

本项目为嵌入式课程设计/毕业设计项目，仅供学习参考。
