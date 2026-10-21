#include "uart_echo.h"

static uint16_t UART_Echo_NextIndex(uint16_t index)
{
    index++;
    return (index >= UART_ECHO_BUFFER_SIZE) ? 0U : index;
}

static void UART_Echo_StartTransmit(UART_EchoHandleTypeDef *echo)
{
    if ((echo->txBusy != 0U) || (echo->head == echo->tail))
    {
        return;
    }

    echo->txByte = echo->buffer[echo->tail];
    echo->txBusy = 1U;
    if (HAL_UART_Transmit_IT(echo->uart, &echo->txByte, 1U) == HAL_OK)
    {
        echo->tail = UART_Echo_NextIndex(echo->tail);
    }
    else
    {
        echo->txBusy = 0U;
    }
}

HAL_StatusTypeDef UART_Echo_Init(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart)
{
    if ((echo == NULL) || (uart == NULL))
    {
        return HAL_ERROR;
    }

    echo->uart = uart;
    echo->head = 0U;
    echo->tail = 0U;
    echo->txBusy = 0U;
    echo->droppedBytes = 0U;

    return HAL_UART_Receive_IT(echo->uart, &echo->rxByte, 1U);
}

void UART_Echo_RxCpltCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart)
{
    if ((echo == NULL) || (uart != echo->uart))
    {
        return;
    }

    uint16_t nextHead = UART_Echo_NextIndex(echo->head);
    if (nextHead != echo->tail)
    {
        echo->buffer[echo->head] = echo->rxByte;
        echo->head = nextHead;
    }
    else
    {
        echo->droppedBytes++;
    }

    (void)HAL_UART_Receive_IT(echo->uart, &echo->rxByte, 1U);
    UART_Echo_StartTransmit(echo);
}

void UART_Echo_TxCpltCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart)
{
    if ((echo == NULL) || (uart != echo->uart))
    {
        return;
    }

    echo->txBusy = 0U;
    UART_Echo_StartTransmit(echo);
}

void UART_Echo_ErrorCallback(UART_EchoHandleTypeDef *echo, UART_HandleTypeDef *uart)
{
    if ((echo == NULL) || (uart != echo->uart))
    {
        return;
    }

    (void)HAL_UART_Receive_IT(echo->uart, &echo->rxByte, 1U);
    UART_Echo_StartTransmit(echo);
}
