//本程序时钟采用内部RC振荡器。DCO：8MHz，供CPU时钟；SMCLK：1MHz，供定时器时钟
#include <msp430g2553.h>
#include "tm1638.h"		//tm1638.h与本文件放在同一路径下

////////////////////////////////
//	  Constant Definition	  //
///////////////////////////////

#define CTL0_L P1OUT&=~BIT0;
#define CTL0_H P1OUT|=BIT0;
#define CTL1_L P1OUT&=~BIT1;
#define CTL1_H P1OUT|=BIT1;
#define CTL2_L P1OUT&=~BIT2;
#define CTL2_H P1OUT|=BIT2;
#define CTL3_L P1OUT&=~BIT3;
#define CTL3_H P1OUT|=BIT3;


//0.1s软件定时器溢出值，5个20ms
#define V_T100ms 5
//0.5s定时器溢出值，25个20ms
#define V_T500ms 25
//增益等级总数
#define GAIN_STATENUM 15

////////////////////////////////
//	  Variable Definition	  //
///////////////////////////////

//软件定时器计数
unsigned char clock100ms=0;
unsigned char clock500ms=0;
//软件定时器溢出标志
unsigned char clock100ms_flag=0;
unsigned char clock500ms_flag=0;
//测试用计数器
unsigned int test_counter=0;
//8位数码管显示的数字或字母符号
//注：板上数码位从左到右序号排列为4/5/6/7/0/1/2/3
unsigned char digit[8]={' ','-',' ',' ','G','A','I','N'};
//8位小数点，1on0off
//注：板上数码位小数点从左到右序号排列为4/5/6/7/0/1/2/3
unsigned char pnt=0x04;
//8个LED灯状态，每个灯4种颜色变化，0灭，1绿，2红，3橙
//注：板上指示灯从左到右序号排列为7/6/5/4/3/2/1/0
//对应元件LED(序号+1)
unsigned char led[]={0,0,0,0,0,0,0,0};
//与按键操作有关的全局变量
unsigned char key_state=0, key_flag=1, key_code=0;
//增益等级取值，初值为1，对应增益0.1
unsigned char gain_state=1;

////////////////////////////////
//	    Initialization  	  //
///////////////////////////////

//  I/O端口和引脚初始化
void Ports_Init(void)
{
	P2SEL &= ~(BIT7+BIT6);       //P2.6、P2.7 设置为通用I/O端口
	P2DIR |= BIT7 + BIT6 + BIT5; //P2.5、P2.6、P2.7 设置为输出，此三者用于连接显示和键盘管理器TM1638
    
    P1DIR |= BIT0 + BIT1 + BIT2 + BIT3;
}

//定时器TIMER0初始化，循环定时20ms
void Timer0_Init(void)
{
	TA0CTL = TASSEL_2 + MC_1 ;      // Source: SMCLK=1MHz, UP mode,
	TA0CCR0 = 5000;                 // 1MHz时钟,计满5000次为 5 毫秒
	TA0CCTL0 = CCIE;                  	// CCR0 interrupt enabled
}

//MCU器件初始化
void Devices_Init(void)
{
	WDTCTL = WDTPW + WDTHOLD;     // Stop watchdog timer
	if (CALBC1_8MHZ ==0xFF || CALDCO_8MHZ == 0xFF)
	{
		while(1);            	 // If calibration constants erased, trap CPU!!
	}

    //设置时钟，内部RC振荡器。     DCO：8MHz,供CPU时钟;  SMCLK：1MHz,供定时器时钟
	BCSCTL1 = CALBC1_8MHZ; 	 	// Set range
	DCOCTL = CALDCO_8MHZ;    	// Set DCO step + modulation
	BCSCTL3 |= LFXT1S_2;     	// LFXT1 = VLO
	IFG1 &= ~OFIFG;          	// Clear OSCFault flag
	BCSCTL2 |= DIVS_3;       	//  SMCLK = DCO/8

    Ports_Init();             	//初始化I/O口
    Timer0_Init();          	//初始化定时器0
    _BIS_SR(GIE);            	//开全局中断
    //all peripherals are now initialized
}


////////////////////////////////
//	       	  Function SubProgrom  		     //
///////////////////////////////

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



////////////////////////////////
//	       	  ISR  		     //
///////////////////////////////

// Timer0 ISR
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

	//刷新全部数码管和LED指示灯
	TM1638_RefreshDIGIandLED(digit,pnt,led);

	//检查键盘输入，0无操作，1-16表示有对应按键
	//键号显示
	key_code=TM1638_Readkeyboard();

	//按键操作在时钟中断服务程序种的状态转移处理程序
	switch(key_state)
	{
	case 0:
		if (key_code>0)
			{ key_state=1; key_flag=1; }
		break；
	case 1:
		if (key_code==0)
			{ key_state=0; }
		break；
	default:
		key_state=0; break;
	}
}

////////////////////////////////
//	       	  main 		     //
///////////////////////////////

void main(void)
{
	Devices_Init();
	while(clock100ms<3);	//延时60ms等待TM1638上电
	TM1638_Init();
	gain_control();

	unsigned char i=0, temp;

	while(1)
	{
		//按键操作在main主程序中的处理程序
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
		digit[2]=gain_state/10;
		digit[3]=gain_state%10;
	}
}
