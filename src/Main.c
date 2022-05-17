/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : 串口1收发演示
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#include <onewire.h>
#include "CH58x_common.h"

uint8_t TxBuff[] = "This is a tx exam\r\n";
uint8_t RxBuff[100];
uint8_t trigB;

/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */
int main()
{
	volatile int j,i=0;
	unsigned char b[3];
	unsigned char a[5];
	uint8_t flag=0;
	float Cmin=0, Cmax=30;
    SetSysClock(CLK_SOURCE_PLL_60MHz);

    /* 配置串口1：先配置IO口模式，再配置串口 */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);      // RXD-配置上拉输入
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA); // TXD-配置推挽输出，注意先让IO口输出高电平
    UART1_DefInit();

	slave_init();
	read_command();
//	onewire_writebyte(0xBD);

////	i=onewire_readbit();
////	PRINT("%d\r\n",i);
//	while(!onewire_readbit());

	mdc02_channel(CAP_CH1CH2_SEL);
	mdc02_range(Cmin,Cmax);
//	mdc02_offset();
//	onewire_finddevic_fun();
//	DelayMs(1000);
	while(1){
//	    onewire_resetcheck();
//		read_command();
//		onewire_writebyte(0x66);
//		while(!onewire_readbit());
//		onewire_resetcheck();
//		read_command();
//		onewire_writebyte(0xBD);
//		for(j = 0;j < 5;j++){
//			a[j]=onewire_read();
//			PRINT("%.2X\t", a[j]);
//		}
//		PRINT("\r\n");
//		if(onewire_crc8(&a[0],4) != a[4]){
//			PRINT(" crc error\r\n");
//		}
////		
		read_cap();
			
		__nop();
	};
}

/*********************************************************************
 * @fn      UART1_IRQHandler
 *
 * @brief   UART1中断函数
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void UART1_IRQHandler(void)
{
    volatile uint8_t i;

    switch(UART1_GetITFlag())
    {
        case UART_II_LINE_STAT: // 线路状态错误
        {
            UART1_GetLinSTA();
            break;
        }

        case UART_II_RECV_RDY: // 数据达到设置触发点
            for(i = 0; i != trigB; i++)
            {
                RxBuff[i] = UART1_RecvByte();
                UART1_SendByte(RxBuff[i]);
            }
            break;

        case UART_II_RECV_TOUT: // 接收超时，暂时一帧数据接收完成
            i = UART1_RecvString(RxBuff);
            UART1_SendString(RxBuff, i);
            break;

        case UART_II_THR_EMPTY: // 发送缓存区空，可继续发送
            break;

        case UART_II_MODEM_CHG: // 只支持串口0
            break;

        default:
            break;
    }
}
