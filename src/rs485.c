#include "rs485.h"

static volatile char rxBuffer[0x40] = {0};
#define INDEX_MASK  (sizeof(rxBuffer) - 1)

static volatile uint32_t count = 0;
static volatile uint32_t writeIndex = 0;
static uint32_t readIndex = 0;

#define RECV_IRQ_ON()  NVIC_EnableIRQ(USART2_IRQn)
#define RECV_IRQ_OFF() NVIC_DisableIRQ(USART2_IRQn)

#define DIR_TRANSMITTER()  GPIO_WriteBit(GPIOA, GPIO_Pin_8, 1)
#define DIR_RECEIVER()  GPIO_WriteBit(GPIOA, GPIO_Pin_8, 0)

static inline bool BufferEmpty(void)
{
    return count == 0;
}

static inline bool BufferFull(void)
{
    return count == sizeof(rxBuffer);
}

static char PeekLast(void)
{
    if (BufferEmpty())
        return 0;

    uint8_t idx = (writeIndex - 1) & INDEX_MASK;
    return rxBuffer[idx];
}

static inline bool IsEndOfLine(char c)
{
    return c == '\n';
}

void RS485_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // RS485-TX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // RS485-RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);


    // RS485-DIR
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);
}

void RS485_SendChar(char c)
{
    DIR_TRANSMITTER();

    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
        ;

    USART_SendData(USART2, c);

    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
        ;

    DIR_RECEIVER();
}

void RS485_SendString(char *str)
{
    DIR_TRANSMITTER();

    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
        ;
    while (*str) {
        USART_SendData(USART2, *str++);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)
            ;
    }
    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
        ;

    DIR_RECEIVER();
}

__attribute__((interrupt("WCH-Interrupt-fast")))
void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        if (!BufferFull()) {
            rxBuffer[writeIndex++] = USART_ReceiveData(USART2);
            writeIndex &= INDEX_MASK;
            count++;
        } else {
            (void)USART_ReceiveData(USART2);
        }
    }
}

static inline char ReadChar(void)
{
    char c = '\0';

    if (!BufferEmpty()) {
        c = rxBuffer[readIndex++];
        readIndex &= INDEX_MASK;
        count--;
    }

    return c;
}

char RS485_ReadChar(void)
{
    RECV_IRQ_OFF();
    char c = ReadChar();
    RECV_IRQ_ON();
    return c;
}

bool RS485_LineReceived(void)
{
    char last = PeekLast();

    if (IsEndOfLine(last))
        return true;

    if (BufferFull())
        RS485_Flush();

    return false;
}

uint32_t RS485_ReadLine(char *buff, uint32_t size)
{
    if (!size)
        return 0;

    uint8_t len = 0;
    --size;  // for '\0'

    if (!RS485_LineReceived())
        goto out;

    RECV_IRQ_OFF();

    char c = ReadChar();

    while (!IsEndOfLine(c) && size--) {
        *buff++ = c;
        len++;
        c = ReadChar();
    }

    RECV_IRQ_ON();

out:
    *buff = '\0';
    return len;
}

void RS485_Flush(void)
{
    RECV_IRQ_OFF();
    readIndex = writeIndex = count = 0;
    RECV_IRQ_ON();
}