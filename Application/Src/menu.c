/********************************************************
 * 文件名: menu.c
 * 描述:   菜单系统 - 提供OLED菜单界面，支持光标选择、参数调节、
 *         空闲自动退出、滚屏显示等功能
 *
 * 菜单项:
 *   项0: 工作模式  REMOTE(遥控) / AUTO(自动避障) / FOLLOW(跟随) 循环切换
 *   项1: 左轮微调  (-300 ~ +300)，用于修正直行偏航
 *   项2: 右轮微调  (-300 ~ +300)
 *   项3: 基准速度  (0 ~ 1000)
 *   项4: 贪吃蛇    (OK键进入游戏)
 *   项5: WiFi配置  (OK键进入配网)
 ********************************************************/
#include "menu.h"
#include "oled.h"
#include "delay.h"
#include "motor.h"
#include "usart.h"
#include "snake.h"
#include "wificfg.h"

/* 外部引用：电机基准速度，由其他模块维护 */
extern volatile u16 speed;

/* ========== 菜单全局状态变量 ========== */
static u8 menu_active = 0;    /* 菜单激活标志：0=关闭，1=激活中 */
static u8 cursor = 0;         /* 当前光标选中的菜单项索引(0~MENU_ITEMS-1) */
static u16 idle_timer = 0;    /* 空闲计时器，每调用Menu_Task递增1，>=750时自动退出菜单 */
static u8 blink_toggle = 1;   /* 光标闪烁标志：1=显示">"指示符，0=显示空格(隐藏光标) */
static u8 blink_cnt = 0;      /* 闪烁计数值，>=8时翻转blink_toggle并重绘 */

/* ========== OLED布局常量(像素坐标) ========== */
#define ITEM_H      16    /* 每个菜单项的高度(像素)，与16点阵字库高度一致 */
#define CURSOR_X    0     /* 光标指示符">"的X坐标(最左侧) */
#define CH_X        8     /* 菜单项中文图标起始X坐标 */
#define DESC_X      44    /* 菜单项描述文字起始X坐标 */
#define LIST_TOP    0     /* 菜单列表起始Y坐标(屏幕顶部) */
#define VISIBLE     4     /* 屏幕每页同时显示的菜单项数(屏幕总高度64/ITEM_H=4) */

/**
 * @brief  绘制单个菜单项到OLED指定位置
 * @param  idx   菜单项索引(0~5)，对应6个菜单功能
 * @param  y     该项在OLED上的垂直起始Y坐标(像素)
 * @param  sel   选中标志：1=当前光标所在项，0=非选中项
 * @retval 无
 * @note   选中项在左侧显示闪烁光标">"，根据blink_toggle状态切换显示或隐藏。
 *         不同菜单项绘制不同的图标(中文或英文)和描述文字。
 *         中文字库索引值通过实测字库位置确定，各值含义见具体case。
 */
static void DrawItem(u8 idx, u8 y, u8 sel)
{
    char buf[21];

    /* 选中项：在左侧CURSOR_X位置显示闪烁光标">" */
    if (sel) {
        if (blink_toggle)
            OLED_ShowString(CURSOR_X, y + 4, (const u8 *)">", 1);
        else
            OLED_ShowString(CURSOR_X, y + 4, (const u8 *)" ", 1);
    }

    switch (idx) {
    case 0:
        /* 工作模式：显示中文图标(遥/控 或 自/动) + 模式名称 */
        if (work_mode == MODE_REMOTE) {
            /* 遥控模式：显示"遥"(索引15) + "控"(索引16) */
            OLED_ShowChinese(CH_X, y, 15, 16, 1);
            OLED_ShowChinese(CH_X + 16, y, 16, 16, 1);
            OLED_ShowString(DESC_X, y + 4, (const u8 *)"Mode:REMOTE", 1);
        } else if (work_mode == MODE_AUTO) {
            /* 自动避障模式：显示"自"(索引17) + "动"(索引18) */
            OLED_ShowChinese(CH_X, y, 17, 16, 1);
            OLED_ShowChinese(CH_X + 16, y, 18, 16, 1);
            OLED_ShowString(DESC_X, y + 4, (const u8 *)"Mode:AUTO  ", 1);
        } else {
            /* 跟随模式：无对应中文，使用英文"Follow" */
            OLED_ShowString(CH_X, y + 4, (const u8 *)"Follow", 1);
            OLED_ShowString(DESC_X, y + 4, (const u8 *)"Mode:FOLLOW", 1);
        }
        break;

    case 1:
        /* 左轮微调：显示"左"(索引20) + "轮"(索引22) + 当前修正值 */
        OLED_ShowChinese(CH_X, y, 20, 16, 1);       /* "左"字库索引20 */
        OLED_ShowChinese(CH_X + 16, y, 22, 16, 1);  /* "轮"字库索引22 */
        sprintf(buf, "L:%+d", left_trim);
        OLED_ShowString(DESC_X, y + 4, (const u8 *)buf, 1);
        break;

    case 2:
        /* 右轮微调：显示"右"(索引21) + "轮"(索引22) + 当前修正值 */
        OLED_ShowChinese(CH_X, y, 21, 16, 1);       /* "右"字库索引21 */
        OLED_ShowChinese(CH_X + 16, y, 22, 16, 1);  /* "轮"字库索引22 */
        sprintf(buf, "R:%+d", right_trim);
        OLED_ShowString(DESC_X, y + 4, (const u8 *)buf, 1);
        break;

    case 3:
        /* 基准速度：显示"速"(索引19) + "度"(索引12) + 当前速度值 */
        OLED_ShowChinese(CH_X, y, 19, 16, 1);       /* "速"字库索引19 */
        OLED_ShowChinese(CH_X + 16, y, 12, 16, 1);  /* "度"字库索引12 */
        sprintf(buf, "Speed:%d", speed);
        OLED_ShowString(DESC_X, y + 4, (const u8 *)buf, 1);
        break;

    case 4:
        /* 贪吃蛇游戏：显示英文"Snake" + 进入提示 */
        OLED_ShowString(CH_X, y + 4, (const u8 *)"Snake", 1);
        OLED_ShowString(DESC_X, y + 4, (const u8 *)"> OK play", 1);
        break;

    case 5:
        /* WiFi配网：显示"Net" + 已配置的SSID名称 */
        OLED_ShowString(CH_X, y + 4, (const u8 *)"Net", 1);
        if (g_wifi_ssid[0])
            OLED_ShowString(DESC_X, y + 4, (const u8 *)g_wifi_ssid, 1);
        else
            OLED_ShowString(DESC_X, y + 4, (const u8 *)"[not set]", 1);
        break;
    }
}

/**
 * @brief  绘制完整菜单界面(清屏后重绘所有可见项)
 * @param  无
 * @retval 无
 * @note   支持滚动显示：根据cursor位置动态计算当前可见的菜单项范围，
 *         实现类似列表滚动的效果。屏幕右上角/右下角显示滚屏指示符
 *         "^"(上方有隐藏项)和"v"(下方有隐藏项)。
 *         绘制完成后通过串口同步输出菜单状态，供调试时查看。
 */
static void Menu_Draw(void)
{
    u8 i, start = 0;

    /* 计算可见范围起始索引：cursor在顶部时从0开始，在底部时从末尾-VISIBLE开始，
       否则让cursor居中，前后各显示一项 */
    if (cursor < 2) start = 0;
    else if (cursor >= MENU_ITEMS - 2) start = MENU_ITEMS - VISIBLE;
    else start = cursor - 1;

    OLED_Clear();
    /* 循环绘制当前页所有可见菜单项 */
    for (i = start; i < start + VISIBLE && i < MENU_ITEMS; i++)
        DrawItem(i, LIST_TOP + (i - start) * ITEM_H, i == cursor);

    /* 列表上方有隐藏项时，右上角显示"^"指示可上滚 */
    if (start > 0)
        OLED_ShowString(122, 0, (const u8 *)"^", 1);
    /* 列表下方有隐藏项时，右下角显示"v"指示可下滚 */
    if (start + VISIBLE < MENU_ITEMS)
        OLED_ShowString(122, 56, (const u8 *)"v", 1);

    OLED_Refresh();

    /* === 串口同步输出(调试用) === */
    {
        char buf[50];
        USART2_SendString("\r\n===== Menu =====\r\n");
        for (i = start; i < start + VISIBLE && i < MENU_ITEMS; i++) {
            if (i == cursor)
                USART2_SendString("> ");  /* 选中项前缀 */
            else
                USART2_SendString("  ");  /* 非选中项缩进 */
            switch (i) {
            case 0:
                USART2_SendString(work_mode == MODE_REMOTE ? "Mode: REMOTE" :
                                   work_mode == MODE_AUTO ? "Mode: AUTO" : "Mode: FOLLOW");
                break;
            case 1: sprintf(buf, "L-Trim: %+d", left_trim); USART2_SendString(buf); break;
            case 2: sprintf(buf, "R-Trim: %+d", right_trim); USART2_SendString(buf); break;
            case 3: sprintf(buf, "Speed: %d", speed);        USART2_SendString(buf); break;
            case 4: sprintf(buf, "WiFi: %s", g_wifi_ssid);   USART2_SendString(buf); break;
            case 5: USART2_SendString("Snake > OK play");    break;
            }
            USART2_SendString("\r\n");
        }
        USART2_SendString("================\r\n");
    }
    /*
     * 注意：串口输出中case4对应WiFi、case5对应Snake，与DrawItem中
     * case4=Snake、case5=WiFi的顺序恰好相反。此为历史实现，保留原状。
     */
}

/**
 * @brief  进入菜单模式
 * @param  无
 * @retval 无
 * @note   置位menu_active激活标志，光标复位到第0项(模式)，
 *         重置空闲计时器和光标闪烁状态，然后绘制初始菜单界面。
 */
void Menu_Enter(void)
{
    menu_active = 1; cursor = 0;
    idle_timer = 0; blink_toggle = 1;
    Menu_Draw();
}

/**
 * @brief  退出菜单模式
 * @param  无
 * @retval 无
 * @note   清除menu_active标志，重置空闲计时器，
 *         清空OLED屏幕并刷新。调用后系统回到主循环正常运行。
 */
void Menu_Exit(void)
{
    menu_active = 0; idle_timer = 0;
    OLED_Clear(); OLED_Refresh();
}

/**
 * @brief  查询菜单是否处于激活状态
 * @param  无
 * @retval 1 = 菜单激活中，0 = 菜单未激活
 */
u8 Menu_IsActive(void) { return menu_active; }

/**
 * @brief  光标向上移动(对应UP按键按下)
 * @param  无
 * @retval 无
 * @note   重置空闲计时器(延后自动退出触发)，cursor减1(不低于0)。
 *         重绘菜单界面反映光标位置变化。
 */
void Menu_Up(void)
{
    if (!menu_active) return;
    idle_timer = 0;          /* 检测到用户操作，重置空闲计时 */
    if (cursor > 0) cursor--;
    Menu_Draw();
}

/**
 * @brief  光标向下移动(对应DOWN按键按下)
 * @param  无
 * @retval 无
 * @note   重置空闲计时器，cursor加1(不超过MENU_ITEMS-1)。
 *         重绘菜单界面。
 */
void Menu_Down(void)
{
    if (!menu_active) return;
    idle_timer = 0;          /* 检测到用户操作，重置空闲计时 */
    if (cursor < MENU_ITEMS - 1) cursor++;
    Menu_Draw();
}

/**
 * @brief  减小当前选中项的值 或 切换模式(对应LEFT按键)
 * @param  无
 * @retval 无
 * @note   不同菜单项的行为不同：
 *         - 项0(模式):    循环切换到下一个模式(REMOTE→AUTO→FOLLOW→REMOTE)
 *         - 项1(左轮微调): left_trim减10(最低-300)
 *         - 项2(右轮微调): right_trim减10(最低-300)
 *         - 项3(基准速度): speed减100(最低0)
 *         其他项无操作。操作后重绘菜单更新显示。
 */
void Menu_Left(void)
{
    if (!menu_active) return;
    idle_timer = 0;          /* 检测到用户操作，重置空闲计时 */
    switch (cursor) {
    case 0: work_mode = (work_mode + 1) % 3; break;   /* 3种模式循环切换 */
    case 2: if (right_trim > -300) right_trim -= 10; break; /* 右轮微调递减，步长10 */
    case 1: if (left_trim  > -300) left_trim  -= 10; break; /* 左轮微调递减，步长10 */
    case 3: if (speed >= 100) speed -= 100; break;          /* 速度递减，步长100 */
    }
    Menu_Draw();
}

/**
 * @brief  增大当前选中项的值 或 切换模式(对应RIGHT按键)
 * @param  无
 * @retval 无
 * @note   不同菜单项的行为不同：
 *         - 项0(模式):    循环切换到下一个模式(同上)
 *         - 项1(左轮微调): left_trim加10(最高+300)
 *         - 项2(右轮微调): right_trim加10(最高+300)
 *         - 项3(基准速度): speed加100(最高1000)
 *         其他项无操作。操作后重绘菜单更新显示。
 */
void Menu_Right(void)
{
    if (!menu_active) return;
    idle_timer = 0;          /* 检测到用户操作，重置空闲计时 */
    switch (cursor) {
    case 0: work_mode = (work_mode + 1) % 3; break;   /* 3种模式循环切换 */
    case 2: if (right_trim < 300) right_trim += 10; break;  /* 右轮微调递增，步长10 */
    case 1: if (left_trim  < 300) left_trim  += 10; break;  /* 左轮微调递增，步长10 */
    case 3: if (speed < 1000) speed += 100; break;          /* 速度递增，步长100 */
    }
    Menu_Draw();
}

/**
 * @brief  确认/进入当前选中的菜单项(对应OK按键)
 * @param  无
 * @retval 无
 * @note   根据当前光标位置执行不同操作：
 *         - 项4(Snake): 退出菜单，进入贪吃蛇游戏
 *         - 项5(Net):   退出菜单，启动WiFi配网连接
 *         - 其他项:     直接退出菜单(确认当前设置)
 */
void Menu_Ok(void)
{
    if (!menu_active) return;
    if (cursor == 4) {
        /* 进入贪吃蛇游戏 */
        Snake_Enter();
        Menu_Exit();
        return;
    }
    if (cursor == 5) {
        /* 进入WiFi配网模式 */
        Menu_Exit();
        WifiCfg_Connect();
        return;
    }
    /* 其他菜单项：OK仅用于退出菜单 */
    Menu_Exit();
}

/**
 * @brief  菜单定时任务(需在主循环中周期性调用)
 * @param  无
 * @retval 无
 * @note   处理两个周期性任务：
 *         1. 光标闪烁：blink_cnt每调用递增1，>=8时翻转blink_toggle并重绘，
 *            实现光标周期性闪烁效果(约80ms周期，取决于调用间隔)
 *         2. 空闲超时退出：idle_timer递增，>=750时自动调用Menu_Exit()，
 *            防止菜单长期占用阻塞主逻辑。约750个调用周期后超时。
 */
void Menu_Task(void)
{
    if (!menu_active) return;
    blink_cnt++;                   /* 闪烁计数器递增 */
    if (blink_cnt >= 8) {
        blink_cnt = 0;             /* 计满归零 */
        blink_toggle = !blink_toggle;  /* 翻转闪烁状态(显示/隐藏光标) */
        Menu_Draw();               /* 重绘以更新光标显示 */
    }
    if (idle_timer < 750) idle_timer++;  /* 空闲计时递增 */
    else Menu_Exit();                    /* 空闲超时，自动退出菜单 */
}
