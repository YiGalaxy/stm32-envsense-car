/********************************************************
 * 文件: follow.c
 * 描述: 跟随模式实现文件
 *       超声波锁定前方物体，保持目标距离实现自动跟随
 *       工作流程: 搜索目标 → 锁定跟随 → 丢失重搜
 *       跟随距离: 20cm(死区±5cm)
 *       搜索范围: 前方60°~120°(舵机扫描)
 ********************************************************/
#include "follow.h"
#include "sr04.h"
#include "servo.h"
#include "motor.h"
#include "delay.h"
#include "ui.h"
#include "usart.h"

/* 跟随目标距离(单位cm)：车与目标保持的理想距离 */
#define TARGET_CM   20

/* 死区范围(单位cm)：实际距离在TARGET_CM±DEADBAND内时停止，防止频繁启停抖动 */
#define DEADBAND    5

/* 最大搜索距离(单位cm)：超过此距离视为目标丢失 */
#define MAX_RANGE   80

/* 最大搜索轮数：连续搜索SEARCH_MAX次未找到目标则原地旋转重新搜索 */
#define SEARCH_MAX  3

/**
 * @brief  跟随模式主任务
 * @note   在主循环中周期调用，实现目标搜索→锁定→跟随→丢失重搜的闭环
 *
 *   状态0 - 搜索阶段:
 *     舵机扫过三个角度(60°/90°/120°)，分别测量距离
 *     取三个方向中最短有效距离作为目标
 *     若距离 < MAX_RANGE(80cm) 则认为找到目标 → 切换状态1
 *     若未找到，尝试SEARCH_MAX(3)次后原地旋转(400ms)重新搜索
 *     搜索过程显示悲伤表情
 *
 *   状态1 - 跟随阶段:
 *     舵机固定在90°正前方，持续测距
 *     距离控制策略:
 *       距离 < TARGET_CM - DEADBAND(15cm) → 后退(靠太近)
 *       距离 > TARGET_CM + DEADBAND(25cm) → 前进(靠太远)
 *       距离在[15,25]cm范围内           → 停止(到位)
 *     若距离超出有效范围(≤0或>80cm)表示目标丢失 → 切回状态0重新搜索
 *
 *   模式切换保护:
 *     使用last_mode静态变量检测work_mode变化
 *     切换进入跟随模式时自动复位状态机和舵机位置
 */
void Follow_Task(void)
{
    static u8 init = 0;          /* 初始化标志：首次运行或模式切换时需复位 */
    static u8 state = 0;         /* 状态: 0=搜索目标, 1=锁定跟随 */
    static u8 tries = 0;         /* 搜索失败计数 */
    float d;                     /* 当前测距结果 */

    /* 模式切换检测：当从其他模式切回跟随模式时自动重置 */
    {
        static u8 last_mode = 0xFF;  /* 上一次记录的运行模式(初值不可能出现的值) */
        if (last_mode != work_mode) {
            init = 0;                /* 标记需要重新初始化 */
            last_mode = work_mode;   /* 更新记录 */
        }
    }

    /* 首次运行或模式切换后的初始化 */
    if (!init) {
        Servo_SetAngle(90);          /* 舵机回正 */
        delay_ms(200);               /* 等待舵机到位 */
        state = 0;                   /* 从搜索状态开始 */
        tries = 0;                   /* 重置失败计数 */
        init = 1;                    /* 标记已初始化 */
    }

    /* =================== 状态0: 搜索目标 =================== */
    if (state == 0) {
        /* 舵机三角度扫描: 左60° → 右120° → 中90°，分别测距 */
        Servo_SetAngle(60);  delay_ms(100); float dl = SR04_GetDistance();  /* 左侧60°测距 */
        Servo_SetAngle(120); delay_ms(100); float dr = SR04_GetDistance();  /* 右侧120°测距 */
        Servo_SetAngle(90);  delay_ms(100); float dc = SR04_GetDistance();  /* 正前方90°测距 */

        /* 取三个方向中最近的有效距离作为候选目标 */
        float best = 999;                            /* 初始化最近距离为极大值 */
        if (dl > 0 && dl < best) best = dl;          /* 左侧有效且更近 */
        if (dr > 0 && dr < best) best = dr;          /* 右侧有效且更近 */
        if (dc > 0 && dc < best) best = dc;          /* 前方有效且更近 */

        if (best < MAX_RANGE) {
            /* 找到目标(距离在有效范围内) */
            state = 1;                               /* 切换到跟随状态 */
            tries = 0;                               /* 重置搜索计数 */
            UI_SetMode(FACE_NORMAL);                  /* 显示正常表情 */
        } else {
            /* 未找到目标 */
            tries++;
            Motor_Stop();
            if (tries >= SEARCH_MAX) {
                /* 连续搜索多次未果 → 原地旋转搜索不同方向 */
                tries = 0;
                Motor_Left(200);                     /* 左转寻找 */
                delay_ms(400);
                Motor_Stop();
            }
            UI_SetMode(FACE_SAD);                    /* 显示悲伤表情(目标丢失) */
        }
        return;
    }

    /* =================== 状态1: 跟随阶段 =================== */
    /* 舵机固定在正前方持续跟随 */
    d = SR04_GetDistance();                          /* 正前方测距 */

    /* 目标丢失检测 */
    if (d <= 0 || d > MAX_RANGE) {
        state = 0;                                   /* 切换到搜索状态 */
        Motor_Stop();                                /* 停车 */
        UI_SetMode(FACE_SAD);                        /* 显示目标丢失 */
        return;
    }

    /* 距离PID控制(简化为三段式) */
    if (d < TARGET_CM - DEADBAND) {
        Motor_Backward(300);                         /* 太近(<15cm)：后退远离 */
        UI_SetMode(FACE_ALERT);                      /* 警告表情(距过近) */
    } else if (d > TARGET_CM + DEADBAND) {
        Motor_Forward(300);                          /* 太远(>25cm): 前进靠近 */
        UI_SetMode(FACE_NORMAL);                     /* 正常表情 */
    } else {
        Motor_Stop();                                /* 在目标距离范围内(15~25cm)：停止 */
        UI_SetMode(FACE_NORMAL);                     /* 正常表情(到位) */
    }
}
