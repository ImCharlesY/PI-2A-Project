﻿///CPU时钟: DCO=8MHz, 定时器0、定时器1时钟: SMCLK=1MHz.
//音乐输出引脚：P2.1

#include  <msp430g2553.h>
#include "tm1638.h"		//tm1638.h与本文件放在同一路径下
#include "Music_Scores.h"



///////////////////////////////
//			Constant         //
///////////////////////////////

//模拟开关控制量定义
#define CTL0_L P1OUT&=~BIT0;
#define CTL0_H P1OUT|=BIT0;
#define CTL1_L P1OUT&=~BIT1;
#define CTL1_H P1OUT|=BIT1;
#define CTL2_L P1OUT&=~BIT2;
#define CTL2_H P1OUT|=BIT2;
#define CTL3_L P1OUT&=~BIT3;
#define CTL3_H P1OUT|=BIT3;

//0.1s软件定时器溢出值,5个20ms
#define V_T100ms 5
//0.5s定时器溢出值,25个20ms
#define V_T500ms 25
//增益等级总数
#define GAIN_STATENUM 15
//音调等级总数
#define TONE_STATENUM 3
//播放速度等级总数
#define SPEED_STATENUM 5
//乐谱总数
#define MUSIC_NUM 3



////////////////////////////////
//			Variables         //
////////////////////////////////

//软件定时器计数
unsigned char clock100ms=0;
unsigned char clock500ms=0;

//软件定时器溢出标志
unsigned char clock100ms_flag=0;
unsigned char clock500ms_flag=0;

//数码管显示值 G-[增益等级[1-15]]|F[音调等级[2-4]]R[播放速度等级[1-5]]
unsigned char digi[8]={'F',' ','R',' ','G','-',' ',' '};
//8位小数点,1on0off
//注：板上数码位小数点从左到右序号排列为4/5/6/7/0/1/2/3
unsigned char pnt=0x00;
//8个LED灯状态,每个灯4种颜色变化,0灭,1绿,2红,3橙
//注：板上指示灯从左到右序号排列为7/6/5/4/3/2/1/0
//对应元件LED(序号+1)
unsigned char led[]={0,0,0,0,0,0,0,0};
//与按键操作有关的全局变量
unsigned char key_state=0, key_flag=1, key_code=0;
//增益等级取值,初值为1,对应增益0.1
unsigned char gain_state=1;

/* 播放乐曲功能变量 */
// 乐谱指针
const unsigned int (*music_ptr)[2];

// 播放中,当前的音频频率
unsigned int audio_frequency;
// 辅助读谱指针、持续时间计数变量
unsigned int audio_ptr=0,audio_dura=0;
//音调等级,[1-5],初值为3
unsigned char tone_state=3;
//播放速度等级,[1-5],对应0.25,0.5，1.0，2,4.0,初值为3
unsigned char speed_state=3;
//当前乐谱编号，[0-？]
unsigned char music_num=0;

//播放速度对照表
const double speed_percent[5]={4.0, 2.0, 1.0, 0.5, 0.25};

//音调频率对照表
extern const unsigned int tone[][5];

//乐谱
//荷塘月色-C大调版
extern const unsigned int music_data0[][2];
//自新大陆-C大调版
extern const unsigned int music_data1[][2];
//告白气球-C大调版
extern const unsigned int music_data2[][2];



///////////////////////////////
//			Function         //
///////////////////////////////

//增益调节函数
void gain_control(void)
{
	switch (gain_state)
	{
	case 1:CTL3_L;CTL2_L;CTL1_L;CTL0_H;break;
	case 2:CTL3_L;CTL2_L;CTL1_H;CTL0_L;break;
	case 3:CTL3_L;CTL2_L;CTL1_H;CTL0_H;break;
	case 4:CTL3_L;CTL2_H;CTL1_L;CTL0_L;break;
	case 5:CTL3_L;CTL2_H;CTL1_L;CTL0_H;break;
	case 6:CTL3_L;CTL2_H;CTL1_H;CTL0_L;break;
	case 7:CTL3_L;CTL2_H;CTL1_H;CTL0_H;break;
	case 8:CTL3_H;CTL2_L;CTL1_L;CTL0_L;break;
	case 9:CTL3_H;CTL2_L;CTL1_L;CTL0_H;break;
	case 10:CTL3_H;CTL2_L;CTL1_H;CTL0_L;break;
	case 11:CTL3_H;CTL2_L;CTL1_H;CTL0_H;break;
	case 12:CTL3_H;CTL2_H;CTL1_L;CTL0_L;break;
	case 13:CTL3_H;CTL2_H;CTL1_L;CTL0_H;break;
	case 14:CTL3_H;CTL2_H;CTL1_H;CTL0_L;break;
	case 15:CTL3_H;CTL2_H;CTL1_H;CTL0_H;break;
	}
}

//按键操作在时钟中断服务程序种的状态转移处理程序
void SW_key_state()
{
	switch(key_state)
	{
	case 0:
		if (key_code>0)
			{ key_state=1; key_flag=1; }
		break;
	case 1:
		if (key_code==0)
			{ key_state=0; }
		break;
	default:
		key_state=0; break;
	}
}

//乐曲播放函数
void play_music()
{
	/* 乐曲(循环)播放 读谱和计时*/
	if (audio_dura==0)
	{
		//定时器1暂停
		TA1CTL = 0;
		if (music_ptr[audio_ptr][1]==0) //判是否终止
		{
			/*乐曲终止*/
			audio_ptr=0;
			audio_dura=0;
		}
		else
		{
			audio_dura=music_ptr[audio_ptr][1]/5*speed_percent[speed_state-1]; //读节拍,除法用于调整节奏快慢
			if (music_ptr[audio_ptr][0]!=0) //判休止符
			{
				/*不是休止符*/
				//根据音频计算定时器A1的初值,并启动定时器A1
				audio_frequency=tone[music_ptr[audio_ptr][0]%10-1][music_ptr[audio_ptr][0]/10-2+tone_state-1];
				TA1CCR0 = 1000000/audio_frequency;	//设定周期
				TA1CCR1 = TA1CCR0/2;                //设置占空比等于50%
				TA1CTL = TASSEL_2 + MC_1 ;          // Source: SMCLK=1MHz, PWM mode,
			}
			audio_ptr++;
		}
	}
	else
		audio_dura--;
}



//////////////////////////////
//			  ISR           //
//////////////////////////////

// Timer0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void)
{
	//0.1s软定时器计数
	if (++clock100ms>=V_T100ms)
	{
		clock100ms_flag = 1; 	//当0.1秒到时,溢出标志置1
		clock100ms = 0;
	}

	//0.5s软定时器计数
	if (++clock500ms>=V_T500ms)
	{
		clock500ms_flag = 1; 	//当0.5秒到时,溢出标志置1
		clock500ms = 0;
	}

	//刷新全部数码管和LED指示灯
	TM1638_RefreshDIGIandLED(digi,pnt,led);

	//检查键盘输入,0无操作,1-16表示有对应按键
	key_code=TM1638_Readkeyboard();

	//按键状态转移
	SW_key_state();

	//播放乐曲
	play_music();
}



/////////////////////////////////////
//			Initialization         //
/////////////////////////////////////

//I/O端口初始化
void Port_Init(void)
{
    P2SEL &= ~(BIT7+BIT6);       //P2.6、P2.7 设置为通用I/O端口
    P2DIR |= BIT7 + BIT6 + BIT5; //P2.5、P2.6、P2.7 设置为输出

	//P2.1 设置为定时器1_ A1的TA1.1　PWM输出
    P2SEL |= BIT1;
    P2DIR |= BIT1;

    //P1.0-1.3作为模拟开关输入
    P1DIR |= BIT0 + BIT1 + BIT2 + BIT3;
}

//TIMER0 initialize -
// desired value: 5ms
void Timer0_Init(void)
{
	// Configure Timer0
	TA0CTL = TASSEL_2 + MC_1 ;      // Source: SMCLK=1MHz, UP mode,
	TA0CCR0 = 5000;                 // 1MHz时钟,计满5000次为 5 毫秒
	CCTL0 = CCIE;                  	// CCR0 interrupt enabled
}

//TIMER1 initialize -
// desired value: 440Hz,P2.1管脚  PWM 输出 标准音"啦"
void Timer1_init(void)
{
	// Configure Timer1
    TA1CTL = TASSEL_2 + MC_1 ;          // Source: SMCLK=1MHz, PWM mode,
	TA1CCTL1 = OUTMOD_7;
	TA1CCR0 = 1000000/440;					//设定周期
	TA1CCR1 = TA1CCR0/2;					//设置占空比等于50%

}

void Devices_Init(void)
{
	WDTCTL = WDTPW + WDTHOLD;     // Stop watchdog timer
	if (CALBC1_8MHZ ==0xFF || CALDCO_8MHZ == 0xFF)
	{
		while(1);            // If calibration constants erased, trap CPU!!
	}

    //设置时钟,内部RC振荡器。     DCO：8MHz,供CPU时钟;  SMCLK：1MHz,供定时器时钟
	BCSCTL1 = CALBC1_8MHZ; 	 // Set range
	DCOCTL = CALDCO_8MHZ;    // Set DCO step + modulation
	BCSCTL3 |= LFXT1S_2;     // LFXT1 = VLO
	IFG1 &= ~OFIFG;          // Clear OSCFault flag
	BCSCTL2 |= DIVS_3;       //  SMCLK = DCO/8

    Port_Init();             //初始化I/O口
    Timer0_Init();          //初始化定时器0
    Timer1_init();          //初始化定时器1
    _BIS_SR(GIE);            //开全局中断
   //all peripherals are now initialized
}



///////////////////////////
//			Main         //
///////////////////////////

void main(void)
{
	Devices_Init( );
	while(clock100ms<3);	//延时60ms等待TM1638上电
	TM1638_Init();
	gain_control();

	// 初始乐谱
	music_ptr=music_data0;
	// 点亮对应二极管
	// Wait for edition

	while(1)
	{
		if (key_flag==1)
		{
			key_flag=0;
			switch (key_code)
			{
			// 增大增益
			case 1:
				if (++gain_state>GAIN_STATENUM) gain_state=1;
				gain_control();
				break;
			// 减小增益
			case 2:
				if (--gain_state==0) gain_state=GAIN_STATENUM;
				gain_control();
				break;
			// 提高音调
			case 3:
				if (++tone_state>TONE_STATENUM+1) tone_state=2;
				break;
			// 降低音调
			case 4:
				if (--tone_state==1) tone_state=TONE_STATENUM+1;
				break;
			// 提高播放速度
			case 5:
				if (++speed_state>SPEED_STATENUM) speed_state=1;
				break;
			// 降低播放速度
			case 6:
				if (--speed_state==0) speed_state=SPEED_STATENUM;
				break;
			// 切换歌曲（下一首）
			case 15:
				if (++music_num >= MUSIC_NUM) music_num=0;
				switch(music_num)
				{
				case 0: music_ptr=music_data0; audio_dura=0; audio_ptr=0; audio_frequency=0; break;
				case 1: music_ptr=music_data1; audio_dura=0; audio_ptr=0; audio_frequency=0; break;
				case 2: music_ptr=music_data2; audio_dura=0; audio_ptr=0; audio_frequency=0; break;
				default: break;
				}
			default: break;
			}
		}
		//更新数码管显示
		digi[1]=tone_state;
		digi[3]=speed_state;
		digi[6]=gain_state/10;
		digi[7]=gain_state%10;
	}
}