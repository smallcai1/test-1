//代码思路：基于 51 单片机设计，在播放《起风了》的基础上增加了创新实现了“背景音乐 + 反应记忆游戏”一体化功能。上电后，单片机首先初始化
//定时器 T0 与相关 I/O 口，通过查表方式在中断中控制蜂鸣器 P2.5 输出不同频率的方波，实现整首乐曲
//的循环播放。与此同时，系统利用 74HC595 驱动 8×8 点阵（或 LED 行列），按随机数组生成并滚动显
//示各种按键组合提示图案，玩家需在图案滚动到最底行时，按下对应的一个或两个按键（P3.0～P3.3）。
//程序在每一轮根据按键状态进行判定：若与当前图案要求一致，则记分变量加 1 分并继续游戏；
//若错误或未按，则游戏结束。结束后，单片机通过多位数码管（段选接 P0、位选接 P2.2～P2.4）
//循环显示难度等级和最终得分，实现一个集音乐播放、随机图案显示、按键交互与成绩显示于一体的综合电子小游戏。



#include "reg52.h"       // 51 系列单片机寄存器定义
#include "intrins.h"     // _nop_ 等内部函数
#include "stdlib.h"      // rand、srand

typedef unsigned char u8;
typedef unsigned int u16;

#define DZLED_SMG_PORT P0   // 段选：数码管 / 点阵共用的数据口

// 蜂鸣器与数码管位选控制
sbit BUZZER = P2^5;         // 蜂鸣器控制端
sbit LSA = P2^2;            // 数码管位选 A
sbit LSB = P2^3;            // 数码管位选 B
sbit LSC = P2^4;            // 数码管位选 C

// 四个按键（难度选择 + 游戏输入）
sbit BUTTER2 = P3^0;
sbit BUTTER1 = P3^1;
sbit BUTTER3 = P3^2;
sbit BUTTER4 = P3^3;

// 74HC595 控制信号（移位寄存器，用于行扫描）
sbit SRCLK = P3^6;          // 移位时钟
sbit rCLK  = P3^5;          // 存储时钟
sbit SER   = P3^4;          // 串行数据输入

/* ---------- 音乐播放参数 ---------- */
u8 m, n;                     // 当前音符索引和节拍时值索引
u16 music_index = 0;         // 当前播放到的音符下标
u16 music_ms_left = 0;       // 当前音符剩余时间（ms）
u8 music_started = 0;        // 是否已经启动音乐播放

/*
 * 原始阻塞版的 delay(n) 约等价于较大的时间单位。
 * 并行版用毫秒节拍模拟时值时，需要更大的基准才能接近原速。
 * 可以按实际效果微调此值（越大则整体节奏越慢）。
 */
#define MUSIC_UNIT_MS 160
// 音阶频率定时初值表：TH0/TL0 对应不同音高
u8 code T[49][2] = {
    {0,0},
    {0xF8,0x8B},{0xF8,0xF2},{0xF9,0x5B},{0xF9,0xB7},{0xFA,0x14},{0xFA,0x66},{0xFA,0xB9},{0xFB,0x03},{0xFB,0x4A},{0xFB,0x8F},{0xFB,0xCF},{0xFC,0x0B},
    {0xFC,0x43},{0xFC,0x78},{0xFC,0xAB},{0xFC,0xDB},{0xFD,0x08},{0xFD,0x33},{0xFD,0x5B},{0xFD,0x81},{0xFD,0xA5},{0xFD,0xC7},{0xFD,0xE7},{0xFE,0x05},
    {0xFE,0x21},{0xFE,0x3C},{0xFE,0x55},{0xFE,0x6D},{0xFE,0x84},{0xFE,0x99},{0xFE,0xAD},{0xFE,0xC0},{0xFE,0x02},{0xFE,0xE3},{0xFE,0xF3},{0xFF,0x02},
    {0xFF,0x10},{0xFF,0x1D},{0xFF,0x2A},{0xFF,0x36},{0xFF,0x42},{0xFF,0x4C},{0xFF,0x56},{0xFF,0x60},{0xFF,0x69},{0xFF,0x71},{0xFF,0x79},{0xFF,0x81}
};

// 乐谱表：music[i][0] 为音符序号，对应 T 表；music[i][1] 为时值单位
u8 code music[][2] = {
    {0,4},
    {18,2},{19,2},{21,2},{23,2},{11,4},{26,2},{23,6},{0,2},{18,2},{19,2},{21,2},{23,2},{9,4},{26,2},{23,2},{21,2},{23,2},{19,2},{21,2},{18,2},{19,2},
    {14,2},{0,4},{18,2},{19,2},{21,2},{23,2},{11,4},{26,2},{23,6},{0,2},{18,2},{19,2},{21,2},{23,2},{9,4},{26,2},{23,2},{21,2},{23,2},{19,2},{21,2},
    {18,2},{19,2},{14,2},{0,4},{9,6},{7,2},{9,6},{7,2},{9,4},{11,4},{14,4},{11,4},{9,6},{7,2},{9,6},{7,2},{9,2},{11,2},{9,2},{7,2},{1,4},{0,4},{9,6},
    {7,2},{9,6},{7,2},{9,4},{11,4},{14,4},{11,4},{9,6},{11,2},{9,4},{7,4},{9,8},{0,8},{9,6},{7,2},{9,6},{7,2},{9,4},{11,4},{14,4},{11,4},{9,6},{11,2},
    {9,4},{7,4},{3,4},{0,4},{11,2},{9,2},{7,2},{9,2},{7,4},{0,4},{11,2},{9,2},{7,2},{9,2},{7,4},{0,4},{11,2},{9,2},{7,2},{9,2},{7,8},{0,8},{0,4},{7,4},
    {9,4},{11,4},{7,4},{16,4},{14,2},{16,6},{0,2},{7,2},{18,4},{16,2},{18,6},{0,4},{18,4},{16,2},{18,6},{11,4},{19,2},{21,2},{19,2},{18,2},{16,4},{14,4},
    {16,4},{14,2},{16,4},{14,2},{16,2},{14,2},{16,4},{14,2},{9,4},{14,4},{11,8},{0,8},{7,4},{9,4},{11,4},{7,4},{16,4},{14,2},{16,6},{0,2},{7,2},{18,4},
    {16,2},{18,6},{0,4},{18,4},{16,2},{18,6},{11,4},{19,2},{21,2},{19,2},{18,2},{16,4},{14,4},{16,4},{23,2},{23,6},{14,4},{16,4},{23,2},{23,4},{14,4},
    {16,2},{16,12},{0,4},{19,4},{21,4},{23,4},{28,2},{26,6},{28,2},{26,6},{28,2},{26,6},{21,4},{23,6},{28,2},{26,6},{28,2},{26,6},{28,2},{26,4},{23,8},
    {21,4},{19,2},{16,4},{19,4},{19,2},{21,4},{19,2},{16,4},{19,4},{23,8},{23,4},{21,6},{0,4},{19,4},{21,4},{23,4},{28,2},{26,6},{28,2},{26,6},{28,2},
    {26,6},{0,2},{21,2},{23,4},{28,2},{26,6},{28,2},{26,6},{28,2},{26,6},{23,6},{21,4},{19,2},{16,4},{23,4},{21,4},{19,2},{16,4},{16,2},{19,2},{19,8},
    {0,4},{16,2},{23,6},{21,4},{19,2},{16,4},{23,4},{21,4},{19,2},{16,4},{16,2},{19,6},{19,8},{0xFF,0xFF}
};

/* ---------- 游戏参数 ---------- */
int seed, my_code, r, sGSMG;        // 随机种子、得分、随机索引、数码管位数
char flag, mod, key = 0;            // 标志位、难度模式、点阵空/实切换标志

// 74HC595 行扫描控制字（逐行点亮）
char ghc595[8] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
// 随机图案编码（对应不同按键/组合）
char random_arrange[10] = {0x7f,0xdf,0xf7,0xfd,0x5f,0x77,0x7d,0xd7,0xdd,0xf5};
// 数码管段码（0~9）
char gsmg_code[10] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
// 点阵当前 8 行图案（从上到下）
char new_arrange[8] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
// 数码管显示缓存：[0] 显示难度，其余位显示得分
char new_gsmg_code[8] = {0xff,0x40,0xff,0xff,0xff,0xff,0xff,0xff};

// 音乐 1ms 节拍函数（由 Delayms 周期调用）
void music_tick_1ms(void);

void Delay10us(void)
{
    unsigned char data i;
    i = 2;
    while(--i);
}

void Delay500us(void)
{
    unsigned char data i;
    _nop_();
    i = 227;
    while(--i);
}

void Delayms(int n)
{
    unsigned char data i, j;
    int k;
    for(k=0; k<n; k++) {
        _nop_();
        i = 2;
        j = 199;
        do {
            while(--j);       // 约 1ms 的延时（与晶振相关）
        } while(--i);
        music_tick_1ms();      // 每 1ms 推进一次音乐节拍
    }
}

void hc595(char dat)
{
    char i;
    for(i=0; i<8; i++) {
        SER = dat >> 7;
        dat <<= 1;
        SRCLK = 0;
        Delay10us();
        SRCLK = 1;
        Delay10us();
    }
    rCLK = 0;
    Delay10us();
    rCLK = 1;
}

void GSMG(int number)
{
    switch(number) {
    case 0: LSA=1; LSB=1; LSC=1; break;
    case 1: LSA=0; LSB=1; LSC=1; break;
    case 2: LSA=1; LSB=0; LSC=1; break;
    case 3: LSA=0; LSB=0; LSC=1; break;
    case 4: LSA=1; LSB=1; LSC=0; break;
    case 5: LSA=0; LSB=1; LSC=0; break;
    case 6: LSA=1; LSB=0; LSC=0; break;
    case 7: LSA=0; LSB=0; LSC=0; break;
    }
}

// 选择难度模式并获取随机种子：
// 在等待按键的过程中不断自增 seed，用作 srand 的种子，
// 同时根据按下的是哪个键确定难度等级 1~4。
void get_seed_mod(void)
{
    seed = 1;
    while(1) {
        seed += 1;
        if(BUTTER1==0 || BUTTER2==0 || BUTTER3==0 || BUTTER4==0) {
            Delay10us();
            if(BUTTER1==0) {
                mod = 1;
                new_gsmg_code[0] = 0x06;
                break;
            }
            if(BUTTER2==0) {
                mod = 2;
                new_gsmg_code[0] = 0x5b;
                break;
            }
            if(BUTTER3==0) {
                mod = 3;
                new_gsmg_code[0] = 0x4f;
                break;
            }
            if(BUTTER4==0) {
                mod = 4;
                new_gsmg_code[0] = 0x66;
                break;
            }
        }
    }
}

// 刷新点阵：
//   1) 所有行整体下移一行；
//   2) key 在 0/1 间交替：
//      - key==0：在顶行生成新的随机图案；
//      - key==1：在顶行填充空行 0xff，实现图案间空隙效果。
void reflash(void)
{
    char k;
    for(k=6; k>=0; k--) {
        new_arrange[k+1] = new_arrange[k];
    }

    if(key == 0) {
        key = 1;
        r = rand() % 10;
        new_arrange[0] = random_arrange[r];
    } else {
        key = 0;
        new_arrange[0] = 0xff;
    }
}

// 根据最底行图案 new_arrange[7] 判断玩家按键是否正确。
// 按对：flag=1，my_code++；按错或无按键：flag 保持 0。
void check(void)
{
    flag = 0;
    switch(new_arrange[7]) {
    case 0x7f: if(BUTTER1==0) {flag=1; my_code+=1;} break;
    case 0xdf: if(BUTTER2==0) {flag=1; my_code+=1;} break;
    case 0xf7: if(BUTTER3==0) {flag=1; my_code+=1;} break;
    case 0xfd: if(BUTTER4==0) {flag=1; my_code+=1;} break;
    case 0x5f: if(BUTTER1==0 && BUTTER2==0) {flag=1; my_code+=1;} break;
    case 0x77: if(BUTTER1==0 && BUTTER3==0) {flag=1; my_code+=1;} break;
    case 0x7d: if(BUTTER1==0 && BUTTER4==0) {flag=1; my_code+=1;} break;
    case 0xd7: if(BUTTER2==0 && BUTTER3==0) {flag=1; my_code+=1;} break;
    case 0xdd: if(BUTTER2==0 && BUTTER4==0) {flag=1; my_code+=1;} break;
    case 0xf5: if(BUTTER3==0 && BUTTER4==0) {flag=1; my_code+=1;} break;
    default: flag=1; break;
    }
}

// 将最终得分转换为数码管段码存入 new_gsmg_code
void note_code(int score)
{
    int g, b, s, m2;
    m2 = score / 10;
    if(m2 == 0) {
        sGSMG = 3;
        g = score % 10;
        new_gsmg_code[2] = gsmg_code[g];
    } else if(m2 >= 10) {
        sGSMG = 5;
        g = score % 10;
        s = (score / 10) % 10;
        b = score / 100;
        new_gsmg_code[2] = gsmg_code[b];
        new_gsmg_code[3] = gsmg_code[s];
        new_gsmg_code[4] = gsmg_code[g];
    } else {
        sGSMG = 4;
        g = score % 10;
        s = (score / 10) % 10;
        new_gsmg_code[2] = gsmg_code[s];
        new_gsmg_code[3] = gsmg_code[g];
    }
}

// 简单鸣叫，用于每轮结束后提示一次
void BUZZERRING(void)
{
    int t = 50;
    while(t--) {
        BUZZER = !BUZZER;
        Delay500us();
    }
}

// 定时器0初始化：方式1，打开中断但先不启动计数
void music_timer0_init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;
    TH0 = 0x00;
    TL0 = 0x00;
    ET0 = 1;
    EA = 1;
    TR0 = 0;
}

// 定时器0中断服务函数：
// 每次溢出翻转一次 BUZZER，引脚输出方波，频率由 TH0/TL0 决定。
void T0_int(void) interrupt 1
{
    BUZZER = !BUZZER;
    TH0 = T[m][0];
    TL0 = T[m][1];
}

// 装载下一音符：
//   - 读出乐谱表 music[music_index]；
//   - 0xFF 视为乐曲结束，重头循环；
//   - m==0 表示休止，关闭定时器0；

void music_load_next_note(void)
{
    m = music[music_index][0];
    n = music[music_index][1];

    if(m == 0xFF) {
        music_index = 0;
        m = music[music_index][0];
        n = music[music_index][1];
    }

    if(m == 0x00) {
        TR0 = 0;
        BUZZER = 0;
    } else {
        TH0 = T[m][0];
        TL0 = T[m][1];
        TR0 = 1;
    }

    music_ms_left = (u16)n * MUSIC_UNIT_MS;
    if(music_ms_left == 0) {
        music_ms_left = MUSIC_UNIT_MS;
    }

    music_index++;
}

// 开始播放音乐
void music_start(void)
{
    music_index = 0;
    music_ms_left = 0;
    music_started = 1;
    music_load_next_note();
}

// 每 1ms 调用一次，用于递减当前音符剩余时间，
// 当计数归零时装载下一音符，实现非阻塞背景音乐播放。
void music_tick_1ms(void)
{
    if(music_started == 0) {
        return;
    }

    if(music_ms_left > 0) {
        music_ms_left--;
    }

    if(music_ms_left == 0) {
        music_load_next_note();
    }
}

// 主游戏循环：
//   1) 根据难度设置的 time 值控制图案下落速度；
//   2) 每轮调用 reflash 让图案整体下移一行；
//   3) 在一段时间内不断刷新点阵显示；
//   4) 调用 check 判断按键是否正确，答对继续、答错结束；
//   5) 结束后调用 note_code，将得分显示在数码管上。
void run_game(void)
{
    int j, t, time;
    my_code = 0;

    get_seed_mod();
    srand(seed);

    switch(mod) {
    case 1: time = 50; break;
    case 2: time = 30; break;
    case 3: time = 20; break;
    case 4: time = 10; break;
    default: time = 30; break;
    }

    while(1) {
        t = time;
        reflash();

        while(t--) {
            for(j=0; j<8; j++) {
                hc595(ghc595[j]);
                DZLED_SMG_PORT = new_arrange[j];
                Delayms(1);
            }
        }

        check();
        BUZZERRING();
        if(flag == 0) {
            break;
        }
    }

    note_code(my_code);
    while(1) {
        for(j=0; j<sGSMG; j++) {
            GSMG(j);
            DZLED_SMG_PORT = new_gsmg_code[j];
            Delayms(1);
        }
    }
}

// 主函数：
//   1) 初始化蜂鸣器和定时器0；
//   2) 启动背景音乐；
//   3) 进入游戏流程，音乐与游戏并行运行。
void main(void)
{
    BUZZER = 0;
    music_timer0_init();
    music_start();

    run_game();
}
