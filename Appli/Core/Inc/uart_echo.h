#ifndef UART_ECHO_H
#define UART_ECHO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7rsxx_hal.h"

#ifndef UART_ECHO_BUFFER_SIZE
#define UART_ECHO_BUFFER_SIZE 128U
#endif

typedef struct
{
    UART_HandleTypeDef *uart;
    uint8_t rxByte;
    uint8_t txByte;
    uint8_t buffer[UART_ECHO_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint8_t txBusy;
    volatile uint32_t droppedBytes;
} UART_EchoHandleTypeDef;

HAL_StatusTypeDef UART_Echo_Init(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart);
void UART_Echo_RxCpltCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart);
void UART_Echo_TxCpltCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart);
void UART_Echo_ErrorCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif

#endif /* UART_ECHO_H */
