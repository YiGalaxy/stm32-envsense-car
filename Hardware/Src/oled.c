/******************************************************************************
 * 文件名: oled.c
 * 描  述: OLED显示驱动模块实现文件
 * 说  明: SSD1306 I2C驱动，128x64像素
 *         软件模拟I2C（bit-banging）方式驱动SSD1306 OLED显示屏
 *         支持点阵绘制、字符显示、位图显示、帧缓冲机制
 * 硬  件: GPIOG.12(SCL) + GPIOD.5(SDA) + GPIOD.4(RES)
 *         使用GPIO模拟I2C时序，非硬件I2C外设
 ******************************************************************************/
#include "oled.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

/*==============================================================================
 * I2C引脚配置
 * SCL  - 串行时钟线: GPIOG Pin 12
 * SDA  - 串行数据线: GPIOD Pin 5
 * RES  - OLED复位线: GPIOD Pin 4 (低电平复位)
 *============================================================================*/
#define OLED_SCL_PORT  GPIOG                          /* I2C时钟线端口 */
#define OLED_SCL_PIN   GPIO_Pin_12                    /* I2C时钟线引脚 */
#define OLED_SDA_PORT  GPIOD                          /* I2C数据线端口 */
#define OLED_SDA_PIN   GPIO_Pin_5                     /* I2C数据线引脚 */

/* I2C引脚电平控制宏：写SCL/SDA高低电平 */
#define SCL_H()   GPIO_SetBits(OLED_SCL_PORT, OLED_SCL_PIN)   /* SCL置高 */
#define SCL_L()   GPIO_ResetBits(OLED_SCL_PORT, OLED_SCL_PIN) /* SCL置低 */
#define SDA_H()   GPIO_SetBits(OLED_SDA_PORT, OLED_SDA_PIN)   /* SDA置高 */
#define SDA_L()   GPIO_ResetBits(OLED_SDA_PORT, OLED_SDA_PIN) /* SDA置低 */

#define OLED_ADDR  0x78                              /* SSD1306 I2C设备地址(7位地址0x3C左移1位) */
#define BS 5                                          /* I2C半周期延时基数(单位us)，决定SCL时钟频率 */

/* OLED复位引脚配置 */
#define RES_PORT GPIOD                                /* 复位引脚端口 */
#define RES_PIN  GPIO_Pin_4                           /* 复位引脚号 */
#define RES_H() GPIO_SetBits(RES_PORT, RES_PIN)       /* 复位引脚置高(正常工作) */
#define RES_L() GPIO_ResetBits(RES_PORT, RES_PIN)     /* 复位引脚置低(复位状态) */

/*==============================================================================
 * 软件I2C底层驱动函数（静态内部）
 * 模拟I2C主模式时序，支持标准模式(100kHz)或快速模式(400kHz)
 *============================================================================*/

/**
 * @brief  I2C半周期延时
 * @note   控制SCL时钟频率，BS=5时约100kHz
 *         增大BS降低频率提高兼容性，减小BS提高刷新率
 */
static void I2C_Delay(void) { delay_us(BS); }

/**
 * @brief  I2C起始条件
 * @note   SCL高电平时SDA从高切换到低，标志传输开始
 */
static void I2C_Start(void) { SDA_H(); SCL_H(); I2C_Delay(); SDA_L(); I2C_Delay(); SCL_L(); }

/**
 * @brief  I2C停止条件
 * @note   SCL高电平时SDA从低切换到高，标志传输结束
 */
static void I2C_Stop(void)  { SDA_L(); SCL_H(); I2C_Delay(); SDA_H(); I2C_Delay(); }

/**
 * @brief  I2C发送一个字节
 * @param  dat 要发送的8位数据
 * @note   MSB先行，每发送完一个字节后释放SDA并接收ACK
 *         ACK检测通过读取SDA电平实现(此处仅产生时钟脉冲，不严格检验ACK)
 */
static void I2C_WriteByte(uint8_t dat)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (dat & 0x80) SDA_H(); else SDA_L();        /* 输出当前位(MSB先) */
        I2C_Delay(); SCL_H(); I2C_Delay(); SCL_L();   /* SCL产生一个时钟脉冲 */
        dat <<= 1;                                     /* 左移，准备下一位 */
    }
    SDA_H(); I2C_Delay(); SCL_H(); I2C_Delay(); SCL_L(); /* 第9个时钟：主机释放SDA，从机应拉低ACK */
}

/**
 * @brief  向OLED发送一个命令字节
 * @param  cmd 命令码(如0xAE关显示、0xAF开显示等)
 * @note   格式: START + 设备地址(写) + 控制字节(0x00=命令) + 命令码 + STOP
 *         控制字节0x00表示后续字节为命令，0x40表示为数据
 */
static void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start(); I2C_WriteByte(OLED_ADDR); I2C_WriteByte(0x00); I2C_WriteByte(cmd); I2C_Stop();
}

/*==============================================================================
 * 帧缓冲区
 * SSD1306为128x64像素，每列8个像素组成一页(字节)，共8页(64/8=8)
 * 缓冲区大小: 128列 x 8页 = 1024字节
 * 缓冲区索引计算: FB[ 列号 + 页号 * 128 ]
 *============================================================================*/
static uint8_t FB[1024];                               /* 帧缓冲区：1024字节(128x64/8) */

/**
 * @brief  获取帧缓冲区指针
 * @return uint8_t* 指向帧缓冲区的指针
 * @note   外部模块可通过此指针直接读写缓冲区，提高效率
 *         用于图像合成、动画帧生成等场景
 */
uint8_t *OLED_GetFrameBuffer(void) { return FB; }

/**
 * @brief  清空帧缓冲区
 * @note   将所有字节置0，等效于全屏熄灭
 *         调用后需调用OLED_Refresh()更新屏幕显示
 */
void OLED_Clear(void) { memset(FB, 0, 1024); }

/**
 * @brief  刷新OLED显示
 * @note   将帧缓冲区数据批量传输到SSD1306的GDDRAM
 *         SSD1306显存映射：共8页(page0~page7)，每页128列
 *   传输流程:
 *     1. 设置页地址 (0xB0 + 页号)
 *     2. 设置列低4位 (0x00)
 *     3. 设置列高4位 (0x10)
 *     4. 连续发送128字节数据(控制字节0x40=数据模式)
 *     5. 循环8页完成整屏刷新
 */
void OLED_Refresh(void)
{
    for (uint8_t pg = 0; pg < 8; pg++) {
        OLED_WriteCmd(0xB0 + pg);                      /* 设置页起始地址(page 0~7) */
        OLED_WriteCmd(0x00);                           /* 设置列起始地址低4位 */
        OLED_WriteCmd(0x10);                           /* 设置列起始地址高4位 */
        I2C_Start(); I2C_WriteByte(OLED_ADDR);         /* 发送设备地址 */
        I2C_WriteByte(0x40);                           /* 控制字节：接下来是数据流 */
        for (uint8_t i = 0; i < 128; i++)              /* 连续发送128列数据 */
            I2C_WriteByte(FB[pg * 128 + i]);           /* 从缓冲区取当前页数据 */
        I2C_Stop();                                    /* 每页传输结束 */
    }
}

/**
 * @brief  在指定坐标绘制一个像素点
 * @param  x 列坐标(0~127)，从左到右
 * @param  y 行坐标(0~63)，从上到下
 * @param  t 像素颜色：1=点亮，0=熄灭
 * @note   采用页+位映射方式：
 *         页索引 = y / 8，位偏移 = y % 8
 *         缓冲区地址 = x + (y/8) * 128
 *         坐标越界时自动忽略防止缓冲区溢出
 */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t)
{
    if (x >= 128 || y >= 64) return;                   /* 坐标越界保护 */
    if (t) FB[x + (y >> 3) * 128] |= (1 << (y & 7));  /* 点亮：对应bit置1 */
    else   FB[x + (y >> 3) * 128] &= ~(1 << (y & 7)); /* 熄灭：对应bit清0 */
}

/*==============================================================================
 * 6x8 ASCII字库(95个可打印字符)
 * 每个字符6字节宽 x 8像素高，每字节对应一列
 * 索引0对应空格(0x20)，索引94对应'~'(0x7E)
 * 位映射：字节的最低位对应像素的顶部
 *============================================================================*/
static const uint8_t F6x8[95][6] = {
/* 0x20 Space */ {0x00,0x00,0x00,0x00,0x00,0x00},
/* 0x21   !   */ {0x00,0x00,0x5F,0x00,0x00,0x00},
/* 0x22   "   */ {0x00,0x07,0x00,0x07,0x00,0x00},
/* 0x23   #   */ {0x14,0x7F,0x14,0x7F,0x14,0x00},
/* 0x24   $   */ {0x24,0x2A,0x7F,0x2A,0x12,0x00},
/* 0x25   %   */ {0x23,0x13,0x08,0x64,0x62,0x00},
/* 0x26   &   */ {0x36,0x49,0x55,0x22,0x50,0x00},
/* 0x27   '   */ {0x00,0x05,0x03,0x00,0x00,0x00},
/* 0x28   (   */ {0x00,0x1C,0x22,0x41,0x00,0x00},
/* 0x29   )   */ {0x00,0x41,0x22,0x1C,0x00,0x00},
/* 0x2A   *   */ {0x14,0x08,0x3E,0x08,0x14,0x00},
/* 0x2B   +   */ {0x08,0x08,0x3E,0x08,0x08,0x00},
/* 0x2C   ,   */ {0x00,0x50,0x30,0x00,0x00,0x00},
/* 0x2D   -   */ {0x08,0x08,0x08,0x08,0x08,0x00},
/* 0x2E   .   */ {0x00,0x60,0x60,0x00,0x00,0x00},
/* 0x2F   /   */ {0x20,0x10,0x08,0x04,0x02,0x00},
/* 0x30   0   */ {0x3E,0x51,0x49,0x45,0x3E,0x00},
/* 0x31   1   */ {0x00,0x42,0x7F,0x40,0x00,0x00},
/* 0x32   2   */ {0x42,0x61,0x51,0x49,0x46,0x00},
/* 0x33   3   */ {0x21,0x41,0x45,0x4B,0x31,0x00},
/* 0x34   4   */ {0x18,0x14,0x12,0x7F,0x10,0x00},
/* 0x35   5   */ {0x27,0x45,0x45,0x45,0x39,0x00},
/* 0x36   6   */ {0x3C,0x4A,0x49,0x49,0x30,0x00},
/* 0x37   7   */ {0x01,0x71,0x09,0x05,0x03,0x00},
/* 0x38   8   */ {0x36,0x49,0x49,0x49,0x36,0x00},
/* 0x39   9   */ {0x06,0x49,0x49,0x29,0x1E,0x00},
/* 0x3A   :   */ {0x00,0x36,0x36,0x00,0x00,0x00},
/* 0x3B   ;   */ {0x00,0x56,0x36,0x00,0x00,0x00},
/* 0x3C   <   */ {0x08,0x14,0x22,0x41,0x00,0x00},
/* 0x3D   =   */ {0x14,0x14,0x14,0x14,0x14,0x00},
/* 0x3E   >   */ {0x41,0x22,0x14,0x08,0x00,0x00},
/* 0x3F   ?   */ {0x02,0x01,0x51,0x09,0x06,0x00},
/* 0x40   @   */ {0x32,0x49,0x79,0x41,0x3E,0x00},
/* 0x41   A   */ {0x7E,0x11,0x11,0x11,0x7E,0x00},
/* 0x42   B   */ {0x7F,0x49,0x49,0x49,0x36,0x00},
/* 0x43   C   */ {0x3E,0x41,0x41,0x41,0x22,0x00},
/* 0x44   D   */ {0x7F,0x41,0x41,0x22,0x1C,0x00},
/* 0x45   E   */ {0x7F,0x49,0x49,0x49,0x41,0x00},
/* 0x46   F   */ {0x7F,0x09,0x09,0x01,0x01,0x00},
/* 0x47   G   */ {0x3E,0x41,0x41,0x51,0x32,0x00},
/* 0x48   H   */ {0x7F,0x08,0x08,0x08,0x7F,0x00},
/* 0x49   I   */ {0x00,0x41,0x7F,0x41,0x00,0x00},
/* 0x4A   J   */ {0x20,0x40,0x41,0x3F,0x01,0x00},
/* 0x4B   K   */ {0x7F,0x08,0x14,0x22,0x41,0x00},
/* 0x4C   L   */ {0x7F,0x40,0x40,0x40,0x40,0x00},
/* 0x4D   M   */ {0x7F,0x02,0x04,0x02,0x7F,0x00},
/* 0x4E   N   */ {0x7F,0x04,0x08,0x10,0x7F,0x00},
/* 0x4F   O   */ {0x3E,0x41,0x41,0x41,0x3E,0x00},
/* 0x50   P   */ {0x7F,0x09,0x09,0x09,0x06,0x00},
/* 0x51   Q   */ {0x3E,0x41,0x51,0x21,0x5E,0x00},
/* 0x52   R   */ {0x7F,0x09,0x19,0x29,0x46,0x00},
/* 0x53   S   */ {0x46,0x49,0x49,0x49,0x31,0x00},
/* 0x54   T   */ {0x01,0x01,0x7F,0x01,0x01,0x00},
/* 0x55   U   */ {0x3F,0x40,0x40,0x40,0x3F,0x00},
/* 0x56   V   */ {0x1F,0x20,0x40,0x20,0x1F,0x00},
/* 0x57   W   */ {0x7F,0x20,0x18,0x20,0x7F,0x00},
/* 0x58   X   */ {0x63,0x14,0x08,0x14,0x63,0x00},
/* 0x59   Y   */ {0x03,0x04,0x78,0x04,0x03,0x00},
/* 0x5A   Z   */ {0x61,0x51,0x49,0x45,0x43,0x00},
/* 0x5B   [   */ {0x00,0x7F,0x41,0x41,0x00,0x00},
/* 0x5C   \   */ {0x02,0x04,0x08,0x10,0x20,0x00},
/* 0x5D   ]   */ {0x00,0x41,0x41,0x7F,0x00,0x00},
/* 0x5E   ^   */ {0x04,0x02,0x01,0x02,0x04,0x00},
/* 0x5F   _   */ {0x40,0x40,0x40,0x40,0x40,0x00},
/* 0x60   `   */ {0x00,0x01,0x02,0x04,0x00,0x00},
/* 0x61   a   */ {0x20,0x54,0x54,0x54,0x78,0x00},
/* 0x62   b   */ {0x7F,0x48,0x44,0x44,0x38,0x00},
/* 0x63   c   */ {0x38,0x44,0x44,0x44,0x20,0x00},
/* 0x64   d   */ {0x38,0x44,0x44,0x48,0x7F,0x00},
/* 0x65   e   */ {0x38,0x54,0x54,0x54,0x18,0x00},
/* 0x66   f   */ {0x08,0x7E,0x09,0x01,0x02,0x00},
/* 0x67   g   */ {0x0C,0x52,0x52,0x52,0x3E,0x00},
/* 0x68   h   */ {0x7F,0x08,0x04,0x04,0x78,0x00},
/* 0x69   i   */ {0x00,0x44,0x7D,0x40,0x00,0x00},
/* 0x6A   j   */ {0x20,0x40,0x44,0x3D,0x00,0x00},
/* 0x6B   k   */ {0x7F,0x10,0x28,0x44,0x00,0x00},
/* 0x6C   l   */ {0x00,0x41,0x7F,0x40,0x00,0x00},
/* 0x6D   m   */ {0x7C,0x04,0x78,0x04,0x78,0x00},
/* 0x6E   n   */ {0x7C,0x08,0x04,0x04,0x78,0x00},
/* 0x6F   o   */ {0x38,0x44,0x44,0x44,0x38,0x00},
/* 0x70   p   */ {0x7C,0x14,0x14,0x14,0x08,0x00},
/* 0x71   q   */ {0x08,0x14,0x14,0x18,0x7C,0x00},
/* 0x72   r   */ {0x7C,0x08,0x04,0x04,0x08,0x00},
/* 0x73   s   */ {0x48,0x54,0x54,0x54,0x20,0x00},
/* 0x74   t   */ {0x04,0x3F,0x44,0x40,0x20,0x00},
/* 0x75   u   */ {0x3C,0x40,0x40,0x20,0x7C,0x00},
/* 0x76   v   */ {0x1C,0x20,0x40,0x20,0x1C,0x00},
/* 0x77   w   */ {0x3C,0x40,0x30,0x40,0x3C,0x00},
/* 0x78   x   */ {0x44,0x28,0x10,0x28,0x44,0x00},
/* 0x79   y   */ {0x0C,0x50,0x50,0x50,0x3C,0x00},
/* 0x7A   z   */ {0x44,0x64,0x54,0x4C,0x44,0x00},
/* 0x7B   {   */ {0x00,0x08,0x36,0x41,0x00,0x00},
/* 0x7C   |   */ {0x00,0x00,0x7F,0x00,0x00,0x00},
/* 0x7D   }   */ {0x00,0x41,0x36,0x08,0x00,0x00},
/* 0x7E   ~   */ {0x08,0x08,0x2A,0x1C,0x08,0x00},
};

/**
 * @brief  OLED显示单个ASCII字符(内部函数)
 * @param  x    起始列坐标(0~127)
 * @param  y    起始行坐标(0~63)
 * @param  chr  要显示的ASCII字符
 * @param  mode 颜色模式：1=前景色点亮, 0=前景色熄灭
 * @note   字符宽6像素，高8像素
 *         非可打印字符(<32或>126)自动替换为空格(0x20)
 *         通过逐列逐位调用OLED_DrawPoint实现
 */
static void OLED_ShowChar(u8 x, u8 y, u8 chr, u8 mode)
{
    if (chr < 32 || chr > 126) chr = 32;               /* 非可打印字符→空格 */
    chr -= 32;                                          /* 字库索引偏移(从空格开始) */
    for (u8 i = 0; i < 6; i++)                          /* 每列 */
        for (u8 j = 0; j < 8; j++)                      /* 每行(8位) */
            if (F6x8[chr][i] >> j & 1)                  /* 取字模对应位 */
                OLED_DrawPoint(x + i, y + j, mode);     /* 绘制像素点 */
}

/**
 * @brief  在OLED指定位置显示图片
 * @param  x    图片左上角列坐标(0~127)
 * @param  y    图片左上角行坐标(0~63)
 * @param  w    图片宽度(像素)
 * @param  h    图片高度(像素)
 * @param  pic  图片数据指针(逐字节，每字节含8行1列的数据)
 * @param  mode 颜色模式：1=原样显示, 0=反色显示
 * @note   图片数据按列排列：每字节的LSB对应最顶上一行
 *         高度不足8的倍数时自动向上取整页数
 *         用于显示小图标或自定义图形
 */
void OLED_ShowPicture(u8 x, u8 y, u8 w, u8 h, const u8 *pic, u8 mode)
{
    u16 j = 0;                                          /* 图片数据索引 */
    u8 i, n, m;
    u8 x0 = x;                                          /* 保存起始列坐标(每页重置) */
    u8 pages = h / 8 + ((h % 8) ? 1 : 0);               /* 图片占用的页数(向上取整) */
    for (n = 0; n < pages; n++) {
        u8 cy = y + n * 8;                              /* 当前页起始行 */
        x = x0;                                         /* 重置列到起始位置 */
        for (i = 0; i < w; i++) {
            u8 temp = pic[j++];                         /* 取一字节(8行数据) */
            for (m = 0; m < 8; m++) {
                if (temp & 0x01) OLED_DrawPoint(x, cy + m, mode);  /* 像素点亮 */
                else OLED_DrawPoint(x, cy + m, !mode);              /* 像素点灭(反色) */
                temp >>= 1;                              /* 处理下一位 */
            }
            x++;                                         /* 下一列 */
        }
    }
}

/**
 * @brief  在OLED指定位置显示ASCII字符串
 * @param  x    起始列坐标(字符左上角)
 * @param  y    起始行坐标
 * @param  str  待显示字符串指针(以'\0'结尾)
 * @param  mode 颜色模式
 * @note   每个字符占6x8像素，字符间无间隔
 *         自动换行：当x超过122(128-6)时，换到下一行(x=0, y+=10)
 *         显示'\0'时自动终止
 */
void OLED_ShowString(u8 x, u8 y, const u8 *str, u8 mode)
{
    while (*str) {
        OLED_ShowChar(x, y, *str, mode);               /* 显示当前字符 */
        x += 6;                                         /* 字符宽度6像素 */
        if (x > 122) { x = 0; y += 10; }                /* 超出屏幕宽度则换行(10像素行距) */
        str++;                                          /* 指向下一个字符 */
    }
}

/**
 * @brief  OLED初始化
 * @note   SSD1306完整初始化序列:
 *   1. GPIO时钟使能 + 引脚配置(推挽输出50MHz)
 *   2. 硬件复位: RES低电平10ms → 高电平10ms
 *   3. 延迟100ms等待内部VDD/DIS充电完成
 *   4. SSD1306寄存器配置(显示关闭→分段映射→对比度→显示开)
 *   5. 清屏并刷新
 *
 *   SSD1306关键寄存器配置说明:
 *   0xAE/0xAF - 显示关闭/开启
 *   0x20 0x00 - 水平寻址模式
 *   0xB0      - 页起始地址
 *   0xC8      - COM扫描方向(从COM[N-1]到COM0)
 *   0x40      - 显示起始行
 *   0x81 0xFF - 对比度最大
 *   0xA1      - 分段重映射(列地址127映射到SEG0)
 *   0xA6      - 正常显示(非反色)
 *   0xA8 0x3F - 多路复用比(64行)
 *   0xA4      - 全屏显示跟随GDDRAM内容
 *   0xD3 0x00 - 显示偏移(0)
 *   0xD5 0xF0 - 时钟分频/振荡频率
 *   0xD9 0x22 - 预充电周期
 *   0xDA 0x12 - COM引脚硬件配置
 *   0xDB 0x20 - VCOMH电压
 *   0x8D 0x14 - 电荷泵使能(内部DC-DC升压)
 */
void OLED_Init(void)
{
    /* 使能GPIOG和GPIOD时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG | RCC_APB2Periph_GPIOD, ENABLE);
    GPIO_InitTypeDef gi;
    gi.GPIO_Mode = GPIO_Mode_Out_PP;                    /* 推挽输出 */
    gi.GPIO_Speed = GPIO_Speed_50MHz;                   /* 50MHz输出速率 */
    gi.GPIO_Pin = OLED_SCL_PIN; GPIO_Init(OLED_SCL_PORT, &gi);  /* 初始化SCL */
    gi.GPIO_Pin = OLED_SDA_PIN; GPIO_Init(OLED_SDA_PORT, &gi);  /* 初始化SDA */
    gi.GPIO_Pin = RES_PIN; GPIO_Init(RES_PORT, &gi);             /* 初始化RES */

    /* 硬件复位序列 */
    RES_L(); delay_ms(10);                               /* RES拉低≥3us复位 */
    RES_H(); delay_ms(10);                               /* RES释放进入正常工作 */
    SCL_H(); SDA_H();                                    /* I2C总线初始化为空闲状态 */
    delay_ms(100);                                       /* 等待OLED内部初始化完成 */

    /* SSD1306配置序列 */
    OLED_WriteCmd(0xAE);                               /* 关闭显示 */
    OLED_WriteCmd(0x20); OLED_WriteCmd(0x00);          /* 设置寻址模式：水平寻址 */
    OLED_WriteCmd(0xB0);                               /* 页起始地址：page 0 */
    OLED_WriteCmd(0xC8);                               /* COM扫描方向：从COM[N-1]到COM0 (bottom-up) */
    OLED_WriteCmd(0x00); OLED_WriteCmd(0x10);          /* 列起始地址：低高字节 */
    OLED_WriteCmd(0x40);                               /* 显示起始行：row 0 */
    OLED_WriteCmd(0x81); OLED_WriteCmd(0xFF);          /* 设置对比度：最大值 */
    OLED_WriteCmd(0xA1);                               /* 段重映射：列127→SEG0 (左右镜像) */
    OLED_WriteCmd(0xA6);                               /* 正常显示(非反色) */
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F);          /* 多路复用比：64行(MUX=63) */
    OLED_WriteCmd(0xA4);                               /* 全局显示：跟随GDDRAM内容 */
    OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00);          /* 显示偏移：0 */
    OLED_WriteCmd(0xD5); OLED_WriteCmd(0xF0);          /* 时钟分频/振荡频率 */
    OLED_WriteCmd(0xD9); OLED_WriteCmd(0x22);          /* 预充电周期：2 DCLK */
    OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12);          /* COM引脚配置：交替模式 */
    OLED_WriteCmd(0xDB); OLED_WriteCmd(0x20);          /* VCOMH电压：0.77xVCC */
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14);          /* 电荷泵使能 */
    OLED_WriteCmd(0xAF);                               /* 开启显示 */

    OLED_Clear();                                       /* 清空缓冲区 */
    OLED_Refresh();                                     /* 刷新显示(全屏熄灭) */
}

/**
 * @brief  在OLED指定区域显示单色位图
 * @param  pic 位图数据指针
 * @param  x   起始列坐标(0~127)
 * @param  y   起始行坐标(0~63)
 * @param  w   位图宽度(像素)，必须是8的倍数
 * @param  h   位图高度(像素)，必须是8的倍数
 * @note   位图数据组织方式：逐列逐页排列
 *         每字节的bit n对应当前列中第n行的像素(LSB在上)
 *         与SSD1306的GDDRAM映射一致，可直接写入
 *         坐标越界保护：超出屏幕范围的像素自动忽略
 *         适用于全屏或区域位图刷新
 */
void OLED_ShowImage(const u8 *pic, u8 x, u8 y, u8 w, u8 h)
{
    u16 j = 0;                                          /* 位图数据索引 */
    u8 pages = h / 8;                                   /* 位图占用的页数 */
    for (u8 n = 0; n < pages; n++) {
        u8 y0 = y + n * 8;                              /* 当前页起始行坐标 */
        for (u8 col = 0; col < w; col++) {
            u8 dat = pic[j++];                           /* 取一字节(当前列当前页的8行数据) */
            for (u8 bit = 0; bit < 8; bit++) {
                u8 px = x + col;                         /* 计算像素列坐标 */
                u8 py = y0 + bit;                        /* 计算像素行坐标 */
                if (px < 128 && py < 64) {               /* 坐标有效性检查 */
                    if (dat & (1 << bit))                /* 位图该位点亮 */
                        FB[px + (py / 8) * 128] |= (1 << (py % 8));   /* 缓冲区对应位置1 */
                    else
                        FB[px + (py / 8) * 128] &= ~(1 << (py % 8));  /* 缓冲区对应位清0 */
                }
            }
        }
    }
}
