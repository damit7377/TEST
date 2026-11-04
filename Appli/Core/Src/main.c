/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "string.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "app_touchgfx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "uart_echo.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define ETH_LOOPBACK_RX_BUFFER_SIZE 96U

typedef struct
{
  uint8_t data[ETH_LOOPBACK_RX_BUFFER_SIZE];
  ETH_BufferTypeDef link;
  volatile uint8_t inUse;
} ETH_LoopbackRxBufferTypeDef;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LCD_WIDTH       1024U
#define LCD_HEIGHT      600U
#define LCD_FB_ADDR     ((uintptr_t)__lcd_framebuffer_start__)
#define RGB565_BLUE     0x001FU
#define RGB565_GREEN    0x07E0U
#define RGB565_RED      0xF800U
#define RGB565_WHITE    0xFFFFU

#define ETH_LOOPBACK_FRAME_SIZE       60U
#define ETH_LOOPBACK_RX_POOL_SIZE     (ETH_RX_DESC_CNT + 1U)
#define ETH_LOOPBACK_ETHERTYPE        0x88B5U
#define ETH_LOOPBACK_TX_PERIOD_MS     1000U
#define ETH_LOOPBACK_TX_TIMEOUT_MS    100U
#define ETH_LOOPBACK_MODE_MAC         1U
#define ETH_LOOPBACK_MODE_PHY         2U
#define ETH_LOOPBACK_MODE_RJ45        3U
#ifndef ETH_LOOPBACK_TEST_MODE
#define ETH_LOOPBACK_TEST_MODE        ETH_LOOPBACK_MODE_RJ45
#endif

#define LAN8742_BMCR                  0U
#define LAN8742_BMSR                  1U
#define LAN8742_PHYID1                2U
#define LAN8742_PHYID2                3U
#define LAN8742_PHYID1_VALUE          0x0007U
#define LAN8742_PHYID2_MASK           0xFFF0U
#define LAN8742_PHYID2_VALUE          0xC130U
#define LAN8742_BMCR_LOOPBACK         (1U << 14)
#define LAN8742_BMCR_SPEED_100M       (1U << 13)
#define LAN8742_BMCR_FULL_DUPLEX      (1U << 8)
#define LAN8742_BMCR_LOOPBACK_100FD   (LAN8742_BMCR_LOOPBACK | \
                                       LAN8742_BMCR_SPEED_100M | \
                                       LAN8742_BMCR_FULL_DUPLEX)
#define LAN8742_BMCR_NORMAL_100FD     (LAN8742_BMCR_SPEED_100M | \
                                       LAN8742_BMCR_FULL_DUPLEX)

/* Backlight control definitions (from ref_code) */
#define MAX_BRIGHTNESS_DUTY    92
#define MIN_BRIGHTNESS_DUTY    20
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
Touch_t Touch;
volatile uint8_t CTP_InterruptPending;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma location=0x24020000
ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
#pragma location=0x24020080
ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __CC_ARM )  /* MDK ARM Compiler */

__attribute__((at(0x24020000))) ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
__attribute__((at(0x24020080))) ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif (defined ( __GNUC__ ) || defined ( __ARMCC_VERSION )) /* GNU Compiler */

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDescripSection"))); /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDescripSection")));   /* Ethernet Tx DMA Descriptors */
#endif

ETH_TxPacketConfigTypeDef TxConfig;

CRC_HandleTypeDef hcrc;

DMA2D_HandleTypeDef hdma2d;

ETH_HandleTypeDef heth;

GPU2D_HandleTypeDef hgpu2d;

I2C_HandleTypeDef hi2c1;

LTDC_HandleTypeDef hltdc;

TIM_HandleTypeDef htim13;

UART_HandleTypeDef huart5;
UART_HandleTypeDef huart8;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TouchGFXTask */
osThreadId_t TouchGFXTaskHandle;
const osThreadAttr_t TouchGFXTask_attributes = {
  .name = "TouchGFXTask",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
extern uint8_t __lcd_framebuffer_start__[];
static ETH_DMADescTypeDef ethLoopbackRxDesc[ETH_RX_DESC_CNT]
  __attribute__((section("noncacheable_buffer"), aligned(32)));
static ETH_DMADescTypeDef ethLoopbackTxDesc[ETH_TX_DESC_CNT]
  __attribute__((section("noncacheable_buffer"), aligned(32)));
static ETH_LoopbackRxBufferTypeDef ethLoopbackRxPool[ETH_LOOPBACK_RX_POOL_SIZE]
  __attribute__((section("noncacheable_buffer"), aligned(32)));
static uint8_t ethLoopbackTxFrame[ETH_LOOPBACK_FRAME_SIZE]
  __attribute__((section("noncacheable_buffer"), aligned(32)));
static uint32_t ethLoopbackLastTxTick;
static uint32_t ethLoopbackSequence;
volatile uint32_t ethLoopbackInitStatus;
volatile uint32_t ethLoopbackTxAttempts;
volatile uint32_t ethLoopbackTxSuccess;
volatile uint32_t ethLoopbackTxErrors;
volatile uint32_t ethLoopbackRxFrames;
volatile uint32_t ethLoopbackRxPass;
volatile uint32_t ethLoopbackRxCompareErrors;
volatile uint32_t ethLoopbackRxMissingPackets;
volatile uint32_t ethLoopbackRxAllocErrors;
volatile uint32_t ethLoopbackLastTxSequence;
volatile uint32_t ethLoopbackLastRxSequence;
volatile uint32_t ethLoopbackRxExpectedSequence;
volatile uint32_t ethLoopbackLastRxLength;
volatile uint32_t ethLoopbackLastHalStatus;
volatile uint32_t ethLoopbackLastEthError;
volatile uint32_t ethLoopbackActiveMode;
volatile uint32_t ethPhyAddress = 0xFFFFFFFFU;
volatile uint32_t ethPhyId1;
volatile uint32_t ethPhyId2;
volatile uint32_t ethPhyBmcr;
volatile uint32_t ethPhyBmsr;
volatile uint32_t ethPhyLinkUp;
volatile uint32_t ethPhyLinkReadErrors;
volatile uint32_t ethPhyLinkChecks;
static uint32_t ethPhyLastLinkCheckTick;
static UART_EchoHandleTypeDef uart5Echo;
static UART_EchoHandleTypeDef uart8Echo;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_CRC_Init(void);
static void MX_DMA2D_Init(void);
static void MX_LTDC_Init(void);
static void MX_GPU2D_Init(void);
static void MX_TIM13_Init(void);
static void MX_ICACHE_GPU2D_Init(void);
static void MX_FLASH_Init(void);
static void MX_I2C1_Init(void);
static void MX_UART5_Init(void);
static void MX_UART8_Init(void);
static void MX_ETH_Init(void);
void StartDefaultTask(void *argument);
extern void TouchGFX_Task(void *argument);

/* USER CODE BEGIN PFP */
static void LCD_FillColor(uint16_t color);
static void APP_FillFrameBuffer(uint16_t color);
static void APP_RunColorCycleTest(void);
static HAL_StatusTypeDef ETH_MAC_Loopback_Init(void);
static HAL_StatusTypeDef ETH_PHY_Loopback_Init(void);
static void ETH_Loopback_Process(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void ETH_Loopback_ReleaseRxBuffers(ETH_BufferTypeDef *buffer)
{
  ETH_BufferTypeDef *next;

  while (buffer != NULL)
  {
    next = buffer->next;
    for (uint32_t index = 0U; index < ETH_LOOPBACK_RX_POOL_SIZE; index++)
    {
      if (&ethLoopbackRxPool[index].link == buffer)
      {
        ethLoopbackRxPool[index].inUse = 0U;
        break;
      }
    }
    buffer = next;
  }
}

static HAL_StatusTypeDef ETH_Loopback_Transmit(void)
{
  static const uint8_t testPattern[] = "ETH_LOOPBACK";
  ETH_BufferTypeDef txBuffer = {0};
  ETH_TxPacketConfigTypeDef txConfig = {0};
  HAL_StatusTypeDef status;

  memset(ethLoopbackTxFrame, 0, sizeof(ethLoopbackTxFrame));
  memcpy(&ethLoopbackTxFrame[0], heth.Init.MACAddr, 6U);
  memcpy(&ethLoopbackTxFrame[6], heth.Init.MACAddr, 6U);
  ethLoopbackTxFrame[12] = (uint8_t)(ETH_LOOPBACK_ETHERTYPE >> 8);
  ethLoopbackTxFrame[13] = (uint8_t)ETH_LOOPBACK_ETHERTYPE;
  memcpy(&ethLoopbackTxFrame[14], testPattern, sizeof(testPattern));
  memcpy(&ethLoopbackTxFrame[32], &ethLoopbackSequence, sizeof(ethLoopbackSequence));

  txBuffer.buffer = ethLoopbackTxFrame;
  txBuffer.len = sizeof(ethLoopbackTxFrame);
  txConfig.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
  txConfig.Length = sizeof(ethLoopbackTxFrame);
  txConfig.TxBuffer = &txBuffer;
  txConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

  ethLoopbackTxAttempts++;
  ethLoopbackLastTxSequence = ethLoopbackSequence;
  status = HAL_ETH_Transmit(&heth, &txConfig, ETH_LOOPBACK_TX_TIMEOUT_MS);
  ethLoopbackLastHalStatus = status;
  ethLoopbackLastEthError = heth.ErrorCode;
  if (status == HAL_OK)
  {
    ethLoopbackTxSuccess++;
    ethLoopbackSequence++;
  }
  else
  {
    ethLoopbackTxErrors++;
  }

  return status;
}

static void ETH_Loopback_ProcessRx(void)
{
  static const uint8_t testPattern[] = "ETH_LOOPBACK";
  ETH_BufferTypeDef *packet = NULL;
  uint32_t receivedSequence;

  while (HAL_ETH_ReadData(&heth, (void **)&packet) == HAL_OK)
  {
    ethLoopbackRxFrames++;
    ethLoopbackLastRxLength = packet->len;

    if ((packet->len >= 36U) &&
        (packet->buffer[12] == (uint8_t)(ETH_LOOPBACK_ETHERTYPE >> 8)) &&
        (packet->buffer[13] == (uint8_t)ETH_LOOPBACK_ETHERTYPE) &&
        (memcmp(&packet->buffer[14], testPattern, sizeof(testPattern)) == 0))
    {
      memcpy(&receivedSequence, &packet->buffer[32], sizeof(receivedSequence));
      ethLoopbackLastRxSequence = receivedSequence;
      if (receivedSequence == ethLoopbackRxExpectedSequence)
      {
        ethLoopbackRxPass++;
        ethLoopbackRxExpectedSequence++;
      }
      else if (receivedSequence > ethLoopbackRxExpectedSequence)
      {
        ethLoopbackRxMissingPackets += receivedSequence - ethLoopbackRxExpectedSequence;
        ethLoopbackRxCompareErrors++;
        ethLoopbackRxExpectedSequence = receivedSequence + 1U;
      }
      else
      {
        ethLoopbackRxCompareErrors++;
      }
    }
    else
    {
      ethLoopbackRxCompareErrors++;
    }

    ETH_Loopback_ReleaseRxBuffers(packet);
    packet = NULL;
  }
}

static HAL_StatusTypeDef ETH_Loopback_CommonInit(ETH_MACConfigTypeDef *macConfig)
{
  HAL_StatusTypeDef status;

  heth.Init.RxDesc = ethLoopbackRxDesc;
  heth.Init.TxDesc = ethLoopbackTxDesc;
  heth.Init.RxBuffLen = ETH_LOOPBACK_RX_BUFFER_SIZE;
  status = HAL_ETH_Init(&heth);

  memset(ethLoopbackRxPool, 0, sizeof(ethLoopbackRxPool));

  if (status == HAL_OK)
  {
    status = HAL_ETH_GetMACConfig(&heth, macConfig);
  }

  return status;
}

static void ETH_Loopback_SaveInitStatus(HAL_StatusTypeDef status)
{
  ethLoopbackInitStatus = status;
  ethLoopbackLastHalStatus = status;
  ethLoopbackLastEthError = heth.ErrorCode;
  ethLoopbackLastTxTick = HAL_GetTick();
}

static HAL_StatusTypeDef ETH_MAC_Loopback_Init(void)
{
  ETH_MACConfigTypeDef macConfig;
  HAL_StatusTypeDef status = ETH_Loopback_CommonInit(&macConfig);

  if (status == HAL_OK)
  {
    macConfig.LoopbackMode = ENABLE;
    macConfig.DuplexMode = ETH_FULLDUPLEX_MODE;
    status = HAL_ETH_SetMACConfig(&heth, &macConfig);
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_Start(&heth);
  }

  ethLoopbackActiveMode = ETH_LOOPBACK_MODE_MAC;
  ETH_Loopback_SaveInitStatus(status);
  return status;
}

static HAL_StatusTypeDef ETH_PHY_FindAddress(uint32_t *phyAddress)
{
  uint32_t id1;
  uint32_t id2;

  for (uint32_t address = 0U; address <= 1U; address++)
  {
    if ((HAL_ETH_ReadPHYRegister(&heth, address, LAN8742_PHYID1, &id1) == HAL_OK) &&
        (HAL_ETH_ReadPHYRegister(&heth, address, LAN8742_PHYID2, &id2) == HAL_OK) &&
        (id1 == LAN8742_PHYID1_VALUE) &&
        ((id2 & LAN8742_PHYID2_MASK) == LAN8742_PHYID2_VALUE))
    {
      *phyAddress = address;
      ethPhyId1 = id1;
      ethPhyId2 = id2;
      return HAL_OK;
    }
  }

  return HAL_ERROR;
}

static HAL_StatusTypeDef ETH_PHY_Loopback_Init(void)
{
  ETH_MACConfigTypeDef macConfig;
  uint32_t phyAddress;
  uint32_t bmcr;
  HAL_StatusTypeDef status = ETH_Loopback_CommonInit(&macConfig);

  if (status == HAL_OK)
  {
    status = ETH_PHY_FindAddress(&phyAddress);
  }
  if (status == HAL_OK)
  {
    ethPhyAddress = phyAddress;
    macConfig.LoopbackMode = DISABLE;
    macConfig.Speed = ETH_SPEED_100M;
    macConfig.DuplexMode = ETH_FULLDUPLEX_MODE;
    status = HAL_ETH_SetMACConfig(&heth, &macConfig);
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_WritePHYRegister(&heth, phyAddress, LAN8742_BMCR,
                                      LAN8742_BMCR_LOOPBACK_100FD);
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_ReadPHYRegister(&heth, phyAddress, LAN8742_BMCR, &bmcr);
    ethPhyBmcr = bmcr;
  }
  if ((status == HAL_OK) &&
      ((ethPhyBmcr & LAN8742_BMCR_LOOPBACK_100FD) != LAN8742_BMCR_LOOPBACK_100FD))
  {
    status = HAL_ERROR;
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_Start(&heth);
  }

  ethLoopbackActiveMode = ETH_LOOPBACK_MODE_PHY;
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
    status = ETH_PHY_FindAddress(&phyAddress);
  }
  if (status == HAL_OK)
  {
    ethPhyAddress = phyAddress;
    macConfig.LoopbackMode = DISABLE;
    macConfig.Speed = ETH_SPEED_100M;
    macConfig.DuplexMode = ETH_FULLDUPLEX_MODE;
    status = HAL_ETH_SetMACConfig(&heth, &macConfig);
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_WritePHYRegister(&heth, phyAddress, LAN8742_BMCR,
                                      LAN8742_BMCR_NORMAL_100FD);
  }
  if (status == HAL_OK)
  {
    HAL_Delay(330U);
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_ReadPHYRegister(&heth, phyAddress, LAN8742_BMCR, &bmcr);
    ethPhyBmcr = bmcr;
  }
  if ((status == HAL_OK) &&
      ((ethPhyBmcr & (LAN8742_BMCR_LOOPBACK | LAN8742_BMCR_SPEED_100M |
                      LAN8742_BMCR_FULL_DUPLEX)) != LAN8742_BMCR_NORMAL_100FD))
  {
    status = HAL_ERROR;
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_ReadPHYRegister(&heth, phyAddress, LAN8742_BMSR, &ethPhyBmsr);
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_ReadPHYRegister(&heth, phyAddress, LAN8742_BMSR, &ethPhyBmsr);
    ethPhyLinkUp = ((ethPhyBmsr & (1U << 2U)) != 0U) ? 1U : 0U;
  }
  if (status == HAL_OK)
  {
    status = HAL_ETH_Start(&heth);
  }

  ethLoopbackActiveMode = ETH_LOOPBACK_MODE_RJ45;
  ethPhyLastLinkCheckTick = HAL_GetTick();
  ETH_Loopback_SaveInitStatus(status);
  return status;
}

static void ETH_RJ45_UpdateLinkStatus(void)
{
  uint32_t firstStatus;
  uint32_t secondStatus;

  if (ethPhyAddress > 31U)
  {
    return;
  }

  ethPhyLinkChecks++;
  if ((HAL_ETH_ReadPHYRegister(&heth, ethPhyAddress, LAN8742_BMSR, &firstStatus) != HAL_OK) ||
      (HAL_ETH_ReadPHYRegister(&heth, ethPhyAddress, LAN8742_BMSR, &secondStatus) != HAL_OK))
  {
    ethPhyLinkReadErrors++;
    ethPhyLinkUp = 0U;
    return;
  }

  ethPhyBmsr = secondStatus;
  ethPhyLinkUp = ((secondStatus & (1U << 2U)) != 0U) ? 1U : 0U;
}

static void ETH_Loopback_Process(void)
{
  uint32_t now = HAL_GetTick();

  ETH_Loopback_ProcessRx();
  if ((ethLoopbackActiveMode == ETH_LOOPBACK_MODE_RJ45) &&
      ((now - ethPhyLastLinkCheckTick) >= 100U))
  {
    ethPhyLastLinkCheckTick = now;
    ETH_RJ45_UpdateLinkStatus();
  }
  if ((now - ethLoopbackLastTxTick) >= ETH_LOOPBACK_TX_PERIOD_MS)
  {
    ethLoopbackLastTxTick = now;
    if ((ethLoopbackActiveMode != ETH_LOOPBACK_MODE_RJ45) || (ethPhyLinkUp != 0U))
    {
      (void)ETH_Loopback_Transmit();
    }
  }
}

static void Set_Brightness_Duty(uint8_t duty)
{
  duty = (duty > 100) ? 100 : duty;
  duty = 100 - duty;
  duty = ((MAX_BRIGHTNESS_DUTY - MIN_BRIGHTNESS_DUTY) * duty) / 100 + MIN_BRIGHTNESS_DUTY;
  __HAL_TIM_SET_COMPARE(&htim13, TIM_CHANNEL_1, duty);
}

static void APP_FillFrameBuffer(uint16_t color)
{
  volatile uint32_t *fb = (volatile uint32_t *)LCD_FB_ADDR;
  uint32_t twoPixels = ((uint32_t)color << 16) | color;

  for (uint32_t i = 0; i < (LCD_WIDTH * LCD_HEIGHT) / 2U; i++)
  {
    fb[i] = twoPixels;
  }
}

static void APP_RunColorCycleTest(void)
{
  static const uint16_t colors[] = {RGB565_RED, RGB565_GREEN, RGB565_BLUE};

  while (1)
  {
    for (uint32_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
    {
      APP_FillFrameBuffer(colors[i]);
      HAL_Delay(1000U);
    }
  }
}

static void LCD_FillColor(uint16_t color)
{
  HAL_GPIO_WritePin(TFT_NRST_GPIO_Port, TFT_NRST_Pin, GPIO_PIN_RESET);
  HAL_Delay(50);
  HAL_GPIO_WritePin(TFT_NRST_GPIO_Port, TFT_NRST_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_EN_GPIO_Port, TFT_EN_Pin, GPIO_PIN_RESET);
  HAL_Delay(900);
  HAL_GPIO_WritePin(TFT_EN_GPIO_Port, TFT_EN_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_SETB_GPIO_Port, TFT_SETB_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(TFT_LR_GPIO_Port, TFT_LR_Pin, GPIO_PIN_SET);
  HAL_Delay(200);
  HAL_GPIO_WritePin(BL_EN_GPIO_Port, BL_EN_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  Set_Brightness_Duty(100);
  HAL_TIM_PWM_Start(&htim13, TIM_CHANNEL_1);
  APP_FillFrameBuffer(color);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Update SystemCoreClock variable according to RCC registers values. */
  SystemCoreClockUpdate();

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CRC_Init();
  MX_DMA2D_Init();
  MX_LTDC_Init();
  MX_GPU2D_Init();
  MX_TIM13_Init();
  MX_ICACHE_GPU2D_Init();
  MX_FLASH_Init();
  MX_I2C1_Init();
  MX_UART5_Init();
  MX_UART8_Init();
  MX_ETH_Init();
  MX_TouchGFX_Init();
  /* Call PreOsInit function */
  MX_TouchGFX_PreOSInit();
  /* USER CODE BEGIN 2 */
  if (UART_Echo_Init(&uart5Echo, &huart5) != HAL_OK)
  {
    Error_Handler();
  }
  if (UART_Echo_Init(&uart8Echo, &huart8) != HAL_OK)
  {
    Error_Handler();
  }
#if ETH_LOOPBACK_TEST_MODE == ETH_LOOPBACK_MODE_MAC
  if (ETH_MAC_Loopback_Init() != HAL_OK)
  {
    Error_Handler();
  }
#elif ETH_LOOPBACK_TEST_MODE == ETH_LOOPBACK_MODE_PHY
  if (ETH_PHY_Loopback_Init() != HAL_OK)
  {
    Error_Handler();
  }
#elif ETH_LOOPBACK_TEST_MODE == ETH_LOOPBACK_MODE_RJ45
  if (ETH_RJ45_Loopback_Init() != HAL_OK)
  {
    Error_Handler();
  }
#else
#error "Invalid ETH_LOOPBACK_TEST_MODE"
#endif
  LCD_FillColor(RGB565_WHITE);
  // APP_RunColorCycleTest();
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of TouchGFXTask */
  TouchGFXTaskHandle = osThreadNew(TouchGFX_Task, NULL, &TouchGFXTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;
  hdma2d.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;
  hdma2d.LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR;
  hdma2d.LayerCfg[1].ChromaSubSampling = DMA2D_NO_CSS;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief ETH Initialization Function
  * @param None
  * @retval None
  */
static void MX_ETH_Init(void)
{

  /* USER CODE BEGIN ETH_Init 0 */

  /* USER CODE END ETH_Init 0 */

   static uint8_t MACAddr[6];

  /* USER CODE BEGIN ETH_Init 1 */

  /* USER CODE END ETH_Init 1 */
  heth.Instance = ETH;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
  heth.Init.TxDesc = DMATxDscrTab;
  heth.Init.RxDesc = DMARxDscrTab;
  heth.Init.RxBuffLen = 1524;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ETH_Init 2 */

  /* USER CODE END ETH_Init 2 */

}

/**
  * @brief FLASH Initialization Function
  * @param None
  * @retval None
  */
static void MX_FLASH_Init(void)
{

  /* USER CODE BEGIN FLASH_Init 0 */
  return;
  /* USER CODE END FLASH_Init 0 */

  FLASH_OBProgramInitTypeDef pOBInit = {0};

  /* USER CODE BEGIN FLASH_Init 1 */


  /* USER CODE END FLASH_Init 1 */
  HAL_FLASHEx_OBGetConfig(&pOBInit);
  if ((pOBInit.USERConfig1 & OB_IWDG_SW) != OB_IWDG_SW||
(pOBInit.USERConfig1 & OB_XSPI1_HSLV_ENABLE) != OB_XSPI1_HSLV_ENABLE||
(pOBInit.USERConfig1 & OB_XSPI2_HSLV_ENABLE) != OB_XSPI2_HSLV_DISABLE||
(pOBInit.USERConfig2 & OB_I2C_NI3C_I2C) != OB_I2C_NI3C_I2C)
  {
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_FLASH_OB_Unlock() != HAL_OK)
  {
    Error_Handler();
  }
  pOBInit.OptionType = OPTIONBYTE_USER;
  pOBInit.USERType = OB_USER_IWDG_SW|OB_USER_XSPI1_HSLV
                              |OB_USER_XSPI2_HSLV|OB_USER_I2C_NI3C;
  pOBInit.USERConfig1 = OB_IWDG_SW|OB_XSPI1_HSLV_ENABLE
                              |OB_XSPI2_HSLV_DISABLE;
  pOBInit.USERConfig2 = OB_I2C_NI3C_I2C;
  if (HAL_FLASHEx_OBProgram(&pOBInit) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_FLASH_OB_Lock() != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_FLASH_Lock() != HAL_OK)
  {
    Error_Handler();
  }
  }
  /* USER CODE BEGIN FLASH_Init 2 */

  /* USER CODE END FLASH_Init 2 */

}

/**
  * @brief GPU2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPU2D_Init(void)
{

  /* USER CODE BEGIN GPU2D_Init 0 */

  /* USER CODE END GPU2D_Init 0 */

  /* USER CODE BEGIN GPU2D_Init 1 */

  /* USER CODE END GPU2D_Init 1 */
  hgpu2d.Instance = GPU2D;
  if (HAL_GPU2D_Init(&hgpu2d) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN GPU2D_Init 2 */

  /* USER CODE END GPU2D_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00E063FF;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief ICACHE_GPU2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_GPU2D_Init(void)
{

  /* USER CODE BEGIN ICACHE_GPU2D_Init 0 */

  /* USER CODE END ICACHE_GPU2D_Init 0 */

  /* USER CODE BEGIN ICACHE_GPU2D_Init 1 */

  /* USER CODE END ICACHE_GPU2D_Init 1 */

  /** Enable instruction cache (default 2-ways set associative cache)
  */
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_GPU2D_Init 2 */

  /* USER CODE END ICACHE_GPU2D_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 23;
  hltdc.Init.VerticalSync = 1;
  hltdc.Init.AccumulatedHBP = 183;
  hltdc.Init.AccumulatedVBP = 24;
  hltdc.Init.AccumulatedActiveW = 1207;
  hltdc.Init.AccumulatedActiveH = 624;
  hltdc.Init.TotalWidth = 1367;
  hltdc.Init.TotalHeigh = 636;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 1024;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 600;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0x90000000;
  pLayerCfg.ImageWidth = 1024;
  pLayerCfg.ImageHeight = 600;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */
  
  /* USER CODE END LTDC_Init 2 */

}

/**
  * @brief TIM13 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM13_Init(void)
{

  /* USER CODE BEGIN TIM13_Init 0 */

  /* USER CODE END TIM13_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM13_Init 1 */

  /* USER CODE END TIM13_Init 1 */
  htim13.Instance = TIM13;
  htim13.Init.Prescaler = 100-1;
  htim13.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim13.Init.Period = 149;
  htim13.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim13.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim13) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim13) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim13, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM13_Init 2 */

  /* USER CODE END TIM13_Init 2 */
  HAL_TIM_MspPostInit(&htim13);

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart5.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart5, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart5, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */
  if (HAL_UARTEx_EnableFifoMode(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief UART8 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART8_Init(void)
{

  /* USER CODE BEGIN UART8_Init 0 */

  /* USER CODE END UART8_Init 0 */

  /* USER CODE BEGIN UART8_Init 1 */

  /* USER CODE END UART8_Init 1 */
  huart8.Instance = UART8;
  huart8.Init.BaudRate = 115200;
  huart8.Init.WordLength = UART_WORDLENGTH_8B;
  huart8.Init.StopBits = UART_STOPBITS_1;
  huart8.Init.Parity = UART_PARITY_NONE;
  huart8.Init.Mode = UART_MODE_TX_RX;
  huart8.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart8.Init.OverSampling = UART_OVERSAMPLING_16;
  huart8.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart8.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart8.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart8, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart8, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&huart8) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART8_Init 2 */
  if (HAL_UARTEx_EnableFifoMode(&huart8) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END UART8_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPION_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, TFT_EN_Pin|TFT_SETB_Pin|TFT_NRST_Pin|CTP_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BL_EN_GPIO_Port, BL_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TFT_LR_GPIO_Port, TFT_LR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : TFT_EN_Pin TFT_SETB_Pin TFT_NRST_Pin CTP_RST_Pin */
  GPIO_InitStruct.Pin = TFT_EN_Pin|TFT_SETB_Pin|TFT_NRST_Pin|CTP_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : BL_EN_Pin */
  GPIO_InitStruct.Pin = BL_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BL_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : TFT_LR_Pin */
  GPIO_InitStruct.Pin = TFT_LR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TFT_LR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CTP_INT_Pin */
  GPIO_InitStruct.Pin = CTP_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_ETH_RxAllocateCallback(uint8_t **buffer)
{
  *buffer = NULL;
  for (uint32_t index = 0U; index < ETH_LOOPBACK_RX_POOL_SIZE; index++)
  {
    if (ethLoopbackRxPool[index].inUse == 0U)
    {
      ethLoopbackRxPool[index].inUse = 1U;
      *buffer = ethLoopbackRxPool[index].data;
      return;
    }
  }
  ethLoopbackRxAllocErrors++;
}

void HAL_ETH_RxLinkCallback(void **start, void **end, uint8_t *buffer, uint16_t length)
{
  ETH_BufferTypeDef *link = NULL;

  for (uint32_t index = 0U; index < ETH_LOOPBACK_RX_POOL_SIZE; index++)
  {
    if (ethLoopbackRxPool[index].data == buffer)
    {
      link = &ethLoopbackRxPool[index].link;
      break;
    }
  }
  if (link == NULL)
  {
    ethLoopbackRxAllocErrors++;
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

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == CTP_INT_Pin)
  {
    CTP_InterruptPending = 1U;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart5)
  {
    UART_Echo_RxCpltCallback(&uart5Echo, huart);
  }
  else if (huart == &huart8)
  {
    UART_Echo_RxCpltCallback(&uart8Echo, huart);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart5)
  {
    UART_Echo_ErrorCallback(&uart5Echo, huart);
  }
  else if (huart == &huart8)
  {
    UART_Echo_ErrorCallback(&uart8Echo, huart);
  }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    ETH_Loopback_Process();
    UART_Echo_Process(&uart5Echo);
    UART_Echo_Process(&uart8Echo);
    osDelay(1);
  }
  /* USER CODE END 5 */
}

 /* MPU Configuration */

static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /* Disables all MPU regions */
  for(uint8_t i=0; i<__MPU_REGIONCOUNT; i++)
  {
    HAL_MPU_DisableRegion(i);
  }

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x90000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.BaseAddress = 0x70000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER3;
  MPU_InitStruct.BaseAddress = 0x24071C00;
  MPU_InitStruct.Size = MPU_REGION_SIZE_1KB;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
