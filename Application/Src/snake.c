/********************************************************
 * 文件: snake.c
 * 描述: 贪吃蛇游戏 (OLED 128x64, 16列x8行网格)
 *       蓝牙方向键控制, 方向存队列, tick时读取
 *       蓝牙延迟处理: 方向缓存, tick同步, 禁止掉头
 ********************************************************/
#include "snake.h"
#include "oled.h"
#include <stdio.h>

/* 宏定义 ------------------------------------------------------------------*/
#define GW          16      // 网格宽度 (列数)
#define GH          8       // 网格高度 (行数)
#define MAX_SNAKE   128     // 最大蛇长 (超过即游戏结束)

/* 全局静态变量 ------------------------------------------------------------*/
static u8 sx[MAX_SNAKE];    // 蛇身各段 X 坐标 (sx[0]=蛇头)
static u8 sy[MAX_SNAKE];    // 蛇身各段 Y 坐标
static u8 slen;             // 当前蛇身长度
static u8 sdir;             // 当前真实移动方向 (tick_fn 中从 ndir 同步)
static u8 ndir;             // 下一方向 (外部 SetDir 写入, tick_fn 消费)
static u8 fx;               // 食物 X 坐标
static u8 fy;               // 食物 Y 坐标
static u8 go;               // 游戏结束标志: 0=进行中, 1=撞墙/自撞/满长
static u8 act;              // 游戏激活标志: 0=未激活, 1=游戏中
static u8 tick;             // 帧计数器 (每帧+1, 每4帧触发一次 tick_fn)
static u16 score;           // 得分 (吃到一个食物+1)

/* 掉头禁止查表 ------------------------------------------------------------*/
/* rev[dir] = 当前方向的反方向; 第16列不查表以消除误检               */
static const u8 rev[] = { 1, 0, 3, 2 };  // UP->DOWN, DOWN->UP, LEFT->RIGHT, RIGHT->LEFT

/**
 * @brief  生成食物 (伪随机放置)
 * @note   计算空位总数 empty = GW*GH - slen; 若 empty=0 直接返回 (蛇已占满)
 *         使用伪随机公式 (tick * 7 + 13) % empty 确定目标空位序号
 *         遍历所有网格, 跳过蛇身占据的格子, 找到第 target 个空位即为食物位置
 *         此算法无需独立随机数种子, 每帧 tick 递增加入变化
 */
static void place_food(void)
{
    u8 empty = GW * GH - slen;          // 剩余空格数
    if (empty == 0) return;              // 蛇已占满, 无需食物
    u8 target = (u8)((tick * 7U + 13U) % empty);  // 伪随机选中一个空位
    u8 idx = 0;                          // 当前遍历到的空位序号
    for (u8 y = 0; y < GH; y++) {
        for (u8 x = 0; x < GW; x++) {
            u8 occ = 0;                  // 标记 (x,y) 是否被蛇身占据
            for (u8 i = 0; i < slen; i++)
                if (sx[i] == x && sy[i] == y) { occ = 1; break; }
            if (!occ) {                  // 空格
                if (idx == target) { fx = x; fy = y; return; }  // 命中目标
                idx++;                   // 空格计数+1
            }
        }
    }
}

/**
 * @brief  初始化蛇 (进入游戏/重置时调用)
 * @note   蛇长4, 初始方向向右, 蛇身水平居中排列 (向右延伸)
 *         sx[0]~sx[3] = 3,2,1,0; sy 均为 GH/2
 *         分数清零, go=0, tick=0, 生成第一个食物
 */
static void init(void)
{
    slen = 4;                            // 初始蛇长4
    sdir = ndir = SNAKE_RIGHT;           // 初始方向向右
    for (u8 i = 0; i < slen; i++) {
        sx[i] = slen - 1 - i;            // 蛇头 x=3, 蛇尾 x=0
        sy[i] = GH / 2;                  // 所有段位于中间行 (第4行)
    }
    score = 0; go = 0; tick = 0;
    place_food();                        // 生成第一个食物
}

/**
 * @brief  进入贪吃蛇游戏 (外部调用接口)
 * @note   设置 act=1 激活游戏, 调用 init() 完成初始化
 */
void Snake_Enter(void) { act = 1; init(); }

/**
 * @brief  退出贪吃蛇游戏 (外部调用接口)
 * @note   设置 act=0 停用游戏, 清屏并刷新 OLED
 */
void Snake_Exit(void)  { act = 0; OLED_Clear(); OLED_Refresh(); }

/**
 * @brief  查询游戏是否激活
 * @return u8 : 1=游戏中, 0=未激活
 */
u8  Snake_IsActive(void) { return act; }

/**
 * @brief  获取当前分数
 * @return uint16_t : 当前得分
 */
uint16_t Snake_GetScore(void) { return score; }

/**
 * @brief  清零分数
 */
void Snake_ClearScore(void) { score = 0; }

/**
 * @brief  设置蛇的下一步方向 (蓝牙/按键回调调用)
 * @param  dir : 方向值 (SNAKE_UP/DOWN/LEFT/RIGHT, 取值范围 0~3)
 * @note   dir>3 直接返回 (非法方向)
 *         当蛇长>1 时, 若 dir 为当前方向的相反方向 (掉头), 忽略请求并保留原 ndir
 *         仅写入 ndir, 在 tick_fn() 中才同步到 sdir
 *         此机制实现"方向队列"以消除蓝牙高频串口中断的异步冲突
 */
void Snake_SetDir(u8 dir)
{
    if (dir > 3) return;                 // 非法方向, 直接忽略
    if (slen > 1 && rev[dir] == sdir) return;  // 禁止掉头
    ndir = dir;                          // 缓存为下一方向
}

/**
 * @brief  tick 处理函数 (每4帧执行一次)
 * @note   游戏逻辑核心:
 *         1. 若 go=1 (已结束) 直接返回
 *         2. 将 ndir 同步到 sdir (消费方向缓存)
 *         3. 计算新蛇头坐标 (含边界回绕)
 *         4. 检测是否吃到食物 (ate 标志)
 *         5. 自撞检测: 遍历蛇身 (吃到食物时遍历全长, 否则排除尾部)
 *         6. 移动蛇身: 各段后移一位, 新坐标赋给蛇头
 *         7. 若吃到食物: 蛇长+1, 分数+1, 检查 MAX_SNAKE, 生成新食物
 */
static void tick_fn(void)
{
    if (go) return;                       // 游戏已结束, 不再更新
    sdir = ndir;                          // 消费方向缓存

    u8 hx = sx[0], hy = sy[0];           // 备份旧蛇头坐标
    /* 根据方向计算新蛇头, 边界溢出则回绕到对侧 (环形边界) ------*/
    switch (sdir) {
    case SNAKE_UP:    hy = (hy == 0) ? GH - 1 : hy - 1; break;   // 上: y-1, 到顶回到底
    case SNAKE_DOWN:  hy = (hy >= GH - 1) ? 0 : hy + 1; break;   // 下: y+1, 到底回到顶
    case SNAKE_LEFT:  hx = (hx == 0) ? GW - 1 : hx - 1; break;   // 左: x-1, 到左回到右
    case SNAKE_RIGHT: hx = (hx >= GW - 1) ? 0 : hx + 1; break;   // 右: x+1, 到右回到左
    }

    u8 ate = (hx == fx && hy == fy);     // 检测新蛇头是否踩到食物

    /* 自撞检测: 如果吃到食物则查全长, 否则排除尾部 (尾部即将移走) --*/
    u8 clen = ate ? slen : slen - 1;
    for (u8 i = 0; i < clen; i++)
        if (sx[i] == hx && sy[i] == hy) { go = 1; return; }      // 撞到自己, 游戏结束

    /* 蛇身移动: 从尾部向头部逐个后移 ----------------------------------*/
    for (u8 i = slen; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
    sx[0] = hx; sy[0] = hy;              // 新蛇头写入

    /* 吃到食物处理 ----------------------------------------------------*/
    if (ate) {
        slen++; score++;                  // 长度+1, 分数+1
        if (slen >= MAX_SNAKE) { go = 1; return; }  // 达到最大长度, 胜利结束
        place_food();                     // 生成新食物
    }
}

/**
 * @brief  绘制画面 (每帧调用)
 * @note   直接操作 OLED 帧缓冲 (1024字节, 128x64像素)
 *         绘制策略:
 *         1. 清空帧缓冲
 *         2. 绘制食物: 闪烁方块 (tick&1 切换两种填充图案)
 *         3. 绘制蛇身: 每个格子填充 0xFF (全亮)
 *         4. 蛇眼抠点: 蛇头抠除两个点模拟眼睛
 *         5. 显示分数: 左上角 "S:score"
 *         6. 游戏结束: 居中显示 "GAME OVER! Score:xx" 和 "[EXIT] quit"
 *         7. 最后调用 OLED_Refresh() 刷到硬件
 */
static void draw(void)
{
    uint8_t *fb = OLED_GetFrameBuffer();  // 获取帧缓冲指针
    for (u16 i = 0; i < 1024; i++) fb[i] = 0;  // 清空缓冲 (全灭)

    /* 绘制食物 (闪烁效果) ----------------------------------------------*/
    if (!go) {
        u8 p = (tick & 1) ? 0x18 : 0x3C;  // 交替两种方块图案, 产生闪烁
        for (u8 d = 0; d < 8; d++)        // 食物占1列8像素
            fb[fy * 128 + fx * 8 + d] = p;
    }

    /* 绘制蛇身 (每个格子8像素列全亮) ----------------------------------*/
    for (u8 i = 0; i < slen; i++) {
        u8 *cell = &fb[sy[i] * 128 + sx[i] * 8];  // 定位到该段对应的帧缓冲位置
        for (u8 d = 0; d < 8; d++) cell[d] = 0xFF; // 整列点亮
    }

    /* 蛇眼抠点 (蛇头去两个像素模拟眼睛) --------------------------------*/
    if (!go && slen > 0) {
        u8 *head = &fb[sy[0] * 128 + sx[0] * 8];   // 蛇头在帧缓冲中的位置
        head[2] &= ~(1 << 2);                       // 第2列第2位 抠掉
        head[5] &= ~(1 << 2);                       // 第5列第2位 抠掉
    }

    /* 左上角显示当前分数 ----------------------------------------------*/
    if (!go) {
        char buf[12];
        sprintf(buf, "S:%u", (unsigned int)score);
        for (u8 d = 0; d < 24; d++) fb[d] = 0;     // 清除分数显示区域
        OLED_ShowString(0, 0, (const u8 *)buf, 1);  // 显示分数
    }

    /* 游戏结束画面 ----------------------------------------------------*/
    if (go) {
        char buf[20];
        sprintf(buf, "GAME OVER! Score:%u", (unsigned int)score);
        u8 x = (128 - 18 * 6) / 2;                  // 居中计算 (18字符, 每个6像素)
        OLED_ShowString(x, 20, (const u8 *)buf, 1); // 显示 GAME OVER
        OLED_ShowString(x, 36, (const u8 *)"[EXIT] quit", 1); // 提示退出
    }

    OLED_Refresh();                                  // 刷新显示 (将帧缓冲写入OLED)
}

/**
 * @brief  贪吃蛇主任务 (每帧由主循环调用一次)
 * @note   仅当 act=1 时执行:
 *         1. tick 递增 (帧计数器)
 *         2. 每4帧 (tick&3==0) 触发一次 tick_fn() 更新游戏逻辑
 *         3. 调用 draw() 刷新画面
 *         此设计确保蛇的移动速度约为屏幕刷新率的 1/4
 */
void Snake_Task(void)
{
    if (!act) return;                      // 游戏未激活, 直接退出
    tick++;                                // 帧计数+1
    if ((tick & 3) == 0) tick_fn();        // 每4帧执行一次逻辑更新
    draw();                                // 绘制画面
}
