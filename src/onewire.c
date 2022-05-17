#include <CH58x_common.h>
#include <onewire.h>
#include <stdbool.h>
#include <math.h>

uint8_t buffer;

int LastDiscrepancy;
int LastFamilyDiscrepancy;
int LastDeviceFlag;
unsigned char crc8;
/****全局变量：保存和电容配置寄存器对应的偏置电容和量程电容数值****/
float CapCfg_offset, CapCfg_range;
uint8_t CapCfg_ChanMap, CapCfg_Chan;
unsigned char ROM_NO[8] = {0};

/****偏置电容和反馈电容阵列权系数****/
static const float COS_Factor[8] = {0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 40.0};
/*Cos= (40.0*q[7]+32.0*q[6]+16.0*q[5]+8.0*q[4]+4.0*q[3]+2.0*q[2]+1.0*q[1]+0.5*q[0])*/
static const struct  {float Cfb0; float Factor[6];} CFB = { 2.0, 2.0, 4.0, 8.0, 16.0, 32.0, 46.0};
/*Cfb =(46*p[5]+32*p[4]+16*p[3]+8*p[2]+4*p[1]+2*p[0]+2)*/

static volatile int ow_flag_timerout = 0;


/*********************************************************************
 * @fn      TMR0_IRQHandler
 *
 * @brief   TMR0中断函数
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void) // TMR0 定时中断
{
    if(TMR0_GetITFlag(TMR0_3_IT_CYC_END))
    {
    	TMR0_Disable();
        TMR0_ClearITFlag(TMR0_3_IT_CYC_END); // 清除中断标志
        ow_flag_timerout = 1;
    }
}


static int ow_timer_init(){
	TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END); // 开启中断
	PFIC_EnableIRQ(TMR0_IRQn);
	return 0;
}

static int ow_timer_start(uint32_t timeout_us){
	ow_flag_timerout = 0;
	TMR0_TimerInit(GetSysClock() / 1000000  * timeout_us);
	return 0;
}

static int ow_timer_stop(){
	TMR0_Disable();
	return 0;
}

static int ow_delay_us(uint32_t us){
	ow_timer_start(us);
	while(!ow_flag_timerout);
	ow_timer_stop();
	return 0;
}

void slave_init(){
	
	ow_timer_init();
	ow_delay_us(100);
	GPIOA_ModeCfg(GPIO_Pin_1,GPIO_ModeOut_PP_5mA);
	GPIOA_SetBits(GPIO_Pin_1);
	
	onewire_resetcheck();	
}

int onewire_resetcheck(){
	uint8_t flag=0;

	onewire_reset();
	flag=check_ack();
	if(flag != 0){
		PRINT("error\r\n");
	}
	ow_delay_us(240);
	return flag;
}
int check_ack(){
	uint8_t ack_flag,i;
	//GPIOA_ModeCfg(GPIO_Pin_1,GPIO_ModeOut_PP_5mA);
	//GPIOA_ResetBits(GPIO_Pin_1);
	//ow_delay_us(240);
	int cnt = 0;
	while(GPIOA_ReadPortPin(GPIO_Pin_1))
	{
		ow_delay_us(1);
		cnt ++;
		if (cnt >= 60){
			PRINT("wait falling edge timeout\r\n");
			goto fail;
		}
	}
	cnt = 0;
	while(0 == GPIOA_ReadPortPin(GPIO_Pin_1)){
		ow_delay_us(10);
		cnt +=10;
		if (cnt >= 240){
			PRINT("err: slave not release wire\r\n");
			goto fail;
		}
	}
	if (cnt < 60){
		PRINT("err: low time < 60\r\n");
		goto fail;
	}

	
	//GPIOA_SetBits(GPIO_Pin_1);
	//ow_delay_us(240);
	return 0;
fail:
	return 1;
}

void onewire_reset(){
	GPIOA_ModeCfg(GPIO_Pin_1,GPIO_ModeOut_PP_5mA);
	GPIOA_ResetBits(GPIO_Pin_1);
	ow_delay_us(500);
	GPIOA_SetBits(GPIO_Pin_1);
	GPIOA_ModeCfg(GPIO_Pin_1,GPIO_ModeIN_PU);
	//ow_delay_us(60);
}




void  onewire_writebit(int bit){
	GPIOA_ModeCfg(GPIO_Pin_1,GPIO_ModeOut_PP_5mA);
	GPIOA_ResetBits(GPIO_Pin_1);
	if (bit)
	{
		// Write '1' to DQ
		ow_delay_us(15);//15s 内释放总线
		GPIOA_SetBits(GPIO_Pin_1);
		ow_delay_us(50); 
	}
	else
	{
		// Write '0' to DQ
		ow_delay_us(60);
	}
	GPIOA_SetBits(GPIO_Pin_1);
	ow_delay_us(2); //恢复时间
	
}
void onewire_writebyte(char data){
	uint8_t i; 
	for(i = 0;i < 8;i++){
		onewire_writebit(data & 0x01);//data&0x01：将data数据的最低位赋值给data
		data >>=1;
	}
}


unsigned char onewire_read(){
	uint8_t i;
	uint8_t read_data=0;
	for(i=0;i<8;i++){
		
		read_data >>= 1;
		
		if(onewire_readbit()){
			read_data |= 0x80;
		}	
	}
	return read_data;
}
int onewire_readbit(){
	int j=0;
	GPIOA_ModeCfg(GPIO_Pin_1,GPIO_ModeOut_PP_5mA);
	GPIOA_ResetBits(GPIO_Pin_1);
	
	ow_delay_us(2);//启动读时隙
	GPIOA_SetBits(GPIO_Pin_1);
	GPIOA_ModeCfg(GPIO_Pin_1, GPIO_ModeIN_PU);

	ow_delay_us(5);
	
	j=GPIOA_ReadPortPin(GPIO_Pin_1);//因为“0”和“1”是通过电平的高低表示的
	if(j == 0){
		j=0;
		ow_delay_us(4);
		if (!GPIOA_ReadPortPin(GPIO_Pin_1)){
			goto fail;
		}
	}else{
		j=1;
	}
	ow_delay_us(60);
	return j;
fail:
	return -1;
}

int onewire_read2bits()
{
	uint8_t i, dq,data = 0;
	for(i=0; i<2; i++)
	{
		dq = onewire_readbit();
		data = (data) | (dq<<i);
	}

		return data;
}

void read_function(){
	uint8_t b=0;
	uint8_t c=0;
	unsigned char a=0;
	onewire_reset();
	check_ack();
	
	PRINT("%02x\r\n",c);
	
}



//
////
/////********    convert CAP       ********/////////////

/**
  * @brief  读电容配置
  * @param  Coffset：配置的偏置电容。
  * @param  Crange：配置的量程电容。
  * @retval 无
*/

bool readcapconfigure(float *Coffset, float *Crange)
{
	getcfg_capoffset(Coffset);
	getcfg_caprange(Crange);

	return TRUE;
}
////
/////**
////  * @brief  获取配置的量程电容数值（pF）
////  * @param  Crange：返回量程电容数值
////  * @retval 无
////*/
void getcfg_caprange(float *Crange)
{
	uint8_t Cfb_cfg;

	readcfbconfig(&Cfb_cfg);
	*Crange = cfbcfgtocaprange(Cfb_cfg);
//	PRINT("cfbcfg to capcfb:%.4f\r\n",*Crange);
}
//
///* @brief  获取配置的偏置电容数值（pF）
//  * @param  Coffset：偏置电容配置
//  * @retval 无
//*/
void getcfg_capoffset(float *Coffset)
{	uint8_t Cos_cfg;

	readcosconfig(&Cos_cfg);
	*Coffset = coscfgtocapoffset(Cos_cfg);
//	PRINT("coscfg to capoffset:%.2f\r\n",*Coffset);
}
//
////
/////**
////  * @brief  将量程电容配置转换为对应的量程电容数值（pF）
////  * @param  fbCfg：量程电容配置
////  * @retval 对应量程电容的数值
////*/
float cfbcfgtocaprange(uint8_t fbCfg)
{
	uint8_t i;
	float Crange = CFB.Cfb0;

	for(i = 0; i <= 5; i++)
	{
		if(fbCfg & 0x01){
			Crange += CFB.Factor[i];
		}
		fbCfg >>= 1;
	}
	return (0.507/3.6) * Crange;
}
//
/////**
////  * @brief  读量程电容配置寄存器内容
////  * @param  Cfb：量程配置寄存器低6位的内容
////  * @retval 状态
////*/
bool readcfbconfig(uint8_t *Cfb)
{
	uint8_t scrb[sizeof(MDC02_SCRPARAMETERS)];
	MDC02_SCRPARAMETERS *scr = (MDC02_SCRPARAMETERS *) scrb;

	/*读15个字节。第5字节是偏置电容配置寄存器，第10字节是量程电容配置寄存器，最后字节是前14个的校验和--CRC。*/
	if(mdc02_readparameters_skiprom(scrb) == FALSE)
	{
		return FALSE;  /*读寄存器失败*/
	}

	/*计算接收的前14个字节的校验和，并与接收的第15个CRC字节比较。*/
	if(scrb[sizeof(MDC02_SCRPARAMETERS)-1] != onewire_crc8(&scrb[0], sizeof(MDC02_SCRPARAMETERS)-1))
	{
		return FALSE;   /*CRC验证未通过*/
	}

	*Cfb = scr->Cfb & MDC02_CFEED_CFB_MASK;
//	PRINT("read register cfb:%d\r\n",*Cfb);
	return TRUE;;
}
//
//
///**
//  * @brief  将偏置电容配置转换为对应的偏置电容数值（pF）
//  * @param  osCfg：偏置电容配置
//  * @retval 对应偏置电容的数值
//*/
float coscfgtocapoffset(uint8_t osCfg)
{
	uint8_t i;
	float Coffset = 0.0;

	for(i = 0; i < 8; i++)
	{
		if(osCfg & 0x01) Coffset += COS_Factor[i];{
			osCfg >>= 1;
		}
	}

	return Coffset;
}

//
////
/**
  * @brief  读偏置电容配置寄存器内容
  * @param  Coffset：偏置配置寄存器有效位的内容
  * @retval 无
*/

bool readcosconfig(uint8_t *Coscfg)
{
	uint8_t scrb[sizeof(MDC02_SCRPARAMETERS)];
	MDC02_SCRPARAMETERS *scr = (MDC02_SCRPARAMETERS *) scrb;

	/*读15个字节。第5字节是偏置电容配置寄存器，第10字节是量程电容配置寄存器，最后字节是前14个的校验和--CRC。*/
	if(mdc02_readparameters_skiprom(scrb) == FALSE)
	{
		return FALSE;  /*读寄存器失败*/
	}

	/*计算接收的前14个字节的校验和，并与接收的第15个CRC字节比较。*/
  	if(scrb[sizeof(MDC02_SCRPARAMETERS)-1] != onewire_crc8(&scrb[0], sizeof(MDC02_SCRPARAMETERS)-1))
  	{
  		return FALSE;  /*CRC验证未通过*/
  	}

	*Coscfg = scr->Cos & (0xFF >> (3 - (scr->Cfb >> 6))); //屏蔽掉无效位，根据CFB寄存器的高2位
//	PRINT("read register cos:%d\r\n",*Coscfg);

  	return TRUE;
}
////
bool mdc02_writeparameters_skiprom(uint8_t *scr)
{
    int16_t i;

    if(onewire_resetcheck() != 0){
		return FALSE;
	}

    read_command();
    onewire_writebyte(0xAB);

	for(i=0; i < sizeof(MDC02_SCRPARAMETERS); i++)
    {
    	onewire_writebyte(*scr++);
	}

    return TRUE;
}
bool mdc02_readparameters_skiprom(uint8_t *scr)
{
    int16_t i;

    if(onewire_resetcheck() != 0){
		return FALSE;
	}

    read_command();
    onewire_writebyte(0x8B);

	for(i=0; i < sizeof(MDC02_SCRPARAMETERS); i++)
    {
    	*scr++ = onewire_read();
	}

    return TRUE;
}

///**
//  * @brief  读状态和配置
//  * @param  status 返回的状态寄存器值
//  * @param  cfg 返回的配置寄存器值
//  * @retval 状态
//*/
bool readstatusconfig(uint8_t *status, uint8_t *cfg)
{
	uint8_t scrb[sizeof(MDC02_SCRATCHPAD_READ)];
	MDC02_SCRATCHPAD_READ *scr = (MDC02_SCRATCHPAD_READ *) scrb;

	/*读9个字节。第7字节是系统配置寄存器，第8字节是系统状态寄存器。最后字节是前8个的校验和--CRC。*/
	if(mdc02_readscratchpad_skiprom(scrb) == FALSE)
	{
		return FALSE;  /*CRC验证未通过*/
	}

	/*计算接收的前8个字节的校验和，并与接收的第9个CRC字节比较。*/
  	if(scrb[8] != onewire_crc8(&scrb[0], 8))
  	{
  		return FALSE;  /*CRC验证未通过*/
  	}

	*status = scr->Status;
	*cfg = scr->Cfg;

	return TRUE;
}
//
////
/////**
////  * @brief  读芯片寄存器的暂存器组
////  * @param  scr：字节数组指针， 长度为 @sizeof（MDC04_SCRATCHPAD_READ）
////  * @retval 读状态
////*/
////bool mdc02_readscratchpad_skiprom(uint8_t *scr)
////{
////    int16_t i;
////
////	/*size < sizeof(MDC04_SCRATCHPAD_READ)*/
////    if(OW_ResetPresence() == FALSE)
////			return FALSE;
////
////    onewire_writebyte(0xCC);
////    onewire_writebyte(0xBE);
////
////		for(i=0; i < sizeof(MDC02_SCRATCHPAD_READ); i++)
////    {
////			*scr++ = onewire_read();
////		}
////
////    return TRUE;
////}
////bool mdc02_writescratchpadext_skiprom(uint8_t *scr)
////{
////    int16_t i;
////
////    if(OW_ResetPresence() == FALSE)
////			return FALSE;
////
////    onewire_writebyte(0xCC);
////    onewire_writebyte(0x77);
////
////		for(i=0; i<sizeof(MDC04_SCRATCHPADEXT)-1; i++)
////    {
////			onewire_writebyte(*scr++);
////		}
////
////    return TRUE;
////}
////
////
bool setcapchannel(uint8_t channel)
{
	uint8_t scrb[sizeof(MDC02_SCRPARAMETERS)];
	MDC02_SCRPARAMETERS *scr = (MDC02_SCRPARAMETERS *) scrb;

	/*读15个字节。第4字节是通道选择寄存器，最后字节是前14个的校验和--CRC。*/
	if(mdc02_readparameters_skiprom(scrb) == FALSE)
	{
		return FALSE;  /*读寄存器失败*/
	}

	/*计算接收的前14个字节的校验和，并与接收的第15个CRC字节比较。*/
	if(scrb[sizeof(MDC02_SCRPARAMETERS)-1] != onewire_crc8(&scrb[0], sizeof(MDC02_SCRPARAMETERS)-1))
	{
		return FALSE;  /*CRC验证未通过*/
	}

	scr->Ch_Sel = (scr->Ch_Sel & ~CCS_CHANNEL_MASK) | (channel & CCS_CHANNEL_MASK);

	mdc02_writeparameters_skiprom(scrb);

	return TRUE;
}

bool convertcap(void)
{
	if(onewire_resetcheck() !=  0){
		return FALSE;
    }

    read_command();
    onewire_writebyte(0x66);

    return TRUE;
}


bool readtempcap1(uint16_t *iTemp, uint16_t *iCap1)
{
	uint8_t scrb[sizeof(MDC02_SCRATCHPAD_READ)];
	MDC02_SCRATCHPAD_READ *scr = (MDC02_SCRATCHPAD_READ *) scrb;

	/*读9个字节。前两个是温度转换结果，最后字节是前8个的校验和--CRC。*/
	if(mdc02_readscratchpad_skiprom(scrb) == FALSE)
	{
		return FALSE;  /*读寄存器失败*/
	}

	/*计算接收的前8个字节的校验和，并与接收的第9个CRC字节比较。*/
	if(scrb[8] != onewire_crc8(&scrb[0], 8))
	{
	    return FALSE;  /*CRC验证未通过*/
	}

	*iTemp=(uint16_t)scr->T_msb<<8 | scr->T_lsb;
	*iCap1=(uint16_t)scr->C1_msb<<8 | scr->C1_lsb;
	return TRUE;
}
////
/////**
////  * @brief  读电容通道2，1
////  * @param  scr：字节数组指针， 长度为 @sizeof（MDC04_C2C3C4）
////  * @retval 写状态
////**/

bool  mdc02_readscratchpad_skiprom(uint8_t *scr)
{
    int16_t i;

	/*size < sizeof(MDC04_SCRATCHPAD_READ)*/
    if(onewire_resetcheck() != 0){
		return FALSE;
	}

    read_command();
    onewire_writebyte(0xBE);

	for(i=0; i < sizeof(MDC02_SCRATCHPAD_READ); i++)
    {
		*scr++ = onewire_read();
	}

    return TRUE;
}



bool mdc02_readc2c3c4_skiprom(uint8_t *scr)
{
    int16_t i;

    if(onewire_resetcheck() != 0){
			return FALSE;
	}

    read_command();
    onewire_writebyte(0xDC);

	for(i=0; i < sizeof(MDC02_C2C3C4); i++)
   {
	    *scr++ = onewire_read();
	}

    return TRUE;
}
////
////
////
bool mdc02_capconfigureoffset(float Coffset)
{
	uint8_t CosCfg, Cosbits;
	float b=Coffset+0.25;
	CosCfg = captocoscfg(b);

	if(!(CosCfg & ~0x1F)) {
		Cosbits = COS_RANGE_5BIT;
	}
	else if(!(CosCfg & ~0x3F)) {
		Cosbits = COS_RANGE_6BIT;
	}
	else if(!(CosCfg & ~0x7F)){
		Cosbits = COS_RANGE_7BIT;
	}
	else{
		Cosbits = COS_RANGE_8BIT;
	}
//	PRINT("%d  %d\r\n",CosCfg,Cosbits);
	writecosconfig(CosCfg, Cosbits);

	return TRUE;
}

/**
  * @brief  写偏置电容配置寄存器和有效位宽设置
  * @param  Coffset：偏置配置寄存器的数值
  * @param  Cosbits：偏置配置寄存器有效位宽，可能为：
	*		@COS_RANGE_5BIT
	*		@COS_RANGE_6BIT
	*		@COS_RANGE_7BIT
	*		@COS_RANGE_8BIT
  * @retval 状态
*/
bool writecosconfig(uint8_t Coffset, uint8_t Cosbits)
{
	uint8_t scrb[sizeof(MDC02_SCRPARAMETERS)];
	MDC02_SCRPARAMETERS *scr = (MDC02_SCRPARAMETERS *) scrb;

	/*读15个字节。第5字节是偏置电容配置寄存器，第10字节是量程电容配置寄存器，最后字节是前14个的校验和--CRC。*/
	if(mdc02_readparameters_skiprom(scrb) == FALSE)
	{
		return FALSE;   /*读寄存器失败*/
	}

	/*计算接收的前14个字节的校验和，并与接收的第15个CRC字节比较。*/
  	if(scrb[sizeof(MDC02_SCRPARAMETERS)-1] != onewire_crc8(&scrb[0], sizeof(MDC02_SCRPARAMETERS)-1))
  	{
		return FALSE;  /*CRC验证未通过*/
  	}

	scr->Cos = Coffset;
	scr->Cfb = (scr->Cfb & ~CFB_COSRANGE_Mask) | Cosbits;
//	PRINT("write cosconfig:%d %f\r\n",scr->Cos,scr->Cfb);

	mdc02_writeparameters_skiprom(scrb);

  	return TRUE;
}

/**
  * @brief  将偏置电容数值（pF）转换为对应的偏置电容配置
  * @param  osCap：偏置电容的数值
  * @retval 对应偏置配置寄存器的数值
*/
uint8_t captocoscfg(float osCap)
{
	int i; 
	uint8_t CosCfg = 0x00;

	for(i = 7; i >= 0; i--)
	{
		if(osCap >= COS_Factor[i])
		{
			CosCfg |= (0x01 << i);
			osCap -= COS_Factor[i];
		}
	}
	return CosCfg;
}

/**
  * @brief  配置电容测量范围
  * @param  Cmin：要配置测量范围的低端。
  * @param  Cmax：要配置测量范围的高端。
  * @retval 状态
*/
bool mdc02_capconfigurerange(float Cmin, float Cmax)
{ 
	float Cfs, Cos;

//	if(!((Cmax <= 119.0) && (Cmax > Cmin) && (Cmin >= 0.0) && ((Cmax-Cmin) <= 31.0)))
//	return FALSE;	//The input value is out of range.

	Cos = (Cmin + Cmax)/2.0;
	Cfs = (Cmax - Cmin)/2.0;

	mdc02_capconfigureoffset(Cos);
	mdc02_capconfigurefs(Cfs);

	return TRUE;
}

/**
  * @brief  配置量程电容
  * @param  Cfs：要配置的量程电容数值。范围+/-（0.281~15.49） pF。
  * @retval 状态
*/
bool mdc02_capconfigurefs(float Cfs)
{
	uint8_t Cfbcfg;

	Cfs = (Cfs + 0.1408);
	Cfbcfg = caprangetocfbcfg(Cfs);
//	PRINT("before write cfbcfg: %d\r\n",Cfbcfg);

	writecfbconfig(Cfbcfg);

	return TRUE;
}

/**
  * @brief  将量程电容数值（pF）转换为对应的量程电容配置
  * @param  fsCap：量程电容的数值
  * @retval 对应量程配置的数值
*/
uint8_t caprangetocfbcfg(float fsCap)
{
	int8_t i; 
	uint8_t CfbCfg = 0x00;

	fsCap = fsCap * (3.6/0.507);

	fsCap -= CFB.Cfb0;

	for(i = 5; i >= 0; i--)
	{
		if(fsCap >= CFB.Factor[i])
		{
			fsCap -= CFB.Factor[i];
			CfbCfg |= (0x01 << i);
		}
	}

	return CfbCfg;
}
/**
  * @brief  写量程电容配置寄存器
  * @param  Cfb：量程配置寄存器低6位的内容
  * @retval 状态
*/
bool writecfbconfig(uint8_t Cfb)
{
	uint8_t scrb[sizeof(MDC02_SCRPARAMETERS)];
	MDC02_SCRPARAMETERS *scr = (MDC02_SCRPARAMETERS *) scrb;

	/*读15个字节。第5字节是偏置电容配置寄存器，第10字节是量程电容配置寄存器，最后字节是前14个的校验和--CRC。*/
	if(mdc02_readparameters_skiprom(scrb) == FALSE)
	{
		return FALSE;   /*读寄存器失败*/
	}

	/*计算接收的前14个字节的校验和，并与接收的第15个CRC字节比较。*/
  	if(scrb[sizeof(MDC02_SCRPARAMETERS)-1] != onewire_crc8(&scrb[0], sizeof(MDC02_SCRPARAMETERS)-1))
  	{
  		return FALSE;  /*CRC验证未通过*/
	}

	scr->Cfb &= ~CFB_CFBSEL_Mask;
	scr->Cfb |= Cfb;
//	PRINT("write cfb:%d\r\n",scr->Cfb);

	mdc02_writeparameters_skiprom(scrb);
	return TRUE;
}


//
//
///**
//  * @brief  读电容通道2，3和4的测量结果。和 @ConvertCap联合使用
//  * @param  icap：数组指针
//  * @retval 读结果状态
//*/
bool readcapc2c3c4(uint16_t *iCap)
{
	uint8_t scrb[sizeof(MDC02_C2C3C4)];
	MDC02_C2C3C4 *scr = (MDC02_C2C3C4 *) scrb;

	/*读6个字节。每两个字节依序分别为通道2、3和4的测量结果，最后字节是前两个的校验和--CRC。*/
	if(mdc02_readc2c3c4_skiprom(scrb) == FALSE)
	{
		return FALSE;  /*读寄存器失败*/
	}

	/*计算接收的前两个字节的校验和，并与接收的第3个CRC字节比较。*/
//  if(scrb[8] != MY_OW_CRC8(scrb, 8))
//  {
//		return FALSE;  /*CRC验证未通过*/
//  }

	*iCap= (uint16_t)scr->C2_msb<<8 | scr->C2_lsb;
//	PRINT("%.2x",*iCap);
//	iCap[1] = (uint16_t)scr->C3_msb<<8 | scr->C3_lsb;
//	iCap[2] = (uint16_t)scr->C4_msb<<8 | scr->C4_lsb;
	return TRUE;
}

//


float mdc02_outputtocap(uint16_t out, float Co, float Cr)
{
	return (2.0*(out/65535.0-0.5)*Cr+Co);
}

//////////////////////////////////////////*********/////////////////////////
//**********   research rom   **********//
//********** 查找从机的rom id   **********//


//读取rom id
bool readrom(uint8_t *scr)
{
    int16_t i;
	onewire_reset();
    if(check_ack() == 1)
			return FALSE;
		
    onewire_writebyte(0x55);

		for(i=0; i < sizeof(MDC02_ROMCODE); i++)
    {
			*scr++ = onewire_read();
		}

    return TRUE;
}

int onewire_read_rom(void)
{ 
	uint8_t rom_id[8];
	readrom(rom_id);
	PRINT("\r\n MDC02 ROMID :");				
	for(int i = 0;i < 8;i++)
	{
		PRINT("%2x ", rom_id[i]);				
	}		
	return 1;
}

int onewire_first()
{
   // reset the search state
   LastDiscrepancy = 0;
   LastDeviceFlag = FALSE;
   LastFamilyDiscrepancy = 0;
   memset(ROM_NO,0,sizeof(char)*8);
   return onewire_search();
}


//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
int onewire_next()
{
   // leave the search state alone
   return onewire_search();
}

void onewirefamilyskipsetup()
{
   // set the Last discrepancy to last family discrepancy
   LastDiscrepancy = LastFamilyDiscrepancy;
   LastFamilyDiscrepancy = 0;

   // check for end of list
   if (LastDiscrepancy == 0){
      LastDeviceFlag = TRUE;
   }
}



//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
int onewire_search()
{
   int id_bit_number;
   int last_zero, rom_byte_number, search_result;
   int id_bit, cmp_id_bit;
   unsigned char rom_byte_mask, search_direction;

   // initialize for search
   id_bit_number = 1;
   last_zero = 0;
   rom_byte_number = 0;
   rom_byte_mask = 1;
   search_result = 0;
   crc8 = 0;

   // if the last call was not the last one
   if (!LastDeviceFlag){
   	  onewire_reset();
      // 1-Wire reset
      if (check_ack()){
         // reset the search
         LastDiscrepancy = 0;
         LastDeviceFlag = FALSE;
         LastFamilyDiscrepancy = 0;
         return FALSE;
      }

      // issue the search command 
      onewire_writebyte(0xF0);  

      // loop to do the search
      do
      {
         // read a bit and its complement
         id_bit = onewire_readbit();
         cmp_id_bit = onewire_readbit();

         // check for no devices on 1-wire
         if ((id_bit == 1) && (cmp_id_bit == 1)){
		 	PRINT("no device exist\r\n");
            break;
		 }
         else
         {
            // all devices coupled have 0 or 1
            if (id_bit != cmp_id_bit){
               search_direction = id_bit;  // bit write value for search
            }
            else
            {
               // if this discrepancy if before the Last Discrepancy
               // on a previous next then pick the same as last time
               if (id_bit_number < LastDiscrepancy){
                  search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
			   }
               else{
                  // if equal to last pick 1, if not then pick 0
                  search_direction = (id_bit_number == LastDiscrepancy);
			   }

               // if 0 was picked then record its position in LastZero
               if (search_direction == 0)
               {
                  last_zero = id_bit_number;

                  // check for Last discrepancy in family
                  if (last_zero < 9){
                     LastFamilyDiscrepancy = last_zero;
				  }
               }
            }

            // set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1)
              ROM_NO[rom_byte_number] |= rom_byte_mask;
            else
              ROM_NO[rom_byte_number] &= ~rom_byte_mask;

            // serial number search direction write bit
            onewire_writebit(search_direction);

            // increment the byte counter id_bit_number
            // and shift the mask rom_byte_mask
            id_bit_number++;
            rom_byte_mask <<= 1;

            // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
            if (rom_byte_mask == 0)
            {
                //crc8 = onewire_crc8(ROM_NO[rom_byte_number],sizeof(ROM_NO[rom_byte_number]));  // accumulate the CRC
                rom_byte_number++;
                rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

      // if the search was successful then
      if (!((id_bit_number < 65) || (crc8 != 0)))	//id_bit_number  >= 65 && crc == 0
      {
         // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
         LastDiscrepancy = last_zero;

         // check for last device
         if (LastDiscrepancy == 0){
            LastDeviceFlag = TRUE;
		 }
         
         search_result = TRUE;
      }
   }

   // if no device found then reset counters so next 'search' will be like a first
   if (!search_result || !ROM_NO[0])
   {
      LastDiscrepancy = 0;
      LastDeviceFlag = FALSE;
      LastFamilyDiscrepancy = 0;
      search_result = FALSE;
	  PRINT("search failed.\r\n");
	  __nop();
   }

   return search_result;
}



//多个字节的crc8算法
uint8_t onewire_crc8(uint8_t *serial, uint8_t length)
{
    uint8_t crc8 = 0x00;
    uint8_t pDataBuf;
    uint8_t i;

    while(length--) {
//        pDataBuf = *serial++;
		crc8 ^= *serial++;
        for(i=0; i<8; i++) {
            if((crc8^(pDataBuf))&0x01) {
                crc8 ^= 0x18;
                crc8 >>= 1;
                crc8 |= 0x80;
            }
            else {
                crc8 >>= 1;
            }
            pDataBuf >>= 1;
    }
  }
    return crc8;
}

void onewire_finddevic_fun(){ 
    volatile int result,i,cnt;
	PRINT("FIND ALL \r\n");
   	cnt = 0;
	result = onewire_first();
	while (result)
	{
		// print device found
	    for (i = 0; i < 8 ; i++){
	       PRINT("%.2X\t", ROM_NO[i]);
		}
	    PRINT("  %d\r\n",++cnt);

	    result = onewire_next();
	}
}
//--------------------------------------------------------------------------
// Verify the device with the ROM number in ROM_NO buffer is present.
// Return TRUE  : device verified present
//        FALSE : device not present
//
int onewire_verify()
{
   unsigned char rom_backup[8];
   int i,result,ld_backup,ldf_backup,lfd_backup;

   // keep a backup copy of the current state
   for (i = 0; i < 8; i++){
      rom_backup[i] = ROM_NO[i];
   }
   ld_backup = LastDiscrepancy;
   ldf_backup = LastDeviceFlag;
   lfd_backup = LastFamilyDiscrepancy;

   // set search to find the same device
   LastDiscrepancy = 64;
   LastDeviceFlag = FALSE;

   if (onewire_search()){
   	// check if same device found
   		result = TRUE;
    	for(i = 0; i < 8; i++){
			if (rom_backup[i] != ROM_NO[i]){
				result = FALSE;
	            break;
	        }
		}
   }
   else{
   		result = FALSE;
   }

   // restore the search state 
   for (i = 0; i < 8; i++){
      ROM_NO[i] = rom_backup[i];
   }
   LastDiscrepancy = ld_backup;
   LastDeviceFlag = ldf_backup;
   LastFamilyDiscrepancy = lfd_backup;

   // return the result of the verify
   return result;
}

//--------------------------------------------------------------------------
// Setup the search to find the device type 'family_code' on the next call
// to OWNext() if it is present.
//
void onewire_targetsetup(unsigned char family_code)
{
   int i;

   // set the search state to find SearchFamily type devices
   ROM_NO[0] = family_code;
   for (i = 1; i < 8; i++){
   		ROM_NO[i] = 0;
   }
   LastDiscrepancy = 64;
   LastFamilyDiscrepancy = 0;
   LastDeviceFlag = FALSE;
}

//--------------------------------------------------------------------------
// Setup the search to skip the current device type on the next call
// to OWNext().
//
void onewire_familyshipsetup()
{
   // set the Last discrepancy to last family discrepancy
   LastDiscrepancy = LastFamilyDiscrepancy;
   LastFamilyDiscrepancy = 0;

   // check for end of list
   if (LastDiscrepancy == 0){
   		LastDeviceFlag = TRUE;
   }
}


void read_command(){
	
	onewire_writebyte(0x55);
	
	onewire_writebyte(0x28);
	onewire_writebyte(0x95);
	onewire_writebyte(0x01);
	onewire_writebyte(0x76);
	onewire_writebyte(0x44);
	onewire_writebyte(0xf5);
	onewire_writebyte(0x00);
	onewire_writebyte(0x00);
}



int read_cap(void)
{	
	float fcap1, fcap2, fcap3, fcap4; uint16_t iTemp, icap1, icap[1];
	uint8_t status, cfg;
		
	setcapchannel(CAP_CH1CH2_SEL);
	readstatusconfig((uint8_t *)&status, (uint8_t *)&cfg);
	
	if(convertcap() == FALSE)
	{
		PRINT("No MDC02\r\n");
	}
	else{
		
		ow_delay_us(15);
		readcapconfigure(&CapCfg_offset, &CapCfg_range);
//		PRINT("%f  %.5f\r\n",CapCfg_offset,CapCfg_range);
		readstatusconfig((uint8_t *)&status, (uint8_t *)&cfg);
		readtempcap1(&iTemp, &icap1);
		if(readcapc2c3c4(&icap[0]) == FALSE){
			PRINT("read cap2 error");
		}
		fcap1 = mdc02_outputtocap(icap1, CapCfg_offset, CapCfg_range);
		fcap2 = mdc02_outputtocap(icap[0], CapCfg_offset, CapCfg_range);
//		fcap3 = MDC04_OutputtoCap(icap[1], CapCfg_offset, CapCfg_range);
//		fcap4=  MDC04_OutputtoCap(icap[2], CapCfg_offset, CapCfg_range);
//		printf("\r\n C1=%5d , %6.3f  C2=%5d, %6.3f  C3=%5d, %6.3f  C4=%5d, %6.3f  SC=%02X%02X", icap1, fcap1, icap[0], fcap2, icap[1], fcap3, icap[2], fcap4, status, cfg);
		PRINT("\r\n");
		PRINT(" C1=%4d , %6.3f  C2=%4d, %6.3f  S=%02X   C=%02X\r\n", icap1, fcap1, icap[0], fcap2, status, cfg);
	
		
		}

	ow_delay_us(990);
	return 1;		
}

/*
  * @brief  设置偏置电容offset
*/
int mdc02_offset(float Co)
{
	PRINT("Co= %5.2f\r\n", Co);
	if(!((Co >=0.0) && (Co <= 103.5))) {
		PRINT(" %s", "The input is out of range"); 
		return 0;
	}
		
	mdc02_capconfigureoffset(Co);
	return 1;
}
/*
  * @brief  设置电容测量范围
  * 请勿设置超出电容量程0~119pf,请勿超出最大range:±15.5pf
*/
int mdc02_range(float Cmin,float Cmax)
{ 
//		printf("\r\nCmin= %3.2f Cmax=%3.2f", Cmin, Cmax);
	if(!((Cmax <= 119.0) && (Cmax > Cmin) && (Cmin >= 0.0) && 
			((Cmax-Cmin) <= 31.0)))  
	{
		PRINT(" %s", "The input is out of range"); 
		return 0;
	}
		
	mdc02_capconfigurerange(Cmin, Cmax);
		
	readcapconfigure(&CapCfg_offset, &CapCfg_range);
//	PRINT("%.2f  %.5f\r\n",CapCfg_offset,CapCfg_range);
		
	return 1;
}
int mdc02_channel(uint8_t channel)
{
	setcapchannel(channel);
									
	return 1;
}

//-----------------------------------------

