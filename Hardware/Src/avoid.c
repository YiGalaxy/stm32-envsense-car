/******************************************************************************
 * 文件名: avoid.c
 * 描  述: 自动避障模块实现文件
 * 说  明: 三级递进避障策略:
 *         第一级 - 红外防撞(近距主动，优先响应)
 *         第二级 - 超声波空间扫描(9扇区180°环境探测)
 *         第三级 - 试探转向(左右各试3次) → 掉头(脱困)
 *         红外传感器响应最快用于紧急避撞，超声波用于空间路径规划
 ******************************************************************************/
#include "avoid.h"
#include "delay.h"
#include "motor.h"
#include "servo.h"
#include "sr04.h"
#include "spatial.h"
#include "ui.h"
#include "usart.h"

/* 从usart.c引用的全局变量：当前行驶速度(占空比/油门值) */
extern volatile u16 speed;

/* 超声波测距结果缓存 - 定义在main.c中，avoid.h中已extern声明
 * distance      : 前向距离(舵机90°)
 * distance_L    : 左侧距离(舵机0°)
 * distance_R    : 右侧距离(舵机180°)
 */

/**
 * @brief  红外传感器GPIO初始化
 * @note   配置PF5~PF8为上拉输入模式
 *         传感器输出低电平时表示检测到障碍物
 *         红外探头安装在车头左右两侧，用于近距(<10cm)防撞
 *         PF5/PF6 - 未使用(备用)
 *         PF7     - 左红外探头 (0=有障碍)
 *         PF8     - 右红外探头 (0=有障碍)
 */
void IR_Init(void)
{
    GPIO_InitTypeDef gpio;
    /* 使能GPIOF时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
    /* 配置PF5~PF8为上拉输入: 外部不接时默认高电平(无障碍) */
    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8;
    gpio.GPIO_Mode = GPIO_Mode_IPU;                     /* 上拉输入模式 */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;                 /* 输入模式下Speed无影响 */
    GPIO_Init(GPIOF, &gpio);
}

/**
 * @brief  自动避障模式主状态机
 * @note   三级递进避障策略，在每个主循环周期调用一次:
 *
 *   第一级 - 红外优先(状态0的子集):
 *     - 左右同时障碍: 后退 → 左转 → 前进
 *     - 左侧障碍: 停止 → 右转 → 前进
 *     - 右侧障碍: 停止 → 左转 → 前进
 *     - 红外每次响应后重置状态机
 *
 *   第二级 - 超声波空间扫描(状态0):
 *     - 前向超声波测距 < avoid_dist 时触发
 *     - 后退一小段 → Spatial_Scan(9扇区)
 *     - 依据扫描结果进入转向(状态2)或掉头(状态3)
 *
 *   第三级 - 试探转向(状态2):
 *     - 按Spatial_Scan推荐方向旋转(每次300ms)
 *     - 每个方向最多尝试3次，再测距确认
 *     - 若3次失败则换向，换向也失败则进入掉头
 *
 *   第四级 - 掉头(状态3):
 *     - 连续左转(每次350ms×6次=约180°)
 *     - 直到前向距离 > avoid_dist 或转够6次
 *
 *   状态转移图:
 *   [前进] ──障碍──→ [空间扫描] ──可通行──→ [试探转向] ──成功──→ [前进]
 *                                   │失败                    │
 *                                   └──→ [掉头] ←───────────┘
 *
 * @note   依赖全局变量: speed(速度), distance(前向测距), avoid_dist(避障阈值)
 *         依赖模块: SR04(超声波), Servo(舵机), Motor(电机), UI(表情), Spatial(空间扫描)
 */
void Mode_Avoid(void)
{
    static uint8_t state = 0;   /* 状态机当前状态: 0=前进, 1=空间扫描, 2=试探转向, 3=掉头 */
    static uint8_t dir = 0;     /* 当前尝试转向方向: 0=左转, 1=右转 */
    static uint8_t cnt = 0;     /* 当前方向尝试次数计数器 */
    static uint8_t tries = 0;   /* 切换方向次数(最多切换2次 = 左右都试过) */
    uint8_t irL, irR;           /* 左右红外传感器状态: 0=有障碍 */

    /* 获取前向超声波距离(舵机默认在90°位置) */
    distance = SR04_GetDistance();
    /* 读取左右红外传感器(上拉输入，低电平有效) */
    irL = PFin(7);               /* PF7: 左红外探头 */
    irR = PFin(8);               /* PF8: 右红外探头 */

    /* =================== 第一级：红外主动避障 =================== */
    /* 红外响应优先级最高，一旦触发立即执行并重置状态机 */

    /* 左右同时障碍 → 全面碰撞风险 */
    if (irL == 0 && irR == 0)
    {
        UI_SetMode(FACE_ALERT);                          /* 显示警告表情 */
        Motor_Stop();                                    /* 急停 */
        Motor_Backward(speed);                           /* 后退脱离 */
        delay_ms(500);
        Motor_Left(speed);                               /* 左转绕行 */
        delay_ms(400);
        Servo_SetAngle(90);                              /* 舵机回正 */
        Motor_Forward(speed);                            /* 恢复前进 */
        state = 0; cnt = 0;                              /* 重置状态机 */
        return;
    }
    /* 左侧障碍 → 右转规避 */
    if (irL == 0)
    {
        UI_SetMode(FACE_ALERT);
        Motor_Stop();
        Motor_Right(speed);                              /* 右转 */
        delay_ms(300);
        Servo_SetAngle(90);
        Motor_Forward(speed);
        state = 0; cnt = 0;
        return;
    }
    /* 右侧障碍 → 左转规避 */
    if (irR == 0)
    {
        UI_SetMode(FACE_ALERT);
        Motor_Stop();
        Motor_Left(speed);                               /* 左转 */
        delay_ms(300);
        Servo_SetAngle(90);
        Motor_Forward(speed);
        state = 0; cnt = 0;
        return;
    }

    /* =================== 第二级：超声波避障 =================== */
    /* distance < 1 表示超声波无回波(超出量程或未检测到目标) */
    /* distance > avoid_dist 表示前向距离足够安全 */
    if (distance < 1 || distance > avoid_dist)
    {
        /* 前向无障碍，正常前进 */
        Motor_Forward(speed);
        Servo_SetAngle(90);                              /* 舵机居前 */
        UI_SetMode(FACE_NORMAL);                         /* 正常表情 */
        state = 0;                                       /* 保持在前进状态 */
        return;
    }

    /* 前向有障碍(distance <= avoid_dist) → 进入避障流程 */
    /* 状态0 → 后退并执行空间扫描 */
    if (state == 0)
    {
        UI_SetMode(FACE_ALERT);
        Motor_Stop();
        Motor_Backward(speed);                           /* 先退一步腾出转向空间 */
        delay_ms(200);
        Motor_Stop();

        /* 执行9扇区空间扫描，返回最优通行方向 */
        uint8_t path = Spatial_Scan(avoid_dist);

        if (path == PATH_ESCAPE)
        {
            /* 所有扇区全部堵死 → 进入掉头脱困模式 */
            UI_SetMode(FACE_SAD);
            state = 3;
        }
        else
        {
            /* 存在可通行方向 → 进入试探转向 */
            dir = (path == PATH_RIGHT) ? 1 : 0;          /* 0=左转, 1=右转 */
            cnt = 0;                                     /* 重置尝试次数 */
            tries = 0;                                   /* 重置换向次数 */
            state = 2;
        }
        return;
    }

    /* =================== 第三级：试探转向(状态2) =================== */
    /* 按照空间扫描推荐方向尝试转向，允许每个方向最多转3次 */
    if (state == 2)
    {
        if (dir == 0) Motor_Left(speed); else Motor_Right(speed);  /* 按推荐方向旋转 */
        delay_ms(300);                                   /* 旋转约30°(依轮径和速度而定) */
        Motor_Stop();
        delay_ms(60);                                    /* 等待车身稳定 */
        cnt++;                                           /* 当前方向尝试次数+1 */

        /* 测距确认：转向后前向是否通畅 */
        if (SR04_GetDistance() > avoid_dist)
        {
            state = 0;                                   /* 找到通路，恢复前进 */
            Servo_SetAngle(90);
        }
        else if (cnt >= 3)
        {
            /* 当前方向尝试3次仍不通 → 换另一侧 */
            dir = !dir;                                   /* 切换转向方向 */
            cnt = 0;                                     /* 重置计数 */
            state = 2;                                   /* 继续试探 */
            tries++;                                     /* 换向次数+1 */
            /* 两个方向都试过3次仍不通 → 进入掉头 */
            if (tries >= 2)
            {
                tries = 0;
                state = 3;                               /* 升级到掉头 */
            }
        }
        return;
    }

    /* =================== 第四级：掉头(状态3) =================== */
    /* 连续左转实现180°掉头，寻找反向通路 */
    if (state == 3)
    {
        UI_SetMode(FACE_SAD);                            /* 悲伤表情(表示被困) */
        Motor_Left(speed);                               /* 持续左转掉头 */
        delay_ms(350);
        Motor_Stop();
        delay_ms(60);                                    /* 稳定等待 */
        cnt++;
        /* 掉头完成条件：转够6次(约180°) 或 前向已通畅 */
        if (cnt >= 6 || SR04_GetDistance() > avoid_dist)
        { state = 0; cnt = 0; Servo_SetAngle(90); }     /* 恢复前进 */
    }
}
