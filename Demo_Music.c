///CPU时钟: DCO=8MHz, 定时器0、定时器1时钟: SMCLK=1MHz.
//音乐输出引脚：P2.1

#include  <msp430g2553.h>
#include "tm1638.h"		//tm1638.h与本文件放在同一路径下

//////////////////////////////
//			常量定义         //
//////////////////////////////

#define CTL0_L P1OUT&=~BIT0;
#define CTL0_H P1OUT|=BIT0;
#define CTL1_L P1OUT&=~BIT1;
#define CTL1_H P1OUT|=BIT1;
#define CTL2_L P1OUT&=~BIT2;
#define CTL2_H P1OUT|=BIT2;
#define CTL3_L P1OUT&=~BIT3;
#define CTL3_H P1OUT|=BIT3;

// 1s软件定时器溢出值，200个5ms
#define V_T1s	200
//0.1s软件定时器溢出值，5个20ms
#define V_T100ms 5
//0.5s定时器溢出值，25个20ms
#define V_T500ms 25
//增益等级总数
#define GAIN_STATENUM 15


//////////////////////////////
//			变量定义         //
//////////////////////////////

//软件定时器计数
unsigned char clock100ms=0;
unsigned char clock500ms=0;
unsigned char clock1s=0;
//软件定时器溢出标志
unsigned char clock100ms_flag=0;
unsigned char clock500ms_flag=0;
unsigned char clock1s_flag=0;

// 测试用计数器
unsigned int test_counter=0;
// 测试用计数值十进制表示
unsigned char digi[8]={' ',' ',' ',' ','G','-',' ',' '};
//8位小数点，1on0off
//注：板上数码位小数点从左到右序号排列为4/5/6/7/0/1/2/3
unsigned char pnt=0x00;
//8个LED灯状态，每个灯4种颜色变化，0灭，1绿，2红，3橙
//注：板上指示灯从左到右序号排列为7/6/5/4/3/2/1/0
//对应元件LED(序号+1)
unsigned char led[]={0,0,0,0,0,0,0,0};
//与按键操作有关的全局变量
unsigned char key_state=0, key_flag=1, key_code=0;
//增益等级取值，初值为1，对应增益0.1
unsigned char gain_state=1;

//乐曲荷塘月色的乐谱 {频率值，节拍值}  const类型指明要存放在ROM中
const unsigned int music_data[][2]=
{
		{523,600},{784,200},{523,200},{784,200},{523,200},{587,200},{659,1600},
		{523,600},{784,200},{523,200},{784,200},{523,200},{587,200},{587,1600},
		{523,600},{784,200},{523,200},{784,200}, {587,200} ,{523,200},{440,1000},{392,200}, {523,200},{587,200},
		{523,600},{784,200},{523,200},{784,200},{523,200},{440,200},{523,1600},
		{523,200},{523,400},{440,200},{392,400},{440,400},
		{523,400},{523,200},{587,200},{659,800},
		{587,200},{587,400},{523,200},{587,400},{587,200},{784,200},
		{784,200},{659,200},{659,200},{587,200},{659,800},
		{523,200},{523,400},{440,200},{392,400},{784,400},
		{659,200},{587,200}, {659,200},{587,200},{523,800},
		{587,200}, {587,400}, {523,200}, {587,200}, {587,400},{659,200},
		{587,200},{523,200},{440,200}, {587,200},{523,800},
		{523,200},{523,400},{440,200},{392,400},{440,400},
		{523,200},{523,400},{587,200},{659,800},
		{587,200},{587,400},{523,200},{587,400},{587,200},{784,200},
		{784,200},{659,200},{659,200},{587,200},{659,800},
		{523,200},{523,200},{523,200},{440,200},{392,400},{784,400},
		{659,200},{587,200}, {659,200},{587,200},{523,800},
		{587,200},{587,400}, {523,200}, {587,200}, {587,400},{659,200},
		{587,200},{523,200},{440,200}, {587,200},{523,800},
		{659,200},{784,400}, {784,200}, {784,400}, {784,400},
		{880,200},{784,200},{659,200},{587,200},{523,800},
		{880,200},{1047,200},{880,200},{784,200},{659,200},{587,200},{523,200},{440,200},
		{587,400},{587,200},{659,200},{659,200},{587,600},
		{659,200},{784,400}, {784,200}, {784,400}, {784,400},
		{880,200},{784,200},{659,200},{587,200},{523,800},
		{440,200},{523,200},{440,200},{392,200},{587,400},{659,400},
		{523,1200},{0,400},
		{0,0}

};

/* 播放乐曲功能变量 */
// 播放中，当前的音频频率
unsigned int audio_frequency;
// 辅助读谱指针、持续时间计数变量
unsigned int audio_ptr=0,audio_dura=0;

///////////////////////////////////////////////
//	       	  Function SubProgrom  		     //
///////////////////////////////////////////////

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


//////////////////////////////
//			函数定义         //
//////////////////////////////


//  I/O端口初始化
void Port_Init(void)
{
	    P2SEL &= ~(BIT7+BIT6);       //P2.6、P2.7 设置为通用I/O端口
	    P2DIR |= BIT7 + BIT6 + BIT5; //P2.5、P2.6、P2.7 设置为输出

    	//P2.1 设置为定时器1_ A1的TA1.1　PWM输出
        P2SEL |= BIT1;
        P2DIR |= BIT1;

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

// Timer0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void)
{
	//0.1s软定时器计数
	if (++clock100ms>=V_T100ms)
	{
		clock100ms_flag = 1; 	//当0.1秒到时，溢出标志置1
		clock100ms = 0;
	}

	//0.5s软定时器计数
	if (++clock500ms>=V_T500ms)
	{
		clock500ms_flag = 1; 	//当0.5秒到时，溢出标志置1
		clock500ms = 0;
	}

 	// 1秒钟软定时器计数
	if (++clock1s>=V_T1s)
	{
		clock1s_flag = 1; //当1秒到时，溢出标志置1
		clock1s = 0;
	}

	//刷新全部数码管和LED指示灯
	TM1638_RefreshDIGIandLED(digi,pnt,led);

	//检查键盘输入，0无操作，1-16表示有对应按键
	//键号显示
	key_code=TM1638_Readkeyboard();

	//按键操作在时钟中断服务程序种的状态转移处理程序
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

	/* 乐曲(循环)播放 读谱和计时*/
	if (audio_dura==0)
	{
		//定时器1暂停
		TA1CTL = 0;
		if (music_data[audio_ptr][1]==0) //判是否终止
		{
			/*乐曲终止*/
			audio_ptr=0;
			audio_dura=0;
		}
		else
		{
			audio_dura=music_data[audio_ptr][1]/5; //读节拍，除法用于调整节奏快慢
			if (music_data[audio_ptr][0]!=0) //判休止符
			{
				/*不是休止符*/
				//根据音频计算定时器A1的初值，并启动定时器A1
				audio_frequency=music_data[audio_ptr][0];
				TA1CCR0 = 1000000/audio_frequency;	//设定周期
				TA1CCR1 = TA1CCR0/2;                //设置占空比等于50%
				TA1CTL = TASSEL_2 + MC_1 ;          // Source: SMCLK=1MHz, PWM mode,
			}
			else
			{
				/*是休止符*/
			}
			audio_ptr++;
		}
	}
	else
		audio_dura--;
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

void Init_Devices(void)
{
	WDTCTL = WDTPW + WDTHOLD;     // Stop watchdog timer
	if (CALBC1_8MHZ ==0xFF || CALDCO_8MHZ == 0xFF)
	{
		while(1);            // If calibration constants erased, trap CPU!!
	}

    //设置时钟，内部RC振荡器。     DCO：8MHz,供CPU时钟;  SMCLK：1MHz,供定时器时钟
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


void main(void)
{
	Init_Devices( );
	while(clock100ms<3);	//延时60ms等待TM1638上电
	TM1638_Init();
	gain_control();

	while(1)
	{
		if (clock1s_flag==1)   // 检查1秒定时是否到
		{
			clock1s_flag=0;
	       	// 每秒加1的数码管十进制显示计数
			if (++test_counter>=10000) test_counter=0;
			digi[0] = test_counter/1000;  	//计算千位数
			digi[1] = (test_counter-digi[0]*1000)/100; 	//计算百位数
			digi[2] = (test_counter-digi[0]*1000-digi[1]*100)/10; 	//计算十位数
			digi[3] = (test_counter-digi[0]*1000-digi[1]*100-digi[2]*10); 	//计算个位数
		}

		//按键操作改变增益
		if (key_flag==1)
		{
			key_flag=0;
			switch (key_code)
			{
			case 1:
				if (++gain_state>GAIN_STATENUM) gain_state=1;
				gain_control();
				break;
			case 2:
				if (--gain_state==0) gain_state=GAIN_STATENUM;
				gain_control();
				break;
			default: break;
			}
		}
		digi[6]=gain_state/10;
		digi[7]=gain_state%10;
	}
}
