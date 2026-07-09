# 成员B/C/D 实训报告大纲

> **注意**：第一、二章直接使用《实训报告-第一二章.md》模板内容，第三至六章按下面对应成员的大纲填写，不得写错自己负责的模块。

---

## 成员B — 显示与交互

**负责模块**：OLED 驱动、字库、UI 界面、UI 资源、WiFi 动画、Creeper 开机画面、菜单系统、贪吃蛇游戏

| 文件 | 说明 |
| --- | --- |
| `oled.h/c` | SSD1306 OLED I2C 驱动（GPIO 模拟 I2C，SCL=GPIOG.12, SDA=GPIOD.5, RES=GPIOD.4，帧缓冲 1024 字节，画点、字符串、位图、中文显示、刷新） |
| `oledfont.h/c` | 字库数据（ASCII 6×8 / 中文 16×16） |
| `ui.h/c` | UI 界面（表情系统：FACE_NORMAL/ALERT/SAD 三种表情，眨眼动画，温湿度更新，Creeper 开机画面，WiFi 动画） |
| `ui_res.h/c` | UI 位图资源（笑脸/警戒/哭脸/表情帧等） |
| `ui_wifi.h/c` | WiFi 信号动画位图（wifi1~wifi4 四帧） |
| `creeper.h/c` | Creeper 64×64 开机画面位图 |
| `menu.h/c` | 6 项菜单系统（模式切换/左轮微调/右轮微调/基准速度/贪吃蛇入口/WiFi 配网入口），光标闪烁，空闲超时自动退出 |
| `snake.h/c` | 贪吃蛇游戏（16×8 网格、方向控制 SNAKE_UP/DOWN/LEFT/RIGHT、最大蛇长 128、4 帧/步、蓝牙方向控制） |

### 第三章 需求分析与概要设计

**3.1 需求分析**（写你负责的部分）
- OLED 需要显示什么内容：表情、温湿度、菜单、游戏、开机画面
- UI 界面需要哪些交互：三种表情切换、眨眼动画、温湿度数值更新、状态消息提示（底部栏，25 帧自动消失）
- 菜单需要几项功能：6 项（模式切换、左轮微调、右轮微调、基准速度、贪吃蛇入口、WiFi 配网入口），光标闪烁（500ms 周期），空闲 30 秒超时自动退出
- 贪吃蛇游戏基本规则：16×8 网格、方向键控制、吃到食物加分（最大蛇长 128）、撞墙/撞自己结束

**3.2 概要设计**
- 3.2.1 总体设计：画出你负责的模块之间的关系图
  ```text
  OLED 驱动 → 字库 (ASCII 6×8 / 中文 16×16)
        ↓
  UI 界面 ← UI 资源 / WiFi 动画帧 / Creeper 开机画面位图
        ↓
  菜单系统  +  贪吃蛇游戏
  ```
- 3.2.2 功能模块设计：列出每个模块的功能一句话描述（用表格）

**3.3 OLED 显示驱动设计**
- SSD1306 简介：128×64 像素、I2C 接口、GPIO 模拟时序（SCL=GPIOG.12, SDA=GPIOD.5, RES=GPIOD.4）
- 帧缓冲机制：1024 字节缓冲区 → 整屏刷新（OLED_Refresh）
- 显示接口：OLED_DrawPoint（单点）、OLED_ShowString（ASCII）、OLED_ShowChinese（中文）、OLED_ShowImage（位图）
- 字库取模方式：逐列式/逐行式，ASCII 6×8 和中文 16×16

### 第四章 详细设计与实现

**4.1 开发环境介绍**
- 同通用模板，加上 OLED 取模工具（PCtoLCD2002）和位图转换工具（Image2Lcd）

**4.2 OLED 驱动实现**
- 实现方法：GPIO 模拟 I2C 时序（SCL=PB8, SDA=PB9），SSD1306 初始化序列（时钟分频/电荷泵/对比度/全屏点亮/清除），帧缓冲写入，OLED_Refresh 批量 I2C 写入
- 实现结果：显示正常，刷新率约 60Hz，无闪烁
- 关键代码：`OLED_Init()` 初始化序列 + `OLED_Refresh()` 1024 字节帧缓冲刷新 + `OLED_DrawPoint()` 单点绘制

**4.3 UI 界面实现**
- 实现方法：三种表情（笑脸/警戒/哭脸）通过位图数据绘制，眨眼动画（定时切换眼睛帧），温湿度数值叠加显示（UI_UpdateTH），状态消息底部栏（UI_ShowMsg，25 帧自动消失），WiFi 信号动画（4 帧循环，UI_ShowWiFi），Creeper 开机画面（64×64 位图，显示 5 秒）
- 实现结果：表情刷新流畅，眨眼动画自然，温湿度更新及时
- 关键代码：`UI_Init()` 开机画面 + `UI_SetMode()` 表情切换 + `UI_Task()` 定时刷新 + `UI_UpdateTH()` 数值更新

**4.4 菜单系统实现**
- 实现方法：6 项菜单列表（MENU_ITEMS=6），光标移动（Menu_Up/Down，步进 1），参数增减（Menu_Left/Right，步进 10），确认进入（Menu_Ok：项 4 进贪吃蛇，项 5 进 WiFi 配网），空闲 30 秒自动退出，光标闪烁（500ms 周期）
- 实现结果：菜单操作流畅，光标闪烁清晰，超时退出正常，参数调整生效
- 关键代码：`Menu_Enter/Up/Down/Left/Right/Ok/Exit/Task/IsActive` 函数集

**4.5 贪吃蛇游戏实现**
- 实现方法：16 列 × 8 行网格坐标系（可容纳最大蛇长 128），方向控制（SNAKE_UP/DOWN/LEFT/RIGHT，禁止掉头），随机食物生成，碰撞检测（墙壁/自身），游戏速度 4 帧/步（Snake_Task 中 tick 累加），蓝牙方向控制（Snake_SetDir 回调）
- 实现结果：游戏可玩，分数正常累加（Snake_GetScore），撞墙结束动画正常，蓝牙方向控制响应及时
- 关键代码：`Snake_Task()` 主逻辑 + tick_fn() 蛇移动 + 食物生成算法 + `Snake_SetDir()` 方向控制

**4.6 呈现效果展示**
- （放 OLED 实物照片：开机 Creeper 画面、笑脸表情、菜单界面、贪吃蛇游戏画面）

### 第五章 总结和展望

- 总结：通过本项目掌握了 I2C 通信协议、OLED 显示驱动、嵌入式 UI 设计和简单游戏开发
- 展望：可增加更多表情、动画帧数、游戏难度等级、高分记录保存

---

## 成员C — 传感器与执行器

**负责模块**：电机驱动、舵机驱动、超声波测距、红外避障传感器、DHT11 温湿度、SysTick 延时

| 文件 | 说明 |
| --- | --- |
| `motor.h/c` | TB6612 双路电机驱动（TIM4 双通道 PWM，PB6=右轮, PB7=左轮，频率 1kHz，CCR 范围 0~999，PB12~PB15 方向控制，差速转向，左右轮独立微调 left_trim/right_trim） |
| `servo.h/c` | SG90 舵机控制（TIM2_CH1 PA0，50Hz PWM，脉宽 0.5ms~2.5ms 对应 0°~180° 线性映射） |
| `sr04.h/c` | HC-SR04 超声波测距（Trig=PA7，Echo=PA6/TIM3_CH1 输入捕获，脉宽/58=距离 cm，超时 40ms 返回 -1） |
| `avoid.h/c`（红外部分） | 4 路红外传感器 GPIO 初始化（PF5~PF8 上拉输入，PF7=左红外, PF8=右红外，低电平=有障碍），仅 `IR_Init()` 在本文件，检测通过 `PFin(7)/PFin(8)` 宏 |
| `dht11.h/c` | DHT11 单总线温湿度读取（PA1 双向 GPIO，起始信号 >18ms，40 位数据：湿度整/小 + 温度整/小 + 校验和） |
| `delay.h/c` | SysTick 毫秒/微秒延时 |

### 第三章 需求分析与概要设计

**3.1 需求分析**（写你负责的部分）
- 电机需要实现什么：前进/后退/左转/右转/停止、PWM 调速（0~999）、差速转向、左右轮独立微调（left_trim/right_trim，范围 -300~+300）
- 舵机需要实现什么：0°~180° 角度控制，用于超声波云台旋转（步进 15° 扫描 9 扇区）
- 超声波需要实现什么：精确测距（2cm~400cm），Trig/PA7 触发 10μs 脉冲，Echo/PA6 输入捕获测脉宽
- 红外传感器需要实现什么：4 路近距障碍检测（PF5~PF8），低电平触发（有障碍=0），PF7=左, PF8=右
- DHT11 需要实现什么：定期读取温湿度（PA1），校验和验证，读取间隔 ≥1 秒
- 延时需要实现什么：毫秒级（delay_ms）和微秒级（delay_us）精确延时

**3.2 概要设计**
- 3.2.1 总体设计：画出模块关系图
  ```text
  SysTick 延时（基础时钟）
        ↓
  电机 TIM4 PWM ─→ 舵机 TIM2 PWM ─→ 超声波 TIM3 输入捕获
        ↓                 ↓
  红外 GPIO PF5~8    DHT11 GPIO PA1
  ```
- 3.2.2 功能模块设计：每个模块一句话描述 + 硬件引脚 + 关键技术（用表格）

**3.3 传感器接口设计**
- 各传感器与 MCU 的接口类型：PWM（电机/舵机）、GPIO 输入捕获（超声波）、GPIO 上拉输入（红外）、单总线（DHT11）
- DHT11 单总线通信时序：主机拉低 >18ms → 释放 20~40μs → DHT 响应 80μs 低 + 80μs 高 → 40 位数据（每位：50μs 低 + 26~70μs 高，延时 40μs 后采样）

### 第四章 详细设计与实现

**4.1 开发环境介绍**
- 同通用模板，加上示波器/逻辑分析仪（用于调试 PWM 和单总线时序）

**4.2 电机驱动实现**
- 实现方法：TIM4 双通道 PWM（CH1=PB6→右轮, CH2=PB7→左轮），频率 1kHz（72MHz/72/1000），CCR 范围 0~999。PB12/PB13=右轮方向 IN1/IN2，PB14/PB15=左轮方向 IN3/IN4。Motor_SetSpeed() 传入有符号速度值（正=前进,负=后退），Motor_Forward() 内置 left_trim/right_trim 补偿机械偏差，Motor_Backward() 不应用 trim。
- 实现结果：PWM 波形正常，调速线性度好，差速转向灵活，trim 微调有效修正跑偏
- 关键代码：`Motor_Init()` PWM+GPIO 初始化 + `Motor_SetSpeed()` 符号判断及方向切换 + `Motor_Forward()/Backward()` 速度/微调叠加

**4.3 舵机驱动实现**
- 实现方法：TIM2 PWM，频率 50Hz（周期 20ms），PA0 输出。脉宽 0.5ms(CCR=500)~2.5ms(CCR=2500) 对应 0°~180° 线性映射（CCR = 500 + angle × 2000/180），角度自动钳位 0~180° 保护舵机
- 实现结果：角度控制精度 ±2°，转动平滑，响应时间约 50ms
- 关键代码：`Servo_Init()` 50Hz PWM 配置 + `Servo_SetAngle()` 角度→CCR 映射公式

**4.4 超声波测距实现**
- 实现方法：Trig(PA7) 发 10μs 脉冲触发 → TIM3_CH1(PA6) 输入捕获 Echo 上升沿和下降沿 → 脉宽差值 = 高电平时间 → 距离 = 脉宽(μs) / 58(cm)。捕获中断处理：首次上升沿记录 CaptureValue1，切换为下降沿捕获；下降沿记录 CaptureValue2，计算 PulseWidth，置位 SR04_Finish。轮询等待最多 40ms（4000×10μs），超时返回 -1
- 实现结果：测距精度 ±1cm（2cm~200cm 范围），刷新率约 10~20Hz
- 关键代码：`TIM3_IRQHandler()` 捕获中断 + `SR04_GetDistance()` 触发+轮询+换算

**4.5 DHT11 温湿度实现**
- 实现方法：PA1 双向 GPIO。DHT_Init_OutPut() → 拉低 >18ms 起始信号 → DHT_Init_InPut() → 等待 DHT11 响应（80μs 低 + 80μs 高）→ DHT_ReadByte() ×5 读取 40 位数据（湿度整/小 + 温度整/小 + 校验和）→ 校验验证（前 4 字节和 = 第 5 字节）。位判断：延时 40μs 后采样，高电平持续约 26μs=0，约 70μs=1
- 实现结果：读取成功率 >95%，温湿度数据准确
- 关键代码：`DHT_ReadData()` 完整通信流程 + `DHT_ReadBit()/DHT_ReadByte()` 位/字节读取

**4.6 红外避障传感器实现**
- 实现方法：PF5~PF8 配置为上拉输入（GPIO_Mode_IPU），PF7=左红外, PF8=右红外，低电平=检测到障碍。上层通过 `PFin(7)/PFin(8)` 宏读取状态，由 avoid.c 中的 Mode_Avoid() 调用
- 实现结果：响应速度 <1ms，检测距离约 5cm~15cm（传感器物理特性）
- 关键代码：`IR_Init()` GPIO 配置 + `PFin(7)/PFin(8)` 检测宏

**4.7 呈现效果展示**
- （放示波器 PWM 波形截图、DHT11 串口输出数据截图、小车实际运动照片）

### 第五章 总结和展望

- 总结：通过本项目掌握了 PWM 电机控制、定时器输入捕获、单总线通信协议和传感器驱动开发
- 展望：可升级为编码器闭环速度控制、使用 DHT22 替代 DHT11 提高精度

---

## 成员D — 基础外设驱动

**负责模块**：LED 指示灯、蜂鸣器、按键、系统初始化、位图资源

| 文件 | 说明 |
| --- | --- |
| `led.h/c` | LED0(PB5) + LED1(PE5) GPIO 推挽输出，低电平点亮，提供 `led_init()` 初始化 |
| `beep.h/c` | 蜂鸣器 PF0 GPIO 推挽输出，支持 `beep_on(ms)`（简易 2kHz 方波）、`beep_tone(freq, ms)`（指定频率音调）、`beep_sprinkler()`（洒水车音乐旋律，C5~C6 音阶） |
| `key.h/c` | KEY0(PE4) + KEY1(PE3) 上拉输入 + EXTI 下降沿中断，NVIC 优先级 KEY1=2/2, KEY0=3/3，中断服务函数中包含 LED 指示 |
| `sys.c` | NVIC 中断优先级分组配置（NVIC_PriorityGroup_2，2 位抢占 + 2 位响应），系统时钟由 system_stm32f10x.c 配置 HSE 8MHz→PLL×9=72MHz |
| `bmp.h/c` | 内置位图数据数组（BMP1/BMP2 等静态位图数据，逐列逐页格式） |

### 第三章 需求分析与概要设计

**3.1 需求分析**（写你负责的部分）
- LED 需要实现什么：两个板载指示灯（LED0=PB5, LED1=PE5），低电平点亮，可用于系统状态指示和调试信号
- 蜂鸣器需要实现什么：有源蜂鸣器 PF0，支持指定频率方波（beep_tone）、固定 2kHz 简易响铃（beep_on）、预置洒水车音乐旋律（beep_sprinkler）
- 按键需要实现什么：两个按键（KEY0=PE4, KEY1=PE3），上拉输入+EXTI 下降沿中断，KEY1 优先级高于 KEY0（NVIC 2/2 vs 3/3），中断中包含 LED 指示逻辑
- 系统初始化需要实现什么：配置 NVIC 中断优先级分组为组 2（2 位抢占 + 2 位响应），系统时钟由启动文件自动配置为 HSE+PLL 72MHz

**3.2 概要设计**
- 3.2.1 总体设计：画出模块关系图
  ```text
  sys.c（NVIC 分组 → 供所有中断使用）
        ↓
  LED (PB5/PE5)   蜂鸣器 (PF0)   按键 (PE3/PE4)
   推挽输出          推挽输出     上拉输入 + EXTI
                                     ↓
                                EXTI3_IRQHandler (KEY1)
                                EXTI4_IRQHandler (KEY0)
  ```
- 3.2.2 功能模块设计：每个模块一句话描述 + 硬件引脚 + 实现方式（用表格）

**3.3 基础接口设计**
- sys.c 提供的 NVIC 分组：NVIC_PriorityGroup_2（2 位抢占 + 2 位响应）
- 系统时钟：HSE 8MHz 外部晶振 → PLL 倍频 ×9 = 72MHz，APB1=36MHz, APB2=72MHz（配置在 system_stm32f10x.c 中）
- key 接口：两个按键独立 EXTI 中断线（EXTI3=KEY1, EXTI4=KEY0），优先级 KEY1 > KEY0，中断中带 LED 指示

### 第四章 详细设计与实现

**4.1 开发环境介绍**
- 同通用模板，你的模块最简单，不需要额外工具

**4.2 LED 驱动实现**
- 实现方法：PB5(LED0) 和 PE5(LED1) 配置为推挽输出（GPIO_Mode_Out_PP），初始输出高电平（GPIO_SetBits，LED 熄灭），低电平点亮
- 实现结果：LED 正常亮灭，可指示系统运行状态
- 关键代码：`led_init()` GPIO 时钟使能 + 初始化 + 默认熄灭

**4.3 蜂鸣器驱动实现**
- 实现方法：PF0 配置为推挽输出。beep_on(ms) 在指定时长内以 500μs 周期（2kHz）翻转 PF0 输出方波，结束后输出低电平。beep_tone(freq, ms) 根据频率计算半周期微秒数（500000/freq），循环翻转 PF0 产生指定频率方波，freq=0 时不发音仅延时（休止符）。beep_sprinkler() 使用音符-时长对数组定义洒水车旋律（C5~C6 音阶），遍历调用 beep_tone() 播放
- 实现结果：音调准确，音乐播放流畅
- 关键代码：`beep_tone()` 频率→半周期换算 + PF0 翻转 + beep_sprinkler() 旋律数组

**4.4 按键驱动实现**
- 实现方法：PE3(KEY1) 和 PE4(KEY0) 配置为上拉输入（GPIO_Mode_IPU），GPIO_EXTILineConfig 映射到 EXTI3/EXTI4，配置为下降沿触发中断（EXTI_Trigger_Falling）。NVIC 优先级：EXTI3=抢占 2/响应 2（KEY1），EXTI4=抢占 3/响应 3（KEY0）。中断服务函数中先关闭所有 LED，再点亮对应按键的指示灯并保持 1 秒。提供 KEY_EXTI_Init() 一站式初始化
- 实现结果：按键响应灵敏，无误触发，中断优先级合理
- 关键代码：`KEY_EXTI_Init()` → key_init() + GPIO_EXTILineConfig + EXTI_Init + NVIC_Init + `EXTI3_IRQHandler()/EXTI4_IRQHandler()` 中断处理

**4.5 系统初始化实现**
- 实现方法：NVIC_Configuration() 设置中断优先级分组为 NVIC_PriorityGroup_2（2 位抢占优先级 + 2 位响应优先级）。系统时钟在 system_stm32f10x.c 中配置：HSE 外部 8MHz 晶振 → PLL 倍频 ×9 = 72MHz 系统时钟，APB1 预分频 /2=36MHz，APB2 不分频=72MHz
- 实现结果：系统稳定运行在 72MHz，中断优先级正常
- 关键代码：`NVIC_Configuration()` 调用 `NVIC_PriorityGroupConfig()`

**4.6 位图资源**
- 实现方法：bmp.h/c 存储预定义的静态位图数组（BMP1、BMP2 等），数据采用逐列逐页格式，与 SSD1306 GDDRAM 存储格式一致，供 OLED 驱动直接调用（OLED_ShowImage/OLED_ShowPicture）
- 实现结果：位图显示正常
- 关键代码：位图数组定义（展示部分数据即可）

**4.7 呈现效果展示**
- （放 LED 亮灭照片、蜂鸣器驱动的示波器波形、按键按下的实物照片）

### 第五章 总结和展望

- 总结：通过本次实训，掌握了 STM32 GPIO 外设操作、EXTI 外部中断、NVIC 中断管理和 SysTick 系统定时器的基础知识，了解了嵌入式系统的时钟树结构和外设初始化流程
- 展望：后续可学习更复杂的外设（如 ADC 模拟采集、DMA 直接内存访问）

---
