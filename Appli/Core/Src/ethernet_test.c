#include "ethernet_test.h"
#include "cmsis_os2.h"
#include <string.h>

#define LAN8742_BMCR                0U
#define LAN8742_BMSR                1U
#define LAN8742_PHYID1              2U
#define LAN8742_PHYID2              3U
#define LAN8742_PHYID1_VALUE        0x0007U
#define LAN8742_PHYID2_MASK         0xFFF0U
#define LAN8742_PHYID2_VALUE        0xC130U
#define LAN8742_BMCR_LOOPBACK       (1U << 14)
#define LAN8742_BMCR_SPEED_100M     (1U << 13)
#define LAN8742_BMCR_FULL_DUPLEX    (1U << 8)
#define LAN8742_BMCR_LOOPBACK_100FD (LAN8742_BMCR_LOOPBACK | \
                                     LAN8742_BMCR_SPEED_100M | \
                                     LAN8742_BMCR_FULL_DUPLEX)
#define LAN8742_BMCR_NORMAL_100FD   (LAN8742_BMCR_SPEED_100M | \
                                     LAN8742_BMCR_FULL_DUPLEX)

#if defined(__GNUC__) || defined(__ARMCC_VERSION)
#define ETH_NONCACHEABLE __attribute__((section("noncacheable_buffer"), aligned(32)))
#else
#define ETH_NONCACHEABLE
#endif

typedef struct
{
    ETH_LoopbackHandleTypeDef *test;
    ETH_LoopbackRxBufferTypeDef rxPool[ETH_RX_DESC_CNT + 1U];
    ETH_DMADescTypeDef rxDesc[ETH_RX_DESC_CNT];
    ETH_DMADescTypeDef txDesc[ETH_TX_DESC_CNT];
    uint8_t txFrame[ETH_LOOPBACK_FRAME_SIZE];
    uint8_t txSnapshot[ETH_LOOPBACK_FRAME_SIZE];
    uint8_t rxSnapshot[ETH_LOOPBACK_FRAME_SIZE];
} ETH_LoopbackContextTypeDef;

static ETH_LoopbackContextTypeDef context ETH_NONCACHEABLE;

static void ETH_Loopback_ReleaseRxBuffers(ETH_BufferTypeDef *buffer)
{
    ETH_BufferTypeDef *next;

    while (buffer != NULL)
    {
        next = buffer->next;
        for (uint32_t index = 0U; index < ETH_RX_DESC_CNT + 1U; index++)
        {
            if (&context.rxPool[index].link == buffer)
            {
                context.rxPool[index].inUse = 0U;
                break;
            }
        }
        buffer = next;
    }
}

static void ETH_Loopback_BuildFrame(uint8_t *frame, uint32_t sequence)
{
    static const uint8_t testPattern[] = "ETH_LOOPBACK";

    memset(frame, 0, ETH_LOOPBACK_FRAME_SIZE);
    memcpy(&frame[0], context.test->eth->Init.MACAddr, 6U);
    memcpy(&frame[6], context.test->eth->Init.MACAddr, 6U);
    frame[12] = (uint8_t)(ETH_LOOPBACK_ETHERTYPE >> 8);
    frame[13] = (uint8_t)ETH_LOOPBACK_ETHERTYPE;
    memcpy(&frame[14], testPattern, sizeof(testPattern));
    memcpy(&frame[32], &sequence, sizeof(sequence));
}

static HAL_StatusTypeDef ETH_Loopback_CommonInit(ETH_MACConfigTypeDef *macConfig)
{
    HAL_StatusTypeDef status;

    context.test->eth->Init.RxDesc = context.rxDesc;
    context.test->eth->Init.TxDesc = context.txDesc;
    context.test->eth->Init.RxBuffLen = ETH_LOOPBACK_RX_BUFFER_SIZE;
    status = HAL_ETH_Init(context.test->eth);
    memset(context.rxPool, 0, sizeof(context.rxPool));

    if (status == HAL_OK)
    {
        status = HAL_ETH_GetMACConfig(context.test->eth, macConfig);
    }

    return status;
}

static void ETH_Loopback_SaveInitStatus(HAL_StatusTypeDef status)
{
    context.test->initStatus = status;
    context.test->lastHalStatus = status;
    context.test->lastEthError = context.test->eth->ErrorCode;
    context.test->lastTxTick = HAL_GetTick();
}

static HAL_StatusTypeDef ETH_Loopback_FindPhyAddress(uint32_t *phyAddress)
{
    uint32_t id1;
    uint32_t id2;

    for (uint32_t address = 0U; address <= 1U; address++)
    {
        if ((HAL_ETH_ReadPHYRegister(context.test->eth, address, LAN8742_PHYID1, &id1) == HAL_OK) &&
            (HAL_ETH_ReadPHYRegister(context.test->eth, address, LAN8742_PHYID2, &id2) == HAL_OK) &&
            (id1 == LAN8742_PHYID1_VALUE) &&
            ((id2 & LAN8742_PHYID2_MASK) == LAN8742_PHYID2_VALUE))
        {
            *phyAddress = address;
            context.test->phyId1 = id1;
            context.test->phyId2 = id2;
            return HAL_OK;
        }
    }

    return HAL_ERROR;
}

static HAL_StatusTypeDef ETH_MAC_Loopback_Init(void)
{
    ETH_MACConfigTypeDef macConfig;
    HAL_StatusTypeDef status = ETH_Loopback_CommonInit(&macConfig);

    if (status == HAL_OK)
    {
        macConfig.LoopbackMode = ENABLE;
        macConfig.DuplexMode = ETH_FULLDUPLEX_MODE;
        status = HAL_ETH_SetMACConfig(context.test->eth, &macConfig);
    }
    if (status == HAL_OK)
    {
        status = HAL_ETH_Start(context.test->eth);
    }

    context.test->activeMode = ETH_LOOPBACK_MODE_MAC;
    ETH_Loopback_SaveInitStatus(status);
    return status;
}

static HAL_StatusTypeDef ETH_PHY_Loopback_Init(void)
{
    ETH_MACConfigTypeDef macConfig;
    uint32_t phyAddress;
    uint32_t bmcr;
    HAL_StatusTypeDef status = ETH_Loopback_CommonInit(&macConfig);

    if (status == HAL_OK)
    {
        status = ETH_Loopback_FindPhyAddress(&phyAddress);
    }
    if (status == HAL_OK)
    {
        context.test->phyAddress = phyAddress;
        macConfig.LoopbackMode = DISABLE;
        macConfig.Speed = ETH_SPEED_100M;
        macConfig.DuplexMode = ETH_FULLDUPLEX_MODE;
        status = HAL_ETH_SetMACConfig(context.test->eth, &macConfig);
    }
    if (status == HAL_OK)
    {
        status = HAL_ETH_WritePHYRegister(context.test->eth, phyAddress, LAN8742_BMCR,
                                          LAN8742_BMCR_LOOPBACK_100FD);
    }
    if (status == HAL_OK)
    {
        status = HAL_ETH_ReadPHYRegister(context.test->eth, phyAddress, LAN8742_BMCR, &bmcr);
        context.test->phyBmcr = bmcr;
    }
    if ((status == HAL_OK) &&
        ((context.test->phyBmcr & LAN8742_BMCR_LOOPBACK_100FD) != LAN8742_BMCR_LOOPBACK_100FD))
    {
        status = HAL_ERROR;
    }
    if (status == HAL_OK)
    {
        status = HAL_ETH_Start(context.test->eth);
    }

    context.test->activeMode = ETH_LOOPBACK_MODE_PHY;
    ETH_Loopback_SaveInitStatus(status);
    return status;
}

static HAL_StatusTypeDef ETH_RJ45_Loopback_Init(void)
{
    ETH_MACConfigTypeDef macConfig;
    uint32_t phyAddress;
    uint32_t bmcr;
    HAL_StatusTypeDef status = ETH_Loopback_CommonInit(&macConfig);

    if (status == HAL_OK)
    {
        status = ETH_Loopback_FindPhyAddress(&phyAddress);
    }
    if (status == HAL_OK)
    {
        context.test->phyAddress = phyAddress;
        macConfig.LoopbackMode = DISABLE;
        macConfig.Speed = ETH_SPEED_100M;
        macConfig.DuplexMode = ETH_FULLDUPLEX_MODE;
        status = HAL_ETH_SetMACConfig(context.test->eth, &macConfig);
    }
    if (status == HAL_OK)
    {
        status = HAL_ETH_WritePHYRegister(context.test->eth, phyAddress, LAN8742_BMCR,
                                          LAN8742_BMCR_NORMAL_100FD);
        HAL_Delay(330U);
    }
    if (status == HAL_OK)
    {
        status = HAL_ETH_ReadPHYRegister(context.test->eth, phyAddress, LAN8742_BMCR, &bmcr);
        context.test->phyBmcr = bmcr;
    }
    if ((status == HAL_OK) &&
        ((context.test->phyBmcr & (LAN8742_BMCR_LOOPBACK | LAN8742_BMCR_SPEED_100M |
                                   LAN8742_BMCR_FULL_DUPLEX)) != LAN8742_BMCR_NORMAL_100FD))
    {
        status = HAL_ERROR;
    }
    if (status == HAL_OK)
    {
        status = HAL_ETH_Start(context.test->eth);
    }

    context.test->activeMode = ETH_LOOPBACK_MODE_RJ45;
    ETH_Loopback_SaveInitStatus(status);
    return status;
}

static void ETH_RJ45_UpdateLinkStatus(void)
{
    uint32_t firstStatus;
    uint32_t secondStatus;

    if (context.test->phyAddress > 31U)
    {
        return;
    }

    context.test->phyLinkChecks++;
    if ((HAL_ETH_ReadPHYRegister(context.test->eth, context.test->phyAddress, LAN8742_BMSR, &firstStatus) != HAL_OK) ||
        (HAL_ETH_ReadPHYRegister(context.test->eth, context.test->phyAddress, LAN8742_BMSR, &secondStatus) != HAL_OK))
    {
        context.test->phyLinkReadErrors++;
        context.test->phyLinkUp = 0U;
        return;
    }

    context.test->phyBmsr = secondStatus;
    context.test->phyLinkUp = ((secondStatus & (1U << 2U)) != 0U) ? 1U : 0U;
}

static HAL_StatusTypeDef ETH_Loopback_Transmit(void)
{
    ETH_BufferTypeDef txBuffer = {0};
    ETH_TxPacketConfigTypeDef txConfig = {0};
    HAL_StatusTypeDef status;

    ETH_Loopback_BuildFrame(context.txFrame, context.test->sequence);
    memcpy(context.txSnapshot, context.txFrame, ETH_LOOPBACK_FRAME_SIZE);

    txBuffer.buffer = context.txFrame;
    txBuffer.len = sizeof(context.txFrame);
    txConfig.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
    txConfig.Length = sizeof(context.txFrame);
    txConfig.TxBuffer = &txBuffer;
    txConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    context.test->txAttempts++;
    context.test->lastTxSequence = context.test->sequence;
    status = HAL_ETH_Transmit(context.test->eth, &txConfig, ETH_LOOPBACK_TX_TIMEOUT_MS);
    context.test->lastHalStatus = status;
    context.test->lastEthError = context.test->eth->ErrorCode;
    if (status == HAL_OK)
    {
        context.test->txSuccess++;
        context.test->sequence++;
    }
    else
    {
        context.test->txErrors++;
    }

    return status;
}

static void ETH_Loopback_ProcessRx(void)
{
    ETH_BufferTypeDef *packet = NULL;
    uint32_t receivedSequence;

    while (HAL_ETH_ReadData(context.test->eth, (void **)&packet) == HAL_OK)
    {
        context.test->rxFrames++;
        context.test->lastRxLength = packet->len;

        if ((packet->next == NULL) && (packet->len == ETH_LOOPBACK_FRAME_SIZE))
        {
            memcpy(context.rxSnapshot, packet->buffer, ETH_LOOPBACK_FRAME_SIZE);
            memcpy(&receivedSequence, &context.rxSnapshot[32], sizeof(receivedSequence));
            context.test->lastRxSequence = receivedSequence;

            if (memcmp(context.rxSnapshot, context.txSnapshot, ETH_LOOPBACK_FRAME_SIZE) != 0)
            {
                context.test->rxCompareErrors++;
            }
            else if (receivedSequence == context.test->rxExpectedSequence)
            {
                context.test->rxPass++;
                context.test->rxExpectedSequence++;
            }
            else if (receivedSequence > context.test->rxExpectedSequence)
            {
                context.test->rxMissingPackets += receivedSequence - context.test->rxExpectedSequence;
                context.test->rxCompareErrors++;
                context.test->rxExpectedSequence = receivedSequence + 1U;
            }
            else
            {
                context.test->rxCompareErrors++;
            }
        }
        else
        {
            context.test->rxCompareErrors++;
        }

        ETH_Loopback_ReleaseRxBuffers(packet);
        packet = NULL;
    }
}

HAL_StatusTypeDef ETH_Loopback_Init(ETH_LoopbackHandleTypeDef *test,
                                    ETH_HandleTypeDef *eth)
{
    HAL_StatusTypeDef status;

    if ((test == NULL) || (eth == NULL))
    {
        return HAL_ERROR;
    }

    memset(test, 0, sizeof(*test));
    test->eth = eth;
    test->phyAddress = 0xFFFFFFFFU;
    context.test = test;

#if ETH_LOOPBACK_TEST_MODE == ETH_LOOPBACK_MODE_MAC
    status = ETH_MAC_Loopback_Init();
#elif ETH_LOOPBACK_TEST_MODE == ETH_LOOPBACK_MODE_PHY
    status = ETH_PHY_Loopback_Init();
#elif ETH_LOOPBACK_TEST_MODE == ETH_LOOPBACK_MODE_RJ45
    status = ETH_RJ45_Loopback_Init();
#else
#error "Invalid ETH_LOOPBACK_TEST_MODE"
#endif

    return status;
}

void ETH_Loopback_Process(ETH_LoopbackHandleTypeDef *test)
{
    uint32_t now;

    if ((test == NULL) || (test != context.test))
    {
        return;
    }

    now = HAL_GetTick();
    ETH_Loopback_ProcessRx();
    if ((test->activeMode == ETH_LOOPBACK_MODE_RJ45) &&
        ((now - context.test->lastTxTick) >= 100U))
    {
        ETH_RJ45_UpdateLinkStatus();
    }
    if ((now - test->lastTxTick) >= ETH_LOOPBACK_TX_PERIOD_MS)
    {
        test->lastTxTick = now;
        if ((test->activeMode != ETH_LOOPBACK_MODE_RJ45) || (test->phyLinkUp != 0U))
        {
            (void)ETH_Loopback_Transmit();
        }
    }
}

void StartETHLoopbackTask(void *argument)
{
    ETH_LoopbackHandleTypeDef *test = (ETH_LoopbackHandleTypeDef *)argument;

    for (;;)
    {
        ETH_Loopback_Process(test);
        osDelay(1U);
    }
}

void HAL_ETH_RxAllocateCallback(uint8_t **buffer)
{
    *buffer = NULL;
    for (uint32_t index = 0U; index < ETH_RX_DESC_CNT + 1U; index++)
    {
        if (context.rxPool[index].inUse == 0U)
        {
            context.rxPool[index].inUse = 1U;
            *buffer = context.rxPool[index].data;
            return;
        }
    }
    if (context.test != NULL)
    {
        context.test->rxAllocErrors++;
    }
}

void HAL_ETH_RxLinkCallback(void **start, void **end,
                            uint8_t *buffer, uint16_t length)
{
    ETH_BufferTypeDef *link = NULL;

    for (uint32_t index = 0U; index < ETH_RX_DESC_CNT + 1U; index++)
    {
        if (context.rxPool[index].data == buffer)
        {
            link = &context.rxPool[index].link;
            break;
        }
    }
    if (link == NULL)
    {
        if (context.test != NULL)
        {
            context.test->rxAllocErrors++;
        }
        return;
    }

    link->buffer = buffer;
    link->len = length;
    link->next = NULL;
    if (*start == NULL)
    {
        *start = link;
    }
    else
    {
        ((ETH_BufferTypeDef *)*end)->next = link;
    }
    *end = link;
}
