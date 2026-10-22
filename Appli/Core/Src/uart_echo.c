#include "uart_echo.h"
#include <string.h>

static HAL_StatusTypeDef UART_Echo_StartReceive(UART_EchoHandleTypeDef *echo)
{
    return HAL_UART_Receive_IT(echo->uart, echo->buffer, UART_ECHO_RX_LENGTH);
}

HAL_StatusTypeDef UART_Echo_Init(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart)
{
    if ((echo == NULL) || (uart == NULL))
    {
        return HAL_ERROR;
    }

    echo->uart = uart;
    echo->packetReady = 0U;
    echo->droppedBytes = 0U;
    echo->errorCount = 0U;
    echo->lastError = HAL_UART_ERROR_NONE;

    return UART_Echo_StartReceive(echo);
}

void UART_Echo_RxCpltCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart)
{
    if ((echo == NULL) || (uart != echo->uart))
    {
        return;
    }

    if (echo->packetReady == 0U)
    {
        memcpy(echo->txBuffer, echo->buffer, UART_ECHO_RX_LENGTH);
        echo->packetReady = 1U;
    }
    else
    {
        echo->droppedBytes += UART_ECHO_RX_LENGTH;
    }

    if (UART_Echo_StartReceive(echo) != HAL_OK)
    {
        echo->errorCount++;
        echo->lastError = HAL_UART_GetError(echo->uart);
    }
}

void UART_Echo_Process(UART_EchoHandleTypeDef *echo)
{
    if ((echo == NULL) || (echo->packetReady == 0U))
    {
        return;
    }

    if (HAL_UART_Transmit(echo->uart, echo->txBuffer, UART_ECHO_RX_LENGTH, UART_ECHO_TX_TIMEOUT_MS) != HAL_OK)
    {
        echo->droppedBytes += UART_ECHO_RX_LENGTH;
    }

    echo->packetReady = 0U;
}

void UART_Echo_ErrorCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart)
{
    if ((echo == NULL) || (uart != echo->uart))
    {
        return;
    }

    echo->lastError = HAL_UART_GetError(uart);
    echo->errorCount++;

    if (uart->RxState == HAL_UART_STATE_READY)
    {
        (void)UART_Echo_StartReceive(echo);
    }
}
