#ifndef ETHERNET_TEST_H
#define ETHERNET_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7rsxx_hal.h"
#include <stdint.h>

#define ETH_LOOPBACK_MODE_MAC  1U
#define ETH_LOOPBACK_MODE_PHY  2U
#define ETH_LOOPBACK_MODE_RJ45 3U

#ifndef ETH_LOOPBACK_TEST_MODE
#define ETH_LOOPBACK_TEST_MODE ETH_LOOPBACK_MODE_RJ45
#endif

#ifndef ETH_LOOPBACK_RX_BUFFER_SIZE
#define ETH_LOOPBACK_RX_BUFFER_SIZE 96U
#endif

#ifndef ETH_LOOPBACK_FRAME_SIZE
#define ETH_LOOPBACK_FRAME_SIZE 60U
#endif

#ifndef ETH_LOOPBACK_ETHERTYPE
#define ETH_LOOPBACK_ETHERTYPE 0x88B5U
#endif

#ifndef ETH_LOOPBACK_TX_PERIOD_MS
#define ETH_LOOPBACK_TX_PERIOD_MS 1000U
#endif

#ifndef ETH_LOOPBACK_TX_TIMEOUT_MS
#define ETH_LOOPBACK_TX_TIMEOUT_MS 100U
#endif

#ifndef ETH_LOOPBACK_TEST_DURATION_MS
#define ETH_LOOPBACK_TEST_DURATION_MS 10000U
#endif

typedef enum
{
    ETH_TEST_NOT_READY = 0U,
    ETH_TEST_IDLE,
    ETH_TEST_RUNNING,
    ETH_TEST_PASS,
    ETH_TEST_FAULT
} ETH_TestResultTypeDef;

typedef struct
{
    uint8_t data[ETH_LOOPBACK_RX_BUFFER_SIZE];
    ETH_BufferTypeDef link;
    volatile uint8_t inUse;
} ETH_LoopbackRxBufferTypeDef;

typedef struct
{
    ETH_HandleTypeDef *eth;
    uint32_t lastTxTick;
    uint32_t sequence;
    volatile uint32_t initStatus;
    volatile uint32_t txAttempts;
    volatile uint32_t txSuccess;
    volatile uint32_t txErrors;
    volatile uint32_t rxFrames;
    volatile uint32_t rxPass;
    volatile uint32_t rxCompareErrors;
    volatile uint32_t rxMissingPackets;
    volatile uint32_t rxAllocErrors;
    volatile uint32_t lastTxSequence;
    volatile uint32_t lastRxSequence;
    volatile uint32_t rxExpectedSequence;
    volatile uint32_t lastRxLength;
    volatile uint32_t lastHalStatus;
    volatile uint32_t lastEthError;
    volatile uint32_t activeMode;
    volatile uint32_t phyAddress;
    volatile uint32_t phyId1;
    volatile uint32_t phyId2;
    volatile uint32_t phyBmcr;
    volatile uint32_t phyBmsr;
    volatile uint32_t phyLinkUp;
    volatile uint32_t phyLinkReadErrors;
    volatile uint32_t phyLinkChecks;
    volatile uint32_t result;
    volatile uint32_t startRequest;
    uint32_t testStartTick;
    uint32_t lastLinkCheckTick;
} ETH_LoopbackHandleTypeDef;

HAL_StatusTypeDef ETH_Loopback_Init(ETH_LoopbackHandleTypeDef *test,
                                    ETH_HandleTypeDef *eth);
HAL_StatusTypeDef ETH_Loopback_RequestStart(void);
ETH_TestResultTypeDef ETH_Loopback_GetResult(void);
void ETH_Loopback_Process(ETH_LoopbackHandleTypeDef *test);
void StartETHLoopbackTask(void *argument);

void HAL_ETH_RxAllocateCallback(uint8_t **buffer);
void HAL_ETH_RxLinkCallback(void **start, void **end,
                            uint8_t *buffer, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNET_TEST_H */
