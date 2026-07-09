/********************************************************
 * 文件: follow.c
 * 描述: 跟随模式实现文件
 *       超声波锁定前方目标，保持20cm距离自动跟随
 *       工作流程: 搜索目标 → 锁定跟随(比例速度) → 丢失重搜(带二次确认+小角度重捕获)
 *       跟随距离: 20cm(死区±5cm)
 *       搜索范围: 前方60°~120°(舵机扫描)
 *
 * 改进说明:
 *   1. 比例速度控制 — 距离偏差越大速度越快，到位前自然减速，消除抖动
 *   2. 丢失二次确认 — 连续2周期无数据才判丢，抗超声波偶发空值
 *   3. 小角度重捕获 — 丢失前扫描±20°，目标偏移时自动转向追踪
 *   4. 方向换向缓冲 — 前进↔后退间插入一个周期停止，消除机械冲击
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

/* 跟随目标距离(单位cm)：车与目标保持的理想距离 */
#define TARGET_CM   15

/* 死区范围(单位cm)：实际距离在TARGET_CM±DEADBAND内时停止，防止频繁启停抖动 */
#define DEADBAND    5

/* 最大搜索距离(单位cm)：超过此距离视为目标丢失 */
#define MAX_RANGE   80

/* 最大搜索轮数：连续搜索SEARCH_MAX次未找到目标则原地旋转重新搜索 */
#define SEARCH_MAX  3

/* 丢失确认次数：连续LOST_THRESHOLD次测不到才判丢失 */
#define LOST_THRESHOLD  2

/* 速度比例系数：距离偏差每超过死区1cm,速度增加 KP 个单位 */
#define FOLLOW_KP   12

/**
 * @brief  跟随模式主任务
 * @note   在主循环中周期调用，实现目标搜索→锁定跟随→丢失重搜的闭环
 *
 *   状态0 - 搜索阶段:
 *     舵机扫过三个角度(60°/90°/120°)，分别测量距离
 *     取三个方向中最短有效距离作为目标
 *     若距离 < MAX_RANGE(80cm) 则认为找到目标 → 切换状态1
 *     若未找到，尝试SEARCH_MAX(3)次后原地旋转(400ms)重新搜索
 *     搜索过程显示悲伤表情
 *
 *   状态1 - 跟随阶段(比例速度控制):
 *     舵机固定在90°正前方，每周期测距一次
 *     计算距离偏差 error = d - TARGET_CM
 *       目标速度 = error × 比例系数 + 基速(180)
 *       偏差越大速度越快,到位前自动减速,消除急停抖动
 *     若连续LOST_THRESHOLD次无有效回波:
 *       1. 扫描±20°尝试重捕获,目标偏移则转向追踪
 *       2. 重捕获失败 → 切回状态0重新搜索
 *
 *   方向换向保护:
 *     通过hold_speed记录当前电机方向
 *     检测到方向反转需求时先停止一个周期再启动,避免机械冲击
 *
 *   模式切换保护:
 *     使用last_mode静态变量检测work_mode变化
 *     切换进入跟随模式时自动复位状态机和舵机位置
 */
void Follow_Task(void)
{
    static u8 init = 0;              /* 初始化标志：首次运行或模式切换时需复位 */
    static u8 state = 0;             /* 状态: 0=搜索目标, 1=锁定跟随 */
    static u8 tries = 0;             /* 搜索失败计数 */
    static u8 lost_cnt = 0;          /* 丢失确认计数器：连续测不到次数 */
    static s16 hold_speed = 0;       /* 当前电机速度保持值(正=前进,负=后退,0=停止),用于换向缓冲 */
    static float dl=0, dr=0, dc=0;   /* 三方向测距缓存(搜索后保持,减少局部变量) */
    float d;                         /* 当前测距结果 */
    s16 error, target_speed;         /* 距离偏差, 目标速度 */

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
        lost_cnt = 0;                /* 重置丢失计数 */
        hold_speed = 0;              /* 重置速度保持 */
        init = 1;                    /* 标记已初始化 */
        return;                      /* 本周期仅做初始化,下一周期开始工作 */
    }

    /* =================== 状态0: 搜索目标 =================== */
    if (state == 0) {
        /* 舵机三角度扫描: 左60° → 右120° → 中90°，分别测距 */
        Servo_SetAngle(60);  delay_ms(100); dl = SR04_GetDistance();  /* 左侧60°测距 */
        Servo_SetAngle(120); delay_ms(100); dr = SR04_GetDistance();  /* 右侧120°测距 */
        Servo_SetAngle(90);  delay_ms(100); dc = SR04_GetDistance();  /* 正前方90°测距 */

        /* 取三个方向中最近的有效距离作为候选目标 */
        float best = 999;                            /* 初始化最近距离为极大值 */
        if (dl > 0 && dl < best) best = dl;          /* 左侧有效且更近 */
        if (dr > 0 && dr < best) best = dr;          /* 右侧有效且更近 */
        if (dc > 0 && dc < best) best = dc;          /* 前方有效且更近 */

        if (best < MAX_RANGE) {
            /* 找到目标(距离在有效范围内) */
            state = 1;                               /* 切换到跟随状态 */
            tries = 0;                               /* 重置搜索计数 */
            lost_cnt = 0;                            /* 重置丢失计数 */
            hold_speed = 0;                          /* 重置速度保持 */
            UI_SetMode(FACE_NORMAL);                  /* 显示正常表情 */
        } else {
            /* 未找到目标 */
            tries++;
            Motor_Stop();
            if (tries >= SEARCH_MAX) {
                /* 连续搜索多次未果 → 原地旋转搜索不同方向 */
                tries = 0;
                Motor_Left(speed * 5 / 8);              /* 原地旋转搜索(速度基于全局基准) */
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

    /*
     * 丢失确认机制:
     *   超声波偶有无回波(目标表面吸音/角度偏),单次无数据直接判丢会频繁误触发.
     *   连续LOST_THRESHOLD次无数据才确认丢失,抗偶发空值.
     *   确认丢失前再扫描±20°,看目标是否偏移到侧面.
     */
    if (d <= 0 || d > MAX_RANGE) {
        lost_cnt++;
        if (lost_cnt >= LOST_THRESHOLD) {
            /*
             * 小角度重捕获扫描:
             *   目标可能没有真正丢失,只是移到了侧面70°或110°方向.
             *   扫描左右各20°范围,发现有目标则转向追踪.
             */
            Servo_SetAngle(70);  delay_ms(100); float dl2 = SR04_GetDistance();
            Servo_SetAngle(110); delay_ms(100); float dr2 = SR04_GetDistance();
            Servo_SetAngle(90);  delay_ms(100);

            if ((dl2 > 0 && dl2 < MAX_RANGE) || (dr2 > 0 && dr2 < MAX_RANGE)) {
                /* 目标偏移到侧方 → 转向目标方向后继续跟随 */
                if (dl2 < dr2) { Motor_Left(speed * 7 / 10);   delay_ms(120); }
                else           { Motor_Right(speed * 7 / 10);  delay_ms(120); }
                Motor_Stop();
                lost_cnt = 0;
                hold_speed = 0;
                UI_SetMode(FACE_NORMAL);
                return;
            }

            /* 确认目标真正丢失 → 切回搜索 */
            state = 0;
            tries = 0;
            lost_cnt = 0;
            hold_speed = 0;
            Motor_Stop();
            UI_SetMode(FACE_SAD);
        } else {
            /* 首次丢失: 停车观望,等下一周期再确认 */
            Motor_Stop();
        }
        return;
    }
    lost_cnt = 0;                                    /* 有效数据,复位丢失计数 */

    /*
     * 比例速度控制(基于全局基准速度 speed):
     *   距离偏差 error = d - TARGET_CM  (>0偏远, <0偏近)
     *   目标速度 = speed + |error| × KP  (靠近时更快,到位前减速)
     *     前进上限: speed * 3/2 (如 speed=400 → 600)
     *     后退上限: speed          (后退比前进保守)
     *     后退基速: speed * 3/4   (默认比前进轻柔)
     *
     *   方向换向缓冲:
     *     前进↔后退直接切换会引起机械冲击和电流尖峰.
     *     检测到方向反转需求时,先停车一个周期(hold_speed置0),
     *     下一周期再启动新方向.
     */
    error = (s16)(d - TARGET_CM);                     /* 距离偏差: +太远, -太近 */

    if (error > DEADBAND) {
        /* 目标偏远: 前进靠近,速度 = 基速 + 偏差×系数 */
        target_speed = speed + (error - DEADBAND) * FOLLOW_KP;
        if (target_speed > speed * 3 / 2) target_speed = speed * 3 / 2;

        /* 换向缓冲: 当前正在后退 → 先停一个周期 */
        if (hold_speed < -50) {
            Motor_Stop();
            hold_speed = 0;
            return;
        }
        Motor_Forward((uint16_t)target_speed);
        hold_speed = target_speed;
        UI_SetMode(FACE_NORMAL);

    } else if (error < -DEADBAND) {
        /* 目标偏近: 后退拉开,基速降为 3/4 speed 更柔和 */
        target_speed = speed * 3 / 4 + (-error - DEADBAND) * FOLLOW_KP;
        if (target_speed > speed) target_speed = speed;

        /* 换向缓冲: 当前正在前进 → 先停一个周期 */
        if (hold_speed > 50) {
            Motor_Stop();
            hold_speed = 0;
            return;
        }
        Motor_Backward((uint16_t)target_speed);
        hold_speed = -target_speed;
        UI_SetMode(FACE_ALERT);

    } else {
        /* 到位停车 */
        Motor_Stop();
        hold_speed = 0;
        UI_SetMode(FACE_NORMAL);
    }
}
