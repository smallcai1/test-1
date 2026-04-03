//代码思路：在蜂鸣器播放《起风了》的基础上随机生成数组并滚动显示各种按键组合提示图案，玩家须在图案滚动到最低行时，按下对应的一个
//或两个按键(p3.0-p3.3).程序在每一轮根据按键状态进行判定：若与当前的图案要求一致，积分变量加一分
//并继续游戏；若错误或未按，则游戏结束。结束后单片机通过多位数码管显示难度等级和最总得分，实现音乐播放
//随机图案形成，按键交互等功能为一体的小游戏；

#include "reg52.h"
#include "intrins.h"//nop等内部函数
#include "stdlib.h"

typedef unsigned char u8;
typedef unsigned int u16;

#define DZLED_SMG_PORT P0 // 段选：数码管

sbit BUZZER = P2^5;  //蜂鸣器控制端
sbit LSA = P2^2;
sbit LSB = P2^3;
sbit LSC = P2^4;

sbit BUTTER2 = P3^0;  //四个按键：难度选择 + 游戏输入
sbit BUTTER1 = P3^1;
sbit BUTTER3 = P3^2;
sbit BUTTER4 = P3^3;

sbit SRCLK = P3^6;
sbit rCLK  = P3^5;
sbit SER   = P3^4;

//音乐播放参数
u8 m, n;
u16 music_index = 0;
u16 music_ms_left = 0;
u8 music_started = 0;


#define MUSIC_UNIT_MS 70
u8 code T[49][2] = {
    {0,0},
    {0xF8,0x8B},{0xF8,0xF2},{0xF9,0x5B},{0xF9,0xB7},{0xFA,0x14},{0xFA,0x66},{0xFA,0xB9},{0xFB,0x03},{0xFB,0x4A},{0xFB,0x8F},{0xFB,0xCF},{0xFC,0x0B},
    {0xFC,0x43},{0xFC,0x78},{0xFC,0xAB},{0xFC,0xDB},{0xFD,0x08},{0xFD,0x33},{0xFD,0x5B},{0xFD,0x81},{0xFD,0xA5},{0xFD,0xC7},{0xFD,0xE7},{0xFE,0x05},
    {0xFE,0x21},{0xFE,0x3C},{0xFE,0x55},{0xFE,0x6D},{0xFE,0x84},{0xFE,0x99},{0xFE,0xAD},{0xFE,0xC0},{0xFE,0x02},{0xFE,0xE3},{0xFE,0xF3},{0xFF,0x02},
    {0xFF,0x10},{0xFF,0x1D},{0xFF,0x2A},{0xFF,0x36},{0xFF,0x42},{0xFF,0x4C},{0xFF,0x56},{0xFF,0x60},{0xFF,0x69},{0xFF,0x71},{0xFF,0x79},{0xFF,0x81}
};

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

//游戏参数
int seed, my_code, r, sGSMG;  //随机种子，得分，随机索引，数码管位数
char flag, mod, key = 0;

// 扫描控制字
char ghc595[8] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
// 随机图案编码
char random_arrange[10] = {0x7f,0xdf,0xf7,0xfd,0x5f,0x77,0x7d,0xd7,0xdd,0xf5};
// 数码管段码
char gsmg_code[10] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
// 点阵当前八行图案
char new_arrange[8] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
// 数码管显示缓存
char new_gsmg_code[8] = {0xff,0x40,0xff,0xff,0xff,0xff,0xff,0xff};

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
            while(--j); // 约1ms的延时
        } while(--i);
        music_tick_1ms(); // 每一毫秒推进一次音乐节拍
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


// 选择难度模式并随机获得种子；
// 在等待按键过程中自增seed ，用作srand的种子
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
// 1 所有行整体下移一行
// 2 key 在0 - 1 间交替
// key == 0 : 在顶行生成新的随机图案；
// key == 1 : 在顶行填充空行 0xff 实现图案间空隙效果

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
// 根据最底行图案 new_arrange[7]判断是否正确
// 按对：flag = 1 ，my_code++ ;按错或无按键：flag = 0；
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
// 定时器0的初始化: 打开中断但不启动计数
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
// 定时器的暂停
void T0_int(void) interrupt 1
{
    BUZZER = !BUZZER;
    TH0 = T[m][0];
    TL0 = T[m][1];
}

// 装载下一音符
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

// 每1ms调用一次，递减当前音符剩余时间
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
//主函数 ： 
// 初始化蜂鸣器和定时器0；
// 启动游戏音乐
// 进入游戏流程，音乐与游戏一起运行
void main(void)
{
    BUZZER = 0;
    music_timer0_init();
    music_start();

    run_game();
}
