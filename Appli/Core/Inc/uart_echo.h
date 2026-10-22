#ifndef UART_ECHO_H
#define UART_ECHO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7rsxx_hal.h"

#ifndef UART_ECHO_BUFFER_SIZE
#define UART_ECHO_BUFFER_SIZE 128U
#endif

#ifndef UART_ECHO_RX_LENGTH
#define UART_ECHO_RX_LENGTH 6U
#endif

#if UART_ECHO_RX_LENGTH > UART_ECHO_BUFFER_SIZE
#error "UART_ECHO_RX_LENGTH must not exceed UART_ECHO_BUFFER_SIZE"
#endif

#ifndef UART_ECHO_TX_TIMEOUT_MS
#define UART_ECHO_TX_TIMEOUT_MS 100U
#endif

typedef struct
{
    UART_HandleTypeDef *uart;
    uint8_t buffer[UART_ECHO_BUFFER_SIZE];
    uint8_t txBuffer[UART_ECHO_RX_LENGTH];
    volatile uint8_t packetReady;
    volatile uint32_t droppedBytes;
    volatile uint32_t errorCount;
    volatile uint32_t lastError;
} UART_EchoHandleTypeDef;

HAL_StatusTypeDef UART_Echo_Init(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart);
void UART_Echo_RxCpltCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart);
void UART_Echo_ErrorCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart);
void UART_Echo_Process(UART_EchoHandleTypeDef *echo);

#ifdef __cplusplus
}
#endif

#endif /* UART_ECHO_H */
