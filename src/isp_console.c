#include "isp_console.h"

static volatile char rxBuffer[0x40] = {0};
#define INDEX_MASK  (sizeof(rxBuffer) - 1)

static volatile uint32_t count = 0;
static volatile uint32_t writeIndex = 0;
static uint32_t readIndex = 0;

#define RECV_IRQ_ON()  NVIC_EnableIRQ(USART1_IRQn)
#define RECV_IRQ_OFF() NVIC_DisableIRQ(USART1_IRQn)

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

void ISP_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);
}

void ISP_SendChar(char c)
{
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        ;
    USART_SendData(USART1, c);
}

void ISP_SendString(char *str)
{
    while (*str)
        ISP_SendChar(*str++);
}

__attribute__((interrupt("WCH-Interrupt-fast")))
void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        if (!BufferFull()) {
            rxBuffer[writeIndex++] = USART_ReceiveData(USART1);
            writeIndex &= INDEX_MASK;
            count++;
        } else {
            (void)USART_ReceiveData(USART1);
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

char ISP_ReadChar(void)
{
    RECV_IRQ_OFF();
    char c = ReadChar();
    RECV_IRQ_ON();
    return c;
}

bool ISP_LineReceived(void)
{
    char last = PeekLast();

    if (IsEndOfLine(last))
        return true;

    if (BufferFull())
        ISP_Flush();

    return false;
}

uint32_t ISP_ReadLine(char *buff, uint32_t size)
{
    if (!size)
        return 0;

    uint8_t len = 0;
    --size;  // for '\0'

    if (!ISP_LineReceived())
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

void ISP_Flush(void)
{
    RECV_IRQ_OFF();
    readIndex = writeIndex = count = 0;
    RECV_IRQ_ON();
}