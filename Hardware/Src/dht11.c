/******************************************************************************
 * 文件名: dht11.c
 * 描  述: DHT11温湿度传感器驱动实现文件
 * 说  明: 本文件实现了DHT11数字温湿度传感器的单总线通信协议驱动。
 *         DHT11采用单总线（Single Bus）双向通信，一次完整的数据传输
 *         包含5个字节（40位）：湿度整数、湿度小数、温度整数、温度小数、校验和。
 *         通信时序要求严格，通过延时和超时机制保证可靠性。
 * 硬  件: DATA引脚 → PA1 (双向GPIO，通过方向切换实现单总线)
 *         时钟: SYSCLK = 72MHz
 * 协  议: 主机拉低 > 18ms → 拉高 20~40us → DHT响应低80us → DHT拉高80us
 *         → 数据位 '0' (低50us + 高26~28us) / 数据位 '1' (低50us + 高70us)
 ******************************************************************************/
#include "stm32f10x.h"
#include "delay.h"
#include "dht11.h"

/**
 * @brief  DHT11数据引脚配置为浮空输入模式。
 *         在等待DHT11响应或读取数据时使用，
 *         此时引脚处于高阻输入状态，由DHT11控制总线电平。
 * @note  GPIOA Pin1 配置为 GPIO_Mode_IN_FLOATING (浮空输入)
 */
void DHT_Init_InPut(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);   /* 开启GPIOA时钟 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;   /* 浮空输入模式 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;               /* DATA引脚 = PA1 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       /* GPIO速度50MHz */
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
 * @brief  DHT11数据引脚配置为推挽输出模式。
 *         在主机发送起始信号时使用，
 *         由主机控制总线电平（拉低/拉高）。
 * @note  GPIOA Pin1 配置为 GPIO_Mode_Out_PP (推挽输出)
 */
void DHT_Init_OutPut(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);   /* 开启GPIOA时钟 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        /* 推挽输出模式 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;               /* DATA引脚 = PA1 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       /* GPIO速度50MHz */
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
 * @brief  发送DHT11起始信号。
 *         时序要求：
 *           1. 主机将总线拉低至少18ms（DHT11检测到后准备响应）
 *           2. 主机将总线拉高20~40us（释放总线并等待DHT11响应）
 *           3. 切换为输入模式，等待DHT11拉低总线作为响应
 * @note   起始信号必须由主机主动发起，DHT11在低功耗待机状态下等待此信号。
 *         拉低时间不足18ms可能导致DHT11无响应。
 */
void DHT_STart(void)
{
    DHT_Init_OutPut();                      /* 切换为输出模式，主机控制总线 */
    GPIO_ResetBits(GPIOA, GPIO_Pin_1);      /* 拉低数据线，发送起始信号 */
    delay_ms(20);                           /* 保持低电平20ms (>18ms，满足时序要求) */
    GPIO_SetBits(GPIOA, GPIO_Pin_1);        /* 拉高数据线，准备释放总线 */
    delay_us(30);                           /* 保持高电平30us (20~40us之间) */
    DHT_Init_InPut();                       /* 切换为输入模式，等待DHT11响应 */
}

/**
 * @brief  读取DHT11数据引脚的当前电平状态。
 * @return uint16_t 0 — 低电平 (RESET)；
 *                   1 — 高电平 (SET)。
 */
uint16_t DHT_Scan(void)
{
    return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1);   /* 读取PA1输入电平 */
}

/**
 * @brief  读取DHT11的一位数据。
 *         DHT11数据位格式：
 *           位 '0' : 低电平约50us + 高电平约26~28us
 *           位 '1' : 低电平约50us + 高电平约70us
 *         读取方法：先等待低电平结束（50us），再延时40us后采样。
 *           若采样为高电平 → 位 '1'（说明高电平剩余 > 40us）
 *           若采样为低电平 → 位 '0'（说明高电平已结束）
 * @return uint16_t 0 或 1，表示读取到的数据位；
 *         超时返回0（作为超时保护，避免死循环）。
 * @note   每位的低电平部分约50us，超时阈值500对应约500us，充分覆盖。
 */
uint16_t DHT_ReadBit(void)
{
    uint16_t timeout = 0;

    /* 等待低电平结束（DHT拉低约50us表示数据位开始） */
    while(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == RESET)
    {
        if (++timeout > 500) return 0;      /* 超时约500us，返回0避免死锁 */
    }

    delay_us(40);   /* 延时40us后采样：若高电平仍持续则为'1'，否则为'0' */

    if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == SET)
    {
        /* 采样为高电平 → 数据位 '1' */
        timeout = 0;
        while(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == SET)
        {
            if (++timeout > 500) return 0;  /* 等待高电平结束，超时保护 */
        }
        return 1;
    }
    else
    {
        /* 采样为低电平 → 数据位 '0' */
        return 0;
    }
}

/**
 * @brief  读取DHT11的一个字节数据（8位）。
 *         从最高位 (MSB) 开始依次读取，逐位移入。
 *         DHT11数据字节顺序：高字节在前，低字节在后。
 * @return uint16_t 读取到的8位数据值（实际使用低8位）。
 */
uint16_t DHT_ReadByte(void)
{
    uint16_t i, data = 0;
    for(i = 0; i < 8; i++)
    {
        data <<= 1;                 /* 左移1位，为新位腾出空间 */
        data |= DHT_ReadBit();      /* 读取1位数据并拼接到最低位 */
    }
    return data;
}

/**
 * @brief  读取DHT11的完整温湿度数据（含校验验证）。
 *         一次完整数据帧为40位（5字节），格式如下：
 *           byte[0] — 湿度整数部分 (RH整数)
 *           byte[1] — 湿度小数部分 (RH小数，DHT11始终为0)
 *           byte[2] — 温度整数部分 (T整数)
 *           byte[3] — 温度小数部分 (T小数，DHT11始终为0)
 *           byte[4] — 校验和 = byte[0]+byte[1]+byte[2]+byte[3] 的低8位
 *         通信过程：
 *           1. 发送起始信号（主机拉低>18ms再拉高）
 *           2. DHT响应：拉低80us → 拉高80us
 *           3. 接收40位数据（5字节）
 *           4. 校验和验证
 *           5. 拉高总线释放DHT11
 * @param  buffer[5] 输出参数，存放读取到的5字节数据。
 *                    [0]=湿度整数, [1]=湿度小数, [2]=温度整数,
 *                    [3]=温度小数, [4]=校验和。
 * @return uint16_t 0 — 成功；1 — 校验和错误；2 — 通信超时/DHT未响应。
 */
uint16_t DHT_ReadData(uint8_t buffer[5])
{
    uint8_t i;
    uint16_t timeout;

    DHT_STart();    /* 发送起始信号 */

    /*
     * ---- 等待DHT11响应 ----
     * DHT11接收到起始信号后会将总线拉低约80us作为应答
     * 此处等待总线从高变低（DHT响应），超时5ms
     */
    timeout = 0;
    while(DHT_Scan() != RESET)
    {
        if (++timeout > 5000) return 2;     /* DHT未响应，超时退出 */
    }

    /* DHT11拉低约80us，等待拉低结束 */
    timeout = 0;
    while(DHT_Scan() == RESET)
    {
        if (++timeout > 500) return 2;      /* 超时保护 */
    }

    /* DHT11拉高约80us，准备发送数据，等待拉高结束 */
    timeout = 0;
    while(DHT_Scan() == SET)
    {
        if (++timeout > 500) return 2;      /* 超时保护 */
    }

    /*
     * ---- 读取40位数据（5字节） ----
     * 依次读取湿度整数、湿度小数、温度整数、温度小数、校验和
     */
    for(i = 0; i < 5; i++)
    {
        buffer[i] = DHT_ReadByte();
    }

    /* 等待最后一个字节传输结束（DHT拉低总线） */
    timeout = 0;
    while(DHT_Scan() == RESET)
    {
        if (++timeout > 500) return 2;      /* 超时保护 */
    }

    /*
     * ---- 通信结束，释放总线 ----
     * 切换为输出模式并将总线拉高，DHT11回到待机状态
     */
    DHT_Init_OutPut();
    GPIO_SetBits(GPIOA, GPIO_Pin_1);

    /*
     * ---- 校验和验证 ----
     * 校验和 = 前4字节相加的低8位，应与第5字节相等
     */
    uint8_t check = buffer[0] + buffer[1] + buffer[2] + buffer[3];
    if(check != buffer[4])
    {
        return 1;                           /* 校验失败，数据不可信 */
    }

    return 0;                               /* 读取成功 */
}

/**
 * @brief  旧版DHT11温湿度读取函数（已废弃，保留注释供参考）。
 *         原实现通过 DHT_ReadBit() 检测起始信号后直接读取40位数据，
 *         缺少显式的起始信号发送过程，可靠性较低。
 *         当前推荐使用 DHT_ReadData() 替代。
 * @param  temp  输出参数，温度值
 * @param  humi  输出参数，湿度值
 * @return u8    0 — 成功；1 — 失败
 */
//u8 DHT11_Readdata(u8 *temp,u8 *humi)
//{
//    u8 i,buf[5];
//
//    /* 检测起始信号 */
//    if(DHT_ReadBit()==0){
//        /* 读取40bits(5bytes)数据 */
//        for(i=0;i<5;i++){
//            buf[i] = DHT_ReadByte();
//        }
//        /* 验证校验和 */
//        if(((buf[0]+buf[1]+buf[2]+buf[3])&0xff)==buf[4]){
//            *temp = buf[2];
//            *humi = buf[0];
//            return 0;
//        }
//    }
//
//    return 1;
//}
