#ifndef HEAD_ONEWIRE_H_
#define HEAD_ONEWIRE_H_
#include "CH58x_common.h"
#include <stdbool.h>
/******************  Bit definition for feeadback capacitor register  *******/
#define MDC02_CFEED_OSR_MASK           	   	0xC0
#define MDC02_CFEED_CFB_MASK           	   	0x3F
/*Bit definition of Ch_Sel register*/
#define CCS_CHANNEL_MASK   					0x07


void *memset(void *s, int ch, size_t n);

void onewire_reset();
void slave_init();
int check_ack();
void onewire_writebyte(char data);
void  onewire_writebit(int bit);

int onewire_readbit();
int onewire_read2bits();
unsigned char onewire_read();
void read_command();


/////////////****  search rom_id **** ////////////
void read_function(void);
uint8_t onewire_crc8(uint8_t *serial, uint8_t length);
int onewire_search();
int onewire_next();
int onewire_first();
int onewire_read_rom(void);
void onewire_targetsetup(unsigned char family_code);
void onewire_familyshipsetup();
void onewire_finddevic_fun();
int onewire_resetcheck();
int mdc02_channel(uint8_t channel);


uint8_t captocoscfg(float osCap);
float cfbcfgtocaprange(uint8_t fbCfg);
float coscfgtocapoffset(uint8_t osCfg);
float mdc02_outputtocap(uint16_t out, float Co, float Cr);

void getcfg_capoffset(float *Coffset);
void getcfg_caprange(float *Crange);
bool setcapchannel(uint8_t channel);
////*** write ***////

bool mdc02_writescratchpadext_skiprom(uint8_t *scr);
bool writecosconfig(uint8_t Coffset, uint8_t Cosbits);
bool mdc02_writeparameters_skiprom(uint8_t *scr);
bool mdc02_capconfigurerange(float Cmin, float Cmax);
bool writecfbconfig(uint8_t Cfb);


/////////////**** read ****/////////////
int read_cap(void);
bool readcfbconfig(uint8_t *Cfb);
bool mdc02_readparameters_skiprom(uint8_t *scr);
bool readcosconfig(uint8_t *Coscfg);
bool readcapconfigure(float *Coffset, float *Crange);
bool readstatusconfig(uint8_t *status, uint8_t *cfg);
bool mdc02_capconfigureoffset(float Coffset);
bool readtempcap1(uint16_t *iTemp, uint16_t *iCap1);
bool mdc02_readc2c3c4_skiprom(uint8_t *scr);
bool readcapc2c3c4(uint16_t *iCap);
bool convertcap(void);
bool mdc02_readscratchpad_skiprom(uint8_t *scr);
int mdc02_offset(float Co);
int mdc02_range(float Cmin,float Cmax);
bool mdc02_capconfigurefs(float Cfs);
uint8_t caprangetocfbcfg(float fsCap);




typedef struct
{
	uint8_t Res[3];
	uint8_t Ch_Sel;					/*电容通道选择寄存器，RW*/
	uint8_t Cos;						/*偏置电容配置寄存器，RW*/
	uint8_t Res1;				
	uint8_t T_coeff[3];			
	uint8_t Cfb;						/*量程电容配置寄存器，RW*/									
	uint8_t Res2;
	uint8_t Res3[2];
	uint8_t dummy8;
	uint8_t crc_para;				/*CRC for byte0-13, RO*/
} MDC02_SCRPARAMETERS;

typedef struct
{
	uint8_t T_lsb;					/*The LSB of 温度结果, RO*/
	uint8_t T_msb;					/*The MSB of 温度结果, RO*/
	uint8_t C1_lsb;					/*The LSB of 电容通道C1, RO*/
	uint8_t C1_msb;					/*The MSB of 电容通道C1, Ro*/	
	uint8_t Tha_set_lsb;		
	uint8_t Tla_set_lsb;		
	uint8_t Cfg;						/*系统配置寄存器, RW*/
	uint8_t Status;					/*系统状态寄存器, RO*/
	uint8_t crc_scr;				/*CRC for byte0-7, RO*/
} MDC02_SCRATCHPAD_READ;

typedef struct
{
	uint8_t Family;				/*Family byte, RO*/
	uint8_t Id[6];				/*Unique ID, RO*/
	uint8_t crc_rc;				/*Crc code for byte0-7, RO*/
} MDC02_ROMCODE;

typedef enum{
CAP_CH1_SEL =0x01,
CAP_CH2_SEL =0x02,
CAP_CH3_SEL =0x03,
CAP_CH4_SEL =0x04, 
CAP_CH1CH2_SEL =0x05 ,
CAP_CH1CH2CH3_SEL = 0x06,
CAP_CH1CH2CH3CH4_SEL =0x07,
}onewire_capch;
typedef struct
{	
	uint8_t C2_lsb;				/*The LSB of C2, RO*/
	uint8_t C2_msb;				/*The MSB of C2, RO*/
	uint8_t C3_lsb;				/*The LSB of C3, RO*/
	uint8_t C3_msb;				/*The MSB of C3, RO*/
	uint8_t C4_lsb;				/*The LSB of C4, RO*/
	uint8_t C4_msb;				/*The MSB of C4, RO*/
/*crc*/	
} MDC02_C2C3C4;


/*Bit definition of CFB register*/
typedef enum{
	CFB_COSRANGE_Mask =	0xC0,
	CFB_CFBSEL_Mask   =	0x3F,	
	CFB_COS_BITRANGE_5 = 0x1F,
 	CFB_COS_BITRANGE_6 = 0x3F,
 	CFB_COS_BITRANGE_7 = 0x7F,
 	CFB_COS_BITRANGE_8 = 0xFF,
	
 	COS_RANGE_5BIT     = 0x00,
 	COS_RANGE_6BIT	   = 0x40,
 	COS_RANGE_7BIT	   = 0x80,
 	COS_RANGE_8BIT     = 0xC0, 
}cfb_config;

#endif /* HEAD_ONEWIRE_H_ */
