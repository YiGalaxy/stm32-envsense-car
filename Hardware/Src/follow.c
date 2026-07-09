/********************************************************
 * 文件: follow.c
 * 描述: 跟随模式实现文件（简化版）
 *       超声波正前方测距，保持固定距离自动跟随
 *       偏远前进 / 偏近后退 / 到位停止
 *       跟随距离: 10~15cm
 ********************************************************/
#include "follow.h"
#include "sr04.h"
#include "servo.h"
#include "motor.h"
#include "delay.h"
#include "ui.h"
#include "usart.h"

/* 全局基准速度,在 usart.c 中定义,菜单可调 */
extern volatile u16 speed;

/* 跟随距离(单位cm)：10~15cm前进跟随，<10后退，>15停车 */
#define DIST_MIN    10
#define DIST_MAX    15

/**
 * @brief  跟随模式主任务
 * @note   在主循环中周期调用
 *         舵机固定正前方90°,每周期测距一次
 *         10~15cm → 前进跟随
 *         <10cm   → 后退拉开
 *         >15cm   → 停车等待
 *         无目标 → 停车等待
 */
void Follow_Task(void)
{
    static u8 init = 0;
    float d;

    /* 模式切换检测：从其他模式切回跟随时重新初始化 */
    {
        static u8 last_mode = 0xFF;
        if (last_mode != work_mode) {
            init = 0;
            last_mode = work_mode;
        }
    }

    /* 首次进入跟随模式：舵机回正 */
    if (!init) {
        Servo_SetAngle(90);
        delay_ms(200);
        init = 1;
        return;
    }

    /* 正前方测距 */
    d = SR04_GetDistance();

    /* 无效数据(超时) → 停车等待 */
    if (d <= 0) {
        Motor_Stop();
        UI_SetMode(FACE_SAD);
        return;
    }

    /* 根据距离决策 */
    if (d >= DIST_MIN && d <= DIST_MAX) {
        /* 10~15cm → 前进跟随 */
        Motor_Forward(speed);
        UI_SetMode(FACE_NORMAL);
    } else if (d < DIST_MIN) {
        /* <10cm → 后退拉开 */
        Motor_Backward(speed * 3 / 4);
        UI_SetMode(FACE_ALERT);
    } else {
        /* >15cm → 停车 */
        Motor_Stop();
        UI_SetMode(FACE_SAD);
    }
}
