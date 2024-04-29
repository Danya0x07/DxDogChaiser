#ifndef _INC_ISP_CONSOLE
#define _INC_ISP_CONSOLE

#include <ch32v20x.h>
#include <stdbool.h>

void ISP_Init(uint32_t baudrate);
void ISP_SendChar(char c);
void ISP_SendString(char *str);
char ISP_ReadChar(void);
bool ISP_LineReceived(void);
uint32_t ISP_ReadLine(char *buff, uint32_t size);
void ISP_Flush(void);

#endif // _INC_ISP_CONSOLE
