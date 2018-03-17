#ifndef _TM1638_H
#define _TM1638_H

#include "msp430g2553.h"

//用于连接TM1638的单片机引脚输出操作定义
#define DIO_L	P2OUT&=~BIT5		//P2.5=0
#define DIO_H	P2OUT|=BIT5			//P2.5=1
#define CLK_L	P2OUT&=~BIT7		//P2.7=0
#define CLK_H	P2OUT|=BIT7		    //P2.7=1
#define STB_L	P2OUT&=~BIT6		//P2.6=0
#define STB_H	P2OUT|=BIT6		    //P2.6=1
#define DIO_IN	P2DIR&=~BIT5		//P2.5 set as Input
#define DIO_OUT	P2DIR&=~BIT5		//P2.5 set as Output
#define DIO_DATA_IN	P2IN&BIT5	

//将显示数字或符号转换位共阴数码管的笔画值
unsigned char TM1638_DigiSegment(unsigned char digit)
{
	unsigned char segment=0;
	switch(digit)
	{
	case 0:segment=0x3F;break;
	case 1:segment=0x06;break;
	case 2:segment=0x5B;break;
	case 3:segment=0x4F;break;
	case 4:segment=0x66;break;
	case 5:segment=0x6D;break;
	case 6:segment=0x7D;break;
	case 7:segment=0x07;break;
	case 8:segment=0x7F;break;
	case 9:segment=0x6F;break;
	case 10:segment=0x77;break;
	case 11:segment=0x7C;break;
	case 12:segment=0x39;break;
	case 13:segment=0x5E;break;
	case 14:segment=0x79;break;
	case 15:segment=0x71;break;
	case '_':segment=0x08;break;
	case '-':segment=0x40;break;
	case ' ':segment=0x00;break;
	case 'G':segment=0x3D;break;
	case 'A':segment=0x77;break;
	case 'I':segment=0x06;break;
	case 'N':segment=0x37;break;
	case 'F':segment=0x71;break;
	case 'U':segment=0x3E;break;
	case 'L':segment=0x38;break;
	case 'R':segment=0x50;break;
	case 'E':segment=0x79;break;
	case 'D':segment=0x5E;break;
	case 'Y':segment=0x6E;break;
	default:segment=0x00;break;
	}

	return segment;
}	

//TM1638串行数据输入
void TM1638_Serial_Input(unsigned char DATA)
{
	unsigned char i;
	DIO_OUT;
	for (i=0;i<8;++i)
	{
		CLK_L;
		if (DATA&0x01)
			DIO_H;
		else
			DIO_L;
		DATA>>=1;
		CLK_H;
	}
}

//TM1638串行数据输出
unsigned char TM1638_Serial_Output(void)
{
	unsigned char i;
	unsigned char temp=0;
	DIO_IN;
	for (i=0;i<8;++i)
	{
		temp>>=1;
		CLK_L;
		CLK_H;
		if (DIO_DATA_IN)
			temp|=0x80;
		CLK_L;
	}
	return temp;
}

//读取键盘状态
unsigned char TM1638_Readkeyboard(void)
{
	unsigned char c[4],i,key_code=0;
	STB_L;
	TM1638_Serial_Input(0x42);		//读键扫数据命令
	_delay_cycles(10);
	for (i=0;i<4;++i)	c[i]=TM1638_Serial_Output();
	STB_H;
	
	//4个字节数据和合成一个字节
	if (c[0]==0x04) key_code=1;
	if (c[0]==0x40) key_code=2;
	if (c[1]==0x04) key_code=3;
	if (c[1]==0x40) key_code=4;
	if (c[2]==0x04) key_code=5;
	if (c[2]==0x40) key_code=6;
	if (c[3]==0x04) key_code=7;
	if (c[3]==0x40) key_code=8;
	if (c[0]==0x02) key_code=9;
	if (c[0]==0x20) key_code=10;
	if (c[1]==0x02) key_code=11;
	if (c[1]==0x20) key_code=12;
	if (c[2]==0x02) key_code=13;
	if (c[2]==0x20) key_code=14;
	if (c[3]==0x02) key_code=15;
	if (c[3]==0x20) key_code=16;

	return key_code;
}

//刷新8位数码管和8组指示灯
void TM1638_RefreshDIGIandLED(unsigned char digit_buf[8],unsigned char pnt_buf,unsigned char led_buf[8])
{
	unsigned char i,mask,buf[16];

	mask=0x01;

	for (i=0;i<8;++i)
	{
		//数码管
		buf[i<<1]=TM1638_DigiSegment(digit_buf[i]);
		if ((pnt_buf&mask)) buf[i<<1]!=0x80;
		mask<<=1;

		//指示灯
		buf[(i<<1)+1]=led_buf[i];
	}

	STB_L;TM1638_Serial_Input(0x40);STB_H;		//设置地址模式自动+1
	STB_L;
	TM1638_Serial_Input(0xC0);					//设置起始地址
	for (i=0;i<16;++i)
		TM1638_Serial_Input(buf[i]);
	STB_H;
}

//TM1638初始化函数
void TM1638_Init(void)
{
	STB_L;
	TM1638_Serial_Input(0x8A);
	STB_H;
}

#endif