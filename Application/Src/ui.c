/******************************************************************************
 * 文件名: ui.c
 * 描  述: 用户界面模块实现文件
 * 说  明: 封装人脸表情、温湿度显示、开机画面等UI功能
 *         内部使用Face_*函数族绘制，对外暴露UI_*接口
 ******************************************************************************/
#include "ui.h"
#include "oled.h"
#include "creeper.h"
#include "delay.h"
#include <stdio.h>
#include <string.h>
#include "oledfont.h"
#include "menu.h"
#include "ui_res.h"
#include "ui_wifi.h"

/*========== 面部几何参数 ==========*/
#define EX1 40         /* 左眼中心X坐标（OLED 128x64 水平方向） */
#define EX2 88         /* 右眼中心X坐标 */
#define EY  26         /* 双眼中心Y坐标 */
#define MX  64         /* 嘴巴中心X坐标（屏幕水平中央） */
#define MY  44         /* 嘴巴中心Y坐标 */
#define ES  4          /* 眼睛半尺寸（单眼占据 2*ES+1 = 9x9 像素区域） */

/*========== 全局状态变量 ==========*/
static u8 face_expr = 0;       /* 当前表情模式：FACE_NORMAL / FACE_ALERT / FACE_SAD */
static u8 blink_timer = 0;     /* 眨眼计时器：每刷一帧+1，>=6时触发眨眼 */
static u8 is_blinking = 0;     /* 眨眼标志：1表示当前帧执行闭眼绘制 */
static char face_msg[22] = {0};/* 底部消息缓冲区（最长21字符 + '\0'） */
static u8 msg_timer = 0;       /* 消息显示倒计时：Face_Task中每帧减1，0则隐藏 */

/******************************************************************************
 * @brief   设置底部状态消息（持续显示约25个刷新周期后自动消失）
 * @param   s 消息字符串（最长21字符，超过则截断）
 * @note    复制字符串到 face_msg 缓冲区，同时复位 msg_timer = 25
 *          在 Face_Task → DrawFace 中每帧检查 msg_timer 决定是否显示
 ******************************************************************************/
void Face_ShowMsg(const char *s)
{
    u8 i = 0;
    while (*s && i < 21) face_msg[i++] = *s++;  /* 逐字符复制，限制21字符 */
    face_msg[i] = 0;                             /* 确保字符串结束 */
    msg_timer = 25;                              /* 倒计时25帧（约1秒@FPS不定） */
}


/* 最近一次温湿度采样值，由 OLED_UpdateTH() 更新，DrawFace() 读取显示 */
static u8 th_ti=0, th_td=0, th_hi=0, th_hd=0;

/******************************************************************************
 * @brief   更新温湿度显示数据（由DHT11/温湿度传感器读取后调用）
 * @param   ti 温度整数部分
 * @param   td 温度小数部分（显示为 如 25.3C）
 * @param   hi 湿度整数部分
 * @param   hd 湿度小数部分（当前仅保留，未使用）
 * @note    仅更新全局变量，实际重绘在下次 Face_Task→DrawFace 时发生
 ******************************************************************************/
void OLED_UpdateTH(u8 ti, u8 td, u8 hi, u8 hd) { th_ti=ti; th_td=td; th_hi=hi; th_hd=hd; (void)th_hd; }

/******************************************************************************
 * @brief   设置脸部表情模式
 * @param   expr 表情模式：FACE_NORMAL / FACE_ALERT / FACE_SAD
 * @note    写入全局 face_expr，下次 DrawFace() 绘制时生效
 ******************************************************************************/
void Face_Set(u8 expr) { face_expr = expr; }


/******************************************************************************
 * @brief   显示16x16中文字符
 * @param   x    起始X坐标（像素）
 * @param   y    起始Y坐标（像素）
 * @param   num  字库索引（0=温, 1=度, 2=湿, 11=温, 12=度, 14=湿）
 * @param   size1 字体大小：16/24/32/64
 * @param   mode 显示模式：1=点亮前景，0=反转（背景色）
 * @note    支持4种字体尺寸，逐字节解析点阵，逐列逐位绘制
 *          字节低位对应像素下方，按列右移扫描
 *          当列数达到size1时换行到下一8像素行
 ******************************************************************************/
void OLED_ShowChinese(u8 x, u8 y, u8 num, u8 size1, u8 mode)
{
    u8 m, temp;
    u8 x0 = x, y0 = y;                            /* 保存起始坐标用于换行计算 */
    u16 i, size3 = (size1 / 8 + ((size1 % 8) ? 1 : 0)) * size1; /* 字模总字节数 */
    for (i = 0; i < size3; i++) {
        /* 根据字体大小选择对应字库 */
        if (size1 == 16)      temp = Hzk1[num][i];
        else if (size1 == 24) temp = Hzk2[num][i];
        else if (size1 == 32) temp = Hzk3[num][i];
        else if (size1 == 64) temp = Hzk4[num][i];
        else return;                              /* 不支持的字体尺寸，直接返回 */

        for (m = 0; m < 8; m++) {                 /* 逐位扫描（一个字节8个像素点） */
            if (temp & 0x01) OLED_DrawPoint(x, y, mode);   /* 位0=1则画点 */
            else OLED_DrawPoint(x, y, !mode);              /* 否则画反色 */
            temp >>= 1;                            /* 右移取下一位（低位在先） */
            y++;                                   /* 纵向扫描：Y递增 */
        }
        x++;                                       /* 横向切换到下一列 */
        if ((x - x0) == size1) { x = x0; y0 = y0 + 8; }  /* 满一列宽则换行 */
        y = y0;                                    /* 重置Y到当前行起始 */
    }
}

/******************************************************************************
 * @brief   绘制脸部表情（含眼睛、嘴巴、温湿度信息）
 * @param   bl 眨眼控制：0=睁眼（方块眼），1=闭眼（横线眼）
 * @note    绘制流程：
 *          1. 清空OLED缓冲区
 *          2. 绘制双眼：bl=1时画水平横线（闭眼），bl=0时画9x9方块（睁眼）
 *          3. 绘制嘴巴：
 *             - FACE_ALERT：画6x5方形嘴（边框描点）
 *             - FACE_NORMAL：dy = -2 - x²/16，抛物线开口朝上（微笑）
 *             - FACE_SAD：   dy = 2 + x²/16，抛物线开口朝下（难过）
 *             - 其它：水平直线
 *          4. 显示底部消息（msg_timer > 0时）
 *          5. 显示温湿度信息（使用中文字库索引11=温/12=度/14=湿）
 ******************************************************************************/
static void DrawFace(u8 bl)
{
    u8 x, y;
    int16_t dx, dy;
    OLED_Clear();                                  /* 清空缓冲区，重新绘制 */

    /*========== 绘制眼睛 ==========*/
    if (bl) {
        /* 闭眼：在眼睛位置画水平横线（左眼和右眼各一条） */
        for (x = EX1 - ES; x <= EX1 + ES; x++) OLED_DrawPoint(x, EY, 1);
        for (x = EX2 - ES; x <= EX2 + ES; x++) OLED_DrawPoint(x, EY, 1);
    } else {
        /* 睁眼：在眼睛位置画9x9实心方块 */
        for (y = EY - ES; y <= EY + ES; y++)
            for (x = EX1 - ES; x <= EX1 + ES; x++) OLED_DrawPoint(x, y, 1);
        for (y = EY - ES; y <= EY + ES; y++)
            for (x = EX2 - ES; x <= EX2 + ES; x++) OLED_DrawPoint(x, y, 1);
    }

    /*========== 绘制嘴巴 ==========*/
    if (face_expr == FACE_ALERT) {
        /* 警戒表情：方形嘴（6x5边框描点，中心镂空） */
        for (dx = -6; dx <= 6; dx++)
            for (dy = -2; dy <= 2; dy++)
                if (dx == -6 || dx == 6 || dy == -2 || dy == 2)
                    OLED_DrawPoint(MX + dx, MY + dy, 1);
    } else {
        /* 抛物线嘴巴（-10 ~ +10 水平范围） */
        for (dx = -10; dx <= 10; dx++) {
            if (face_expr == FACE_NORMAL)
                dy = -2 - (dx * dx) / 16;          /* 笑脸：y=-2-x²/16，向上开口 */
            else if (face_expr == FACE_SAD)
                dy = 2 + (dx * dx) / 16;           /* 哭脸：y=2+x²/16，向下开口 */
            else
                dy = 0;                            /* 其他：水平直线 */
            OLED_DrawPoint(MX + dx, MY + dy, 1);
        }
    }

    /*========== 显示底部消息 ==========*/
    if (msg_timer) {
        OLED_ShowString(2, 54, (const u8 *)face_msg, 1);  /* 在Y=54行显示消息 */
        msg_timer--;                                         /* 倒计时递减 */
    }

    /*========== 显示温湿度信息 ==========*/
    {
        char b[16];
        /* 显示"温度"（字库索引11=温, 12=度） */
        OLED_ShowChinese(0, 48, 11, 16, 1);
        OLED_ShowChinese(16, 48, 12, 16, 1);
        sprintf(b, ":%d.%dC", th_ti, th_td);      /* 格式：温度值 + C */
        OLED_ShowString(34, 54, (const u8 *)b, 1);

        /* 显示"湿度"（字库索引14=湿, 12=度） */
        OLED_ShowChinese(68, 48, 14, 16, 1);
        OLED_ShowChinese(84, 48, 12, 16, 1);
        sprintf(b, ":%d%%", th_hi);                /* 格式：湿度值 + % */
        OLED_ShowString(102, 54, (const u8 *)b, 1);
    }
    /* 注：th_td（温度小数）和th_hd（湿度小数）在温湿度行中被使用 */
    /* OLED_ShowChinese索引说明：11=温 12=度 14=湿（见 oledfont.h 字库定义） */
}

/******************************************************************************
 * @brief   脸部刷新任务（更新表情 + 自动眨眼）
 * @note    眨眼机制：
 *          每调用一次 blink_timer++，当 >=6 时置位 is_blinking。
 *          下一帧检测到 is_blinking=1 时，先绘制闭眼帧并刷新OLED，
 *          延时100ms后恢复睁眼，复位标志和计时器。
 *          这样实现约每6帧眨眼一次的效果。
 *          最后调用 DrawFace(0) 绘制睁眼状态并刷新屏幕。
 ******************************************************************************/
void Face_Task(void)
{
    if (is_blinking) {
        /* 闭眼帧：绘制横线眼 → 刷新屏幕 → 保持100ms模拟眨眼瞬间 */
        DrawFace(1); OLED_Refresh();
        delay_ms(100);
        is_blinking = 0; blink_timer = 0;          /* 复位眨眼状态 */
    }
    DrawFace(0); OLED_Refresh();                   /* 睁眼帧：正常显示 */
    blink_timer++;                                  /* 帧计数累加 */
    if (blink_timer >= 6) is_blinking = 1;          /* 每6帧触发一次眨眼 */
}

/******************************************************************************
 * @brief   全屏显示温湿度（调试用，当前未使用）
 * @param   ti 温度整数部分
 * @param   td 温度小数部分
 * @param   hi 湿度整数部分
 * @param   hd 湿度小数部分
 * @note    单独全屏显示，不叠加在表情画面上
 *          与 DrawFace 中的温湿度行不同，此为独立调试界面
 ******************************************************************************/
void OLED_ShowTH(u8 ti, u8 td, u8 hi, u8 hd)
{
    char b[16];
    OLED_Clear();
    OLED_ShowString(4, 10, (const u8 *)"Temp:", 1);  /* 显示"Temp:"标签 */
    sprintf(b, "%d.%dC", ti, td);                    /* 格式化温度字符串 */
    OLED_ShowString(4, 22, (const u8 *)b, 1);        /* 显示温度值 */
    OLED_ShowString(4, 38, (const u8 *)"Humi:", 1);  /* 显示"Humi:"标签 */
    sprintf(b, "%d.%d%%", hi, hd);                   /* 格式化湿度字符串 */
    OLED_ShowString(4, 50, (const u8 *)b, 1);        /* 显示湿度值 */
    OLED_Refresh();
}


/******************************************************************************
 * @brief   界面初始化
 * @note    流程：
 *          1. OLED_Init() — 初始化OLED显示屏（SSD1306驱动）
 *          2. OLED_ShowImage() — 显示Creeper 64x64位图（我的世界苦力怕面孔）
 *          3. OLED_Refresh() — 刷新显示
 *          4. delay_ms(5000) — 开机画面保持5秒
 *          5. Face_Set(FACE_NORMAL) — 恢复默认笑脸
 ******************************************************************************/
void UI_Init(void)
{
    OLED_Init();
    OLED_ShowImage(creeper_64x64, 32, 0, 64, 64);    /* Creeper画面居中偏移32 */
    OLED_Refresh();
    delay_ms(5000);                                    /* 开机画面停留5秒 */
    Face_Set(FACE_NORMAL);                             /* 切回正常表情 */
}

/******************************************************************************
 * @brief   界面定时刷新（主循环中周期调用）
 * @note    若菜单（Menu）处于激活状态，则调用 Menu_Task() 处理菜单刷新
 *         并检查空闲超时自动退出；否则调用 Face_Task() 刷新表情/眨眼/温湿度
 *         这样确保菜单激活时不会叠加显示脸部画面
 ******************************************************************************/
void UI_Task(void)
{
    if (Menu_IsActive())
    {
        Menu_Task();  /* 菜单活跃时处理菜单任务（含空闲超时检测） */
        return;
    }
    Face_Task();      /* 无菜单活动时刷新脸部表情 */
}

/******************************************************************************
 * @brief   设置表情模式（对外接口）
 * @param   mode 表情模式：FACE_NORMAL / FACE_ALERT / FACE_SAD
 * @note    委托给 Face_Set() 写入 face_expr，间接影响 DrawFace() 的嘴巴绘制
 ******************************************************************************/
void UI_SetMode(u8 mode)
{
    Face_Set(mode);
}

/******************************************************************************
 * @brief   更新温湿度显示（对外接口）
 * @param   ti 温度整数部分
 * @param   td 温度小数部分
 * @param   hi 湿度整数部分
 * @param   hd 湿度小数部分
 * @note    委托给 OLED_UpdateTH() 更新全局变量，下次 Face_Task 绘制时生效
 ******************************************************************************/
void UI_UpdateTH(u8 ti, u8 td, u8 hi, u8 hd) { OLED_UpdateTH(ti, td, hi, hd); }

/******************************************************************************
 * @brief   全屏显示UI2测试位图（5秒后自动退出）
 * @note    使用 ui2_bmp 位图全屏覆盖，5秒后返回
 *         OLED_ShowPicture(0, 0, 128, 64, ...) 从(0,0)到(127,63)整屏绘制
 ******************************************************************************/
void UI_ShowTest(void)
{
    OLED_Clear();
    OLED_ShowPicture(0, 0, 128, 64, ui2_bmp, 1);     /* 全屏显示测试位图 */
    OLED_Refresh();
    delay_ms(5000);                                    /* 保持5秒 */
}

/******************************************************************************
 * @brief   WiFi信号强度动画循环播放
 * @param   cycles 动画重复次数（每次完整播放4帧）
 * @param   delay  帧间延时(ms)，建议300~500ms
 * @note    依次显示 wifi1_bmp ~ wifi4_bmp 四帧位图，形成信号格逐级增长效果
 *          动画执行期间阻塞主循环（delay_ms 阻塞等待）
 *          结束后由 Face_Task 在下次 UI_Task 调用时恢复脸部绘制
 *          帧位图来源于 ui_wifi.h（128x64全屏位图）
 *          典型调用：UI_ShowWiFi(3, 400) 播放3轮，每帧400ms
 ******************************************************************************/
void UI_ShowWiFi(u8 cycles, u16 delay)
{
    u8 idx;        /* 外层循环计数器：重复轮次 */
    u8 frame;      /* 帧索引：0~3，对应wifi1~wifi4 */
    static const unsigned char *frames[4] = {
        wifi1_bmp, wifi2_bmp, wifi3_bmp, wifi4_bmp
    };

    for (idx = 0; idx < cycles; idx++) {
        for (frame = 0; frame < 4; frame++) {
            OLED_Clear();
            OLED_ShowPicture(0, 0, 128, 64, frames[frame], 1);  /* 全屏绘制当前帧 */
            OLED_Refresh();
            delay_ms(delay);                                     /* 帧间延时 */
        }
    }
}


/******************************************************************************
 * @brief   显示临时状态消息（对外接口）
 * @param   str 消息字符串
 * @note    委托给 Face_ShowMsg()，将字符串复制到 face_msg 缓冲区
 *          并设置 msg_timer=25（约持续1秒），由 DrawFace 自动绘制
 ******************************************************************************/
void UI_ShowMsg(const char *str)
{
    Face_ShowMsg(str);
}
