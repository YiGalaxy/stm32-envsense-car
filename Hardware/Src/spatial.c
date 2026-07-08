/******************************************************************************
 * 文件名: spatial.c
 * 描  述: 空间扫描模块实现文件
 * 说  明: 配合舵机+超声波实现前方180°扇形区域环境探测
 *         9扇区扫描(30°~150°，每15°一个扇区)
 *         采用"最长连续可通行扇区"算法决策最优通行方向
 *         用于自动避障模式中的路径规划
 ******************************************************************************/
#include "spatial.h"
#include "servo.h"
#include "sr04.h"
#include "delay.h"

/*
 * 扇区角度映射表（单位：度）
 * 索引:  0     1     2     3     4     5     6     7     8
 * 角度: 30°   45°   60°   75°   90°  105°  120°  135°  150°
 * 方位: 最左  左前  左前  左前  正中  右前  右前  右前  最右
 * 覆盖宽度: 180°(前方完整半圆)
 */
static const uint8_t g_angles[SCT_NUM] = {30, 45, 60, 75, 90, 105, 120, 135, 150};

/*
 * 距离地图缓冲区
 * 存储每个扇区最新的超声波测距结果，单位cm
 * 索引与g_angles一一对应
 * g_map[0] = 30°方向距离  ...  g_map[8] = 150°方向距离
 * 值范围: 0~量程上限(约400cm)，0或>threshold表示可通行
 */
static uint16_t g_map[SCT_NUM];

/**
 * @brief  获取距离地图指针
 * @return uint16_t* 指向g_map数组首地址，共SCT_NUM(9)个元素
 * @note   供外部模块(如UI调试显示)读取各扇区距离数据
 *         获取后可遍历g_map[0]~g_map[8]读取每个方向的距离
 */
uint16_t *Spatial_GetMap(void)
{
    return g_map;
}

/**
 * @brief  执行一次空间扫描
 * @param  threshold 障碍判定距离阈值(单位cm)
 *                   距离大于此值的扇区视为可通行
 *                   距离等于0(无回波)也表示无遮挡
 * @return PATH_FORWARD(0) - 正前方通畅，可直行
 *         PATH_LEFT(1)    - 左侧更优，推荐左转
 *         PATH_RIGHT(2)   - 右侧更优，推荐右转
 *         PATH_ESCAPE(3)  - 全部扇区均不可通行，需脱困
 *
 * @note   算法流程:
 *   1. 舵机逐角度旋转至各扇区，延时150ms等待稳定后测距
 *   2. 测距结果存入g_map[]数组
 *   3. 扫描完成后舵机自动回中(90°)
 *   4. 使用最长连续可通行扇区算法分析距离地图:
 *      - 可通行定义: distance > threshold(远于阈值) 或 distance < 1(无回波)
 *      - 遍历g_map找出最长的连续可通行扇区段
 *   5. 决策逻辑:
 *      - 若最长段覆盖正前方(索引4) → 直行 PATH_FORWARD
 *      - 若最长段中心偏左(索引<4) → 左转 PATH_LEFT
 *      - 若最长段中心偏右(索引>4) → 右转 PATH_RIGHT
 *      - 无可通行扇区 → 脱困 PATH_ESCAPE
 *
 * @warning 每次调用耗时约 9 × 150ms = 1350ms(含舵机动作时间)
 *          阻塞式调用，期间不响应其他任务
 *          调用前确保舵机已初始化(Servo_Init)
 */
uint8_t Spatial_Scan(uint16_t threshold)
{
    uint8_t i;                  /* 循环计数器 */
    uint8_t best_start = 0;     /* 最长可通行段的起始扇区索引 */
    uint8_t best_len = 0;       /* 最长可通行段的长度(连续扇区数) */
    uint8_t cur_start = 0;      /* 当前可通行段的起始扇区索引 */
    uint8_t cur_len = 0;        /* 当前可通行段的累计长度 */
    uint8_t center;             /* 最长可通行段的中心扇区索引 */

    /* 第一步：逐扇区扫描采集距离数据 */
    for (i = 0; i < SCT_NUM; i++)
    {
        Servo_SetAngle(g_angles[i]);   /* 舵机转到当前扇区角度 */
        delay_ms(150);                  /* 等待舵机到位(约50ms) + 超声波稳定(约100ms) */
        g_map[i] = (uint16_t)SR04_GetDistance();  /* 采集距离 */
    }
    Servo_SetAngle(90);                  /* 舵机回中到正前方 */

    /* 第二步：最长连续可通行扇区算法 */
    /*
     * 遍历距离地图，寻找最长连续可通行扇区段
     * 可通行条件: 距离 > threshold(障碍阈值) 或 距离 < 1(无回波/超出量程)
     * 连续段定义: 索引相邻且均可通行的扇区集合
     */
    for (i = 0; i < SCT_NUM; i++)
    {
        if (g_map[i] > threshold || g_map[i] < 1)
        {
            /* 当前扇区可通行 */
            if (cur_len == 0)
            {
                cur_start = i;           /* 记录新可通行段的起始位置 */
            }
            cur_len++;                   /* 可通行段长度+1 */
            if (cur_len > best_len)
            {
                /* 更新最长记录 */
                best_len = cur_len;
                best_start = cur_start;
            }
        }
        else
        {
            cur_len = 0;                 /* 遇到不可通行扇区，重置当前段 */
        }
    }

    /* 第三步：无可通行扇区 → 脱困 */
    if (best_len == 0)
    {
        return PATH_ESCAPE;
    }

    /* 第四步：计算最优路径的中心扇区 */
    center = best_start + best_len / 2;

    /*
     * 第五步：路径决策
     * 判定标准:
     *   - 若可通行段覆盖了正前方扇区(SCT_CENTER=4)
     *     则说明正前方有足够宽的通道 → 直行
     *   - 若中心偏左(center < SCT_CENTER)
     *     说明左侧空间更大 → 左转
     *   - 若中心偏右(center > SCT_CENTER)
     *     说明右侧空间更大 → 右转
     */
    if (best_start <= SCT_CENTER && (best_start + best_len - 1) >= SCT_CENTER)
    {
        return PATH_FORWARD;             /* 正前方在可通行范围内 */
    }

    if (center < SCT_CENTER)
    {
        return PATH_LEFT;                /* 最佳通道偏左 */
    }
    else
    {
        return PATH_RIGHT;               /* 最佳通道偏右 */
    }
}
