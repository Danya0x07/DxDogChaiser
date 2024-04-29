#include <ch32v20x.h>
#include <debug.h>

#include "isp_console.h"
#include "rs485.h"

volatile bool canMsgPending = false;

void InitCan(void)
{
    GPIO_InitTypeDef GPIO_InitSturct = {0};
    CAN_InitTypeDef CAN_InitStruct = {0};
    CAN_FilterInitTypeDef CAN_FilterInitStruct = {0};
    NVIC_InitTypeDef NVIC_InitStruct;

    GPIO_InitSturct.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitSturct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitSturct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitSturct);

    GPIO_InitSturct.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitSturct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitSturct);

    CAN_InitStruct.CAN_Prescaler = 6;
    CAN_InitStruct.CAN_Mode = CAN_Mode_Normal;
    CAN_InitStruct.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStruct.CAN_BS1 = CAN_BS1_13tq;
    CAN_InitStruct.CAN_BS2 = CAN_BS2_2tq;
    CAN_InitStruct.CAN_TTCM = DISABLE;
    CAN_InitStruct.CAN_ABOM = ENABLE;
    CAN_InitStruct.CAN_AWUM = ENABLE;
    CAN_InitStruct.CAN_NART = ENABLE;
    CAN_InitStruct.CAN_RFLM = ENABLE;
    CAN_InitStruct.CAN_TXFP = ENABLE;

    if (CAN_Init(CAN1, &CAN_InitStruct) != CAN_InitStatus_Success) {
	ISP_SendString("CAN init error\n");
    }

    CAN_FilterInitStruct.CAN_FilterIdHigh = 0x00D0 << 5;
    CAN_FilterInitStruct.CAN_FilterIdLow = 0x00D0 << 5;
    CAN_FilterInitStruct.CAN_FilterMaskIdHigh = 0x00D7 << 5 ;
    CAN_FilterInitStruct.CAN_FilterMaskIdLow = 0x00D7 << 5;
    CAN_FilterInitStruct.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    CAN_FilterInitStruct.CAN_FilterNumber = 0;
    CAN_FilterInitStruct.CAN_FilterMode = CAN_FilterMode_IdList;
    CAN_FilterInitStruct.CAN_FilterScale = CAN_FilterScale_16bit;
    CAN_FilterInitStruct.CAN_FilterActivation = ENABLE;

    CAN_FilterInit(&CAN_FilterInitStruct);

    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

    NVIC_InitStruct.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}

void CAN_SendChar(char c)
{
    CanTxMsg msg;

    msg.StdId = 0x00D7;
    msg.IDE = CAN_Id_Standard;
    msg.RTR = CAN_RTR_Data;
    msg.DLC = 8;
    msg.Data[0] = c;

    CAN_Transmit(CAN1, &msg);
}

CanRxMsg rxmsg;

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemInit();
    SystemCoreClockUpdate();
    Delay_Init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB1Periph_CAN1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOA, ENABLE);

    Delay_Ms(100);


    ISP_Init(115200);
    InitCan();
    RS485_Init(1200);

    ISP_SendString("Begining\n");
    printf("Clock is: %ld\n", SystemCoreClock);
    RS485_SendString("Begining\n");

    for (;;) {
	if (ISP_LineReceived()) {
	    char line[64];
	    ISP_ReadLine(line, sizeof(line));
	    RS485_SendString(line);
	    RS485_SendChar('\n');

	    CAN_SendChar(line[0]);
	}
	if (RS485_LineReceived()) {
	    char line[64];
	    RS485_ReadLine(line, sizeof(line));
	    ISP_SendString(line);
	    ISP_SendChar('\n');
	}
	if (canMsgPending) {
	    printf("ReceivedCAN message with ID: %lx\n", rxmsg.StdId);
	    printf("IDE: %lx\tRTR: %lx\tDLC %lx\tFMI: %lx\n", rxmsg.IDE, rxmsg.RTR, rxmsg.DLC, rxmsg.FMI);
	    rxmsg.Data[7] = '\0';
	    ISP_SendString(rxmsg.Data);
	    ISP_SendString("\n\n");
	    canMsgPending = false;
	}
    }
}

__attribute__((interrupt("WCH-Interrupt-fast")))
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    if (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET) {
	canMsgPending = true;
	ISP_SendChar('R');
	CAN_Receive(CAN1, CAN_FIFO0, &rxmsg);
    }
}

__attribute__((interrupt("WCH-Interrupt-fast")))
void NMI_Handler(void)
{

}

__attribute__((interrupt("WCH-Interrupt-fast")))
void HardFault_Handler(void)
{
    for (;;) {

    }
}