#ifndef _INC_RS485
#define _INC_RS485

#include <ch32v20x.h>
#include <stdbool.h>

void RS485_Init(uint32_t baudrate);
void RS485_SendChar(char c);
void RS485_SendString(char *str);
char RS485_ReadChar(void);
bool RS485_LineReceived(void);
uint32_t RS485_ReadLine(char *buff, uint32_t size);
void RS485_Flush(void);

#endif // _INC_RS485