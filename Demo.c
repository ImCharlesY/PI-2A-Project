///CPU时钟: DCO=8MHz, 定时器0、定时器1时钟: SMCLK=1MHz.
//模拟开关输出：P1.0-1.3
//音乐输出引脚：P2.2
//ADC输入引脚：P1.4
//红外检测引脚：P1.5
//波形发生输出：P1.6-1.7 P2.0-2.1

#include  <msp430g2553.h>
#include "tm1638.h"
#include "Table.h"

///////////////////////////////
//          Constant         //
///////////////////////////////

//红外遥感宏定义
#define INFRARED  (P1IN&BIT5)

//0.1s软件定时器溢出值,20个5ms
#define V_T100ms 20
//增益等级总数
#define GAIN_STATENUM 15
//音调等级总数
#define TONE_STATENUM 3
//播放速度等级总数
#define SPEED_STATENUM 5
//乐谱总数
#define MUSIC_NUM 4
//自动增益阈值
#define VMAX 1.5
#define VMIN 0.4
//红外遥感脉宽阈值
#define PULSEWIDTHLOW 2
#define PULSEWIDTHHIGH 15
//波形发生函数表长度
#define WAVELENGTH 100


////////////////////////////////
//          Variables         //
////////////////////////////////

//增益控制字表格
extern const unsigned char mask[64];

//软件定时器计数
unsigned char clock100ms=0;
//软件定时器溢出标志
unsigned char clock100ms_flag=0;

//数码管显示值 G-[增益等级[1-15]]|F[音调等级[2-4]]R[播放速度等级[1-5]]
unsigned char digi[8]={'F',' ','R',' ','G','-',' ',' '};
//8位小数点,1on0off；注：板上数码位小数点从左到右序号排列为4/5/6/7/0/1/2/3
unsigned char pnt=0x40;
//8个LED灯状态,每个灯4种颜色变化,0灭,1绿,2红,3橙；注：板上指示灯从左到右序号排列为7/6/5/4/3/2/1/0；对应元件LED(序号+1)
unsigned char led[]={0,0,0,0,0,0,0,0};

//与按键操作有关的全局变量
unsigned char key_state=0, key_flag=1, key_code=0;

//增益等级取值,初值为1,对应增益0.1
unsigned char gain_state=1;

//开关变量：音乐开关，自动增益开关，红外遥感开关，波形发生开关
unsigned char music_ctrl=0, auto_ctrl=0, infrared_ctrl=0, wavegen_ctrl=0;


/* 播放乐曲功能变量 */
const unsigned int (*music_ptr)[2];		// 乐谱指针
const unsigned int (*tone_ptr)[5];		// 频率表指针
unsigned char tone_decode=10;			// 频率表解码，C大调取10，12平均律取100
unsigned int audio_frequency;			// 播放中,当前的音频频率
unsigned int audio_ptr=0,audio_dura=0;	// 辅助读谱指针、持续时间计数变量
unsigned char tone_state=3;				// 音调等级,[1-5],初值为3
unsigned char speed_state=3;			// 播放速度等级,[1-5],对应0.25,0.5，1.0，2,4.0,初值为3
unsigned char music_num=0;				// 当前乐谱编号，[0-3]
//播放速度对照表
const double speed_percent[5]={4.0, 2.0, 1.0, 0.5, 0.25};
//音调频率对照表及乐谱
extern const unsigned int tone1[][5];   // C大调
extern const unsigned int tone2[][5];   // 12平均律
extern const unsigned int music_data0[][2];
extern const unsigned int music_data1[][2];
extern const unsigned int music_data2[][2];
extern const unsigned int music_data3[][2];


/* 自动增益功能变量 */
int sample;					// ADC模块单词转换值
double volt_sample[10]={0}; // 滤波数组
double volt;				// 当前电平值


/* 红外遥感功能变量 */
unsigned int infrared_state=0, infrared_pulsewidth=0, infrared_flag=0;


/* 波形发生功能变量 */ 
const unsigned char *wave_ptr;	// 增益序列指针
unsigned char wave_type=0;		// 当前波形编号
unsigned char wave_idx = 0;		// 辅助读表指针
//波形发生增益序列
extern const unsigned char squarewave[WAVELENGTH];
extern const unsigned char triangwave[WAVELENGTH];
extern const unsigned char sawtoowave[WAVELENGTH];
extern const unsigned char sincoswave[WAVELENGTH];


///////////////////////////////
//          Function         //
///////////////////////////////

//增益调节函数
void gain_control(void)
{
    P1OUT = (!wavegen_ctrl && gain_state>15)?mask[15]:mask[gain_state];
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
                audio_frequency=tone_ptr[music_ptr[audio_ptr][0]%tone_decode-1][music_ptr[audio_ptr][0]/tone_decode-2+tone_state-1];

                TA1CCR0 = 1000000/audio_frequency;  //设定周期
                TA1CCR1 = TA1CCR0/2;                //设置占空比等于50%
                TA1CTL = TASSEL_2 + MC_1 ;          // Source: SMCLK=1MHz, PWM mode,
            }
            audio_ptr++;
        }
    }
    else
        audio_dura--;
}

//自动增益控制程序
void auto_control()
{
    ADC10CTL0 |= ENC + ADC10SC;
    while (ADC10CTL1 & ADC10BUSY);
    sample = ADC10MEM;
    int i;
    for (i=0; i<10; ++i) volt_sample[i] = volt_sample[i + 1];
    volt = volt_sample[9] = sample*2.5/1024;
	
	for (i=0; i<10; ++i) volt = (volt<volt_sample[i])?volt_sample[i]:volt;

    if(volt > VMAX && gain_state >1)
    {
        --gain_state;
        gain_control();
    }
    if(volt < VMIN && gain_state <15)
    {
        ++gain_state;
        gain_control();
    }

    // ADC10CTL0 &=~ ENC;
}

//红外遥感状态机检测函数
void infrared()
{
	switch(infrared_state)
	{
	case 0:
		if (INFRARED!=0)
			{if (++infrared_pulsewidth>=PULSEWIDTHLOW)  infrared_state=1;}
		else
			infrared_pulsewidth=0;
		break;
	case 1:
		if (INFRARED!=0)
			{if (++infrared_pulsewidth>=PULSEWIDTHHIGH)  infrared_state=2;}
		else
		{
			infrared_flag=1;
			infrared_pulsewidth=0;
			infrared_state=0;
		}
		break;
	case 2:
		if (INFRARED==0)
		{
			infrared_flag=2;
			infrared_pulsewidth=0;
			infrared_state=0;
		}
		break;
	default:
		infrared_state=0;
		infrared_pulsewidth=0;
		break;
	}
	if (INFRARED==0) led[5]=1;
	else led[5]=2;
}

//波形发生
void wave_generator()
{
    if (++wave_idx == WAVELENGTH) wave_idx = 0;
    gain_state = wave_ptr[wave_idx];
    gain_control();
}

//刷新数码管和LED
void RefreshDIGITandLEDS(void)
{
    if (wavegen_ctrl)
    {
        pnt=0x00;
        digi[4]='G';
        digi[5]='E';
        digi[6]='N';
        digi[7]=digi[0]=digi[1]=digi[2]=digi[3]='-';
         //显示当前波形
        switch(wave_type)
        {
        case 0: led[0]=1; led[1]=led[2]=led[3]=2; break;
        case 1: led[1]=1; led[0]=led[2]=led[3]=2; break;
        case 2: led[2]=1; led[0]=led[1]=led[3]=2; break;
        case 3: led[3]=1; led[0]=led[1]=led[2]=2; break;
        default: break;
        }
        led[7]=led[6]=led[5]=2;
        led[4]=1;
    }
    else 
    {
        digi[4]='G';
        digi[5]='-';
        if (auto_ctrl)
        {
            if (clock100ms_flag)
            {
                clock100ms_flag = 0;
                int volt_display=volt*100;
                digi[0]='U';
                digi[1]=volt_display/100;
                digi[2]=volt_display%100/10;
                digi[3]=volt_display%10;
                pnt=0x42;
            }
        }
        else
        {
            digi[0]='F';
            digi[1]=tone_state-2;
            digi[2]='R';
            digi[3]=speed_state;
            pnt=0x40;
        }
            
        //显示增益值
        digi[6]=gain_state/10;
        digi[7]=gain_state%10;

        //显示当前乐谱
        switch(music_num)
        {
        case 0: led[0]=1; led[1]=led[2]=led[3]=2; break;
        case 1: led[1]=1; led[0]=led[2]=led[3]=2; break;
        case 2: led[2]=1; led[0]=led[1]=led[3]=2; break;
        case 3: led[3]=1; led[0]=led[1]=led[2]=2; break;
        default: break;
        }

        //更新二极管，指示功能
        led[7]=music_ctrl;
        led[6]=auto_ctrl;
        led[4]=wavegen_ctrl;
        led[5]=(infrared_ctrl)?led[5]:0;
    }
}


//////////////////////////////
//            ISR           //
//////////////////////////////

// Timer0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void)
{
    //0.1s软定时器计数
    if (++clock100ms>=V_T100ms)
    {
        clock100ms_flag = 1;    //当0.1秒到时,溢出标志置1
        clock100ms = 0;
    }

    //刷新全部数码管和LED指示灯
    TM1638_RefreshDIGIandLED(digi,pnt,led);

    //检查键盘输入,0无操作,1-16表示有对应按键
    key_code=TM1638_Readkeyboard();

    //按键状态转移
    SW_key_state();

    //播放乐曲
    if (music_ctrl) play_music();

    //自动增益
    if(auto_ctrl) auto_control();

    //红外遥感
    if(infrared_ctrl) infrared();

    //幅度调制波形发生
    if(wavegen_ctrl) wave_generator();
}


/////////////////////////////////////
//          Initialization         //
/////////////////////////////////////

//I/O端口初始化
void Port_Init(void)
{
    P2SEL &= ~(BIT7+BIT6);       //P2.6、P2.7 设置为通用I/O端口
    P2DIR |= BIT7 + BIT6 + BIT5; //P2.5、P2.6、P2.7 设置为输出

    //P1.0-1.3作为模拟开关输出, P1.6-1.7, P2.0-2.1作为波形发生输出
    P1DIR |= BIT0 + BIT1 + BIT2 + BIT3 + BIT6 + BIT7;
    P2DIR |= BIT0 + BIT1;

    //P2.2 设置为定时器1_ A1的TA1.1　PWM输出
    P2SEL |= BIT2; P2DIR |= BIT2;

    //P1.4作为检波电路输入
    P1DIR &=~ BIT4;

    //P1.5作为红外检测电路输入
    P1DIR &=~ BIT5;
}

//ADC初始化
void ADC10_Init(void)
{
	ADC10CTL0 &=~ ENC;
    ADC10CTL0 = SREF_1 + ADC10SHT_2 + REFON + REF2_5V+ ADC10ON;
    ADC10CTL1 = INCH_4 ;
    ADC10AE0 |= BIT4;
    ADC10CTL0 |= ENC;
}

//TIMER0 initialize -
// desired value: 5ms
void Timer0_Init(void)
{
    // Configure Timer0
    TA0CTL = TASSEL_2 + MC_1 ;      // Source: SMCLK=1MHz, UP mode,
    TA0CCR0 = 5000;                 // 1MHz时钟,计满5000次为 5 毫秒
    CCTL0 = CCIE;                   // CCR0 interrupt enabled
}

//TIMER1 initialize -
// desired value: 440Hz,P2.2管脚  PWM 输出 标准音"啦"
void Timer1_init(void)
{
    // Configure Timer1
    // TA1CTL = TASSEL_2 + MC_1 ;          // Source: SMCLK=1MHz, PWM mode,
    TA1CCTL1 = OUTMOD_7;
    TA1CCR0 = 1000000/440;                  //设定周期
    TA1CCR1 = TA1CCR0/2;                    //设置占空比等于50%

}

void Devices_Init(void)
{
    WDTCTL = WDTPW + WDTHOLD;     // Stop watchdog timer
    if (CALBC1_8MHZ ==0xFF || CALDCO_8MHZ == 0xFF)
    {
        while(1);            // If calibration constants erased, trap CPU!!
    }

    //设置时钟,内部RC振荡器。     DCO：8MHz,供CPU时钟;  SMCLK：1MHz,供定时器时钟
    BCSCTL1 = CALBC1_8MHZ;   // Set range
    DCOCTL = CALDCO_8MHZ;    // Set DCO step + modulation
    BCSCTL3 |= LFXT1S_2;     // LFXT1 = VLO
    IFG1 &= ~OFIFG;          // Clear OSCFault flag
    BCSCTL2 |= DIVS_3;       //  SMCLK = DCO/8

    Port_Init();             //初始化I/O口
    Timer0_Init();          //初始化定时器0
    Timer1_init();          //初始化定时器1
    ADC10_Init();           //初始化ADC
    _BIS_SR(GIE);            //开全局中断
   //all peripherals are now initialized
}

///////////////////////////
//          Main         //
///////////////////////////

void main(void)
{
    Devices_Init( );
    while(clock100ms<3);    //延时60ms等待TM1638上电
    TM1638_Init();
    gain_control();

    // 初始乐谱
    music_ptr=music_data0; tone_ptr=tone1;
    wave_ptr=squarewave;

    while(1)
    {
    	if (infrared_flag!=0)
		{
			switch(infrared_flag)
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
			infrared_flag=0;
		}
        if (key_flag==1)
        {
            key_flag=0;
            switch (key_code)
            {
            // 减小增益
            case 1:
                if (--gain_state==0) gain_state=GAIN_STATENUM;
                gain_control();
                break;
            // 增大增益
            case 2:
                if (++gain_state>GAIN_STATENUM) gain_state=1;
                gain_control();
                break;
            // 降低音调
            case 3:
                if (--tone_state==1) tone_state=TONE_STATENUM+1;
                break;
            // 提高音调
            case 4:
                if (++tone_state>TONE_STATENUM+1) tone_state=2;
                break;
            // 降低播放速度
            case 5:
                if (--speed_state==0) speed_state=SPEED_STATENUM;
                break;
            // 提高播放速度
            case 6:
                if (++speed_state>SPEED_STATENUM) speed_state=1;
                break;
            //波形发生
            case 9:
                wavegen_ctrl^=1;
                if (!wavegen_ctrl) gain_state=1, TA0CCR0 = 5000;	// 如果是关闭波形发生，将增益调制0.1，并重置TA0定时器
                else
                {
                    music_ctrl = auto_ctrl = infrared_ctrl = 0;		// 屏蔽其他三个功能
                    TA0CCR0 = 2000;									// TA0执行2ms定时中断
                    TA1CTL = 0;										// 停止音乐播放
                }	
                break;
            //波形切换
            case 10:
                if (wavegen_ctrl && ++wave_type >= 4) wave_type=0;
                switch(wave_type)
                {
                case 0: wave_ptr=squarewave; break;
                case 1: wave_ptr=triangwave; break;
                case 2: wave_ptr=sawtoowave; break;
                case 3: wave_ptr=sincoswave; break;
                default: break;
                }
                break;
            //红外遥感
            case 13:
            	if (!wavegen_ctrl) infrared_ctrl^=1;
            	break;
            //自动增益
            case 14:
                if (!wavegen_ctrl) auto_ctrl^=1;
                break;
            // 切换歌曲（下一首）
            case 15:
                if (music_ctrl && ++music_num >= MUSIC_NUM) music_num=0;
                switch(music_num)
                {
                case 0: music_ptr=music_data0; tone_ptr=tone1; tone_decode=10; audio_dura=0; audio_ptr=0; audio_frequency=0; break;
                case 1: music_ptr=music_data1; tone_ptr=tone1; tone_decode=10; audio_dura=0; audio_ptr=0; audio_frequency=0; break;
                case 2: music_ptr=music_data2; tone_ptr=tone1; tone_decode=10; audio_dura=0; audio_ptr=0; audio_frequency=0; break;
                case 3: music_ptr=music_data3; tone_ptr=tone2; tone_decode=100; audio_dura=0; audio_ptr=0; audio_frequency=0; break;
                default: break;
                }
                break;
            // 暂停播放
            case 16:
                if (!wavegen_ctrl) TA1CTL = (music_ctrl^=1)?(TASSEL_2 + MC_1):0;
                break;
            default: break;
            }
        }
        RefreshDIGITandLEDS();
    }
}
