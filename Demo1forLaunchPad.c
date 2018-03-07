//本程序时钟采用内部RC振荡器。DCO：8MHz，供CPU时钟；SMCLK：1MHz，供定时器时钟
#include <msp430g2553.h>
#include "tm1638.h"		//tm1638.h与本文件放在同一路径下

////////////////////////////////
//	  Constant Definition	  //
///////////////////////////////

//0.1s软件定时器溢出值，5个20ms
#define V_T100ms 5
//0.5s定时器溢出值，25个20ms
#define V_T500ms 25

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
unsigned char digit[8]={' ',' ',' ',' ','_',' ',' ','_'};
//8位小数点，1on0off
//注：板上数码位小数点从左到右序号排列为4/5/6/7/0/1/2/3
unsigned char pnt=0x04;
//8个LED灯状态，每个灯4种颜色变化，0灭，1绿，2红，3橙
//注：板上指示灯从左到右序号排列为7/6/5/4/3/2/1/0
//对应元件LED(序号+1)
unsigned char led[]={0,0,1,1,2,2,3,3};
//当前按键值
unsigned char key_code=0;

////////////////////////////////
//	    Initialization  	  //
///////////////////////////////

//  I/O端口和引脚初始化
void Ports_Init(void)
{
	P2SEL &= ~(BIT7+BIT6);       //P2.6、P2.7 设置为通用I/O端口
	P2DIR |= BIT7 + BIT6 + BIT5; //P2.5、P2.6、P2.7 设置为输出，此三者用于连接显示和键盘管理器TM1638
    
    //Wait to add...
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
	digit[6]=key_code%10;
	digit[5]=key_code/10;
}

////////////////////////////////
//	       	  main 		     //
///////////////////////////////

void main(void)
{
	Devices_Init();
	while(clock100ms<3);	//延时60ms等待TM1638上电
	TM1638_Init();

	unsigned char i=0, temp;

	while(1)
	{
		if (clock100ms_flag==1)   // 检查0.1秒定时是否到
		{
			clock100ms_flag=0;
	       	// 每0.1s累加计时值在数码管上以十进制显示，有按键时暂停计时
	       	if (key_code==0)
	       	{
	       		if (++test_counter>=10000) test_counter=0;
				digit[0] = test_counter/1000;  										//计算千位数
				digit[1] = (test_counter-digit[0]*1000)/100; 						//计算百位数
				digit[2] = (test_counter-digit[0]*1000-digit[1]*100)/10; 			//计算十位数
				digit[3] = (test_counter-digit[0]*1000-digit[1]*100-digit[2]*10); 	//计算个位数
	       	}
		}

		if (clock500ms_flag==1)   // 检查0.5秒定时是否到
		{
			clock500ms_flag=0;
	       	// 8个指示灯以走马灯方式，每0.5s右移一位
	       	temp=led[0];
	       	for (i=0;i<7;++i) led[i]=led[i+1];
	       	led[7]=temp;
		}
	}
}
