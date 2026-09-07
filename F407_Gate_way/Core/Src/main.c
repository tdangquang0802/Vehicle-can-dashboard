/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
* @file           : main.c
  * @brief          : F407 - Gateway (FreeRTOS)
  *
  *  Task layout:
  *   - CAN RX ISR        : does the minimum possible work - copies the raw
  *                         frame into a queue item, toggles RX_LED, and
  *                         wakes vTask_CanRxProcess. No parsing happens
  *                         inside the ISR itself.
  *   - vTask_CanRxProcess: blocks on the CAN RX queue, parses each frame,
  *                         updates the shared sensor-aggregate struct
  *                         (protected by hAggMutex), replies with an
  *                         ACK/NACK (toggling TX_LED), and signals
  *                         vTask_UartPush that fresh data is available.
  *   - vTask_UartPush    : waits on a semaphore with a timeout equal to
  *                         UART_PUSH_INTERVAL_MS - this naturally implements
  *                         "push immediately on new data, otherwise push
  *                         periodically so the PC knows the link is alive".
  *
  * HARDWARE ASSUMPTIONS (adjust to match your actual board):
  *
  *  - TX_LED      : PD12 (LD4 on STM32F4-Discovery, green)
  *  - RX_LED      : PD14 (LD5 on STM32F4-Discovery, red)
  *
  * FreeRTOS: this file assumes the FreeRTOS kernel is already integrated by
  * CubeMX (heap_4, configUSE_MUTEXES=1). Native FreeRTOS API is used
  * directly (xTaskCreate/xQueueCreate/xSemaphoreCreate...) rather than the
  * CMSIS-RTOS2 wrapper.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "can_protocol.h"
#include "uart_protocol.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
#define TX_LED_GPIO_Port    GPIOD
#define TX_LED_Pin          GPIO_PIN_12  /* LD4 */
#define RX_LED_GPIO_Port    GPIOD
#define RX_LED_Pin          GPIO_PIN_14  /* LD5 */

#define TASK_STACK_SIZE     256
#define TASK_PRIO_CAN_RX    (tskIDLE_PRIORITY + 3)
#define TASK_PRIO_UART_TX   (tskIDLE_PRIORITY + 2)
#define CAN_RX_QUEUE_LEN    8

typedef struct {
    uint32_t stdId;
    uint8_t  data[8];
} CanRxItem_t;

typedef struct {
    uint16_t cycle_time_s;
    uint16_t speed_x10_kmh;
    uint16_t rpm;
    uint8_t  phase;
    uint32_t last_ms;
} CycleAggregate_t;

static CycleAggregate_t g_agg = {0};

static QueueHandle_t     hCanRxQueue;
static SemaphoreHandle_t hAggMutex;
static SemaphoreHandle_t hNewDataSem;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */
static void CAN_SendAck(uint8_t msgtypeEcho, uint8_t seqEcho, uint8_t status);
static void UART_SendFrame(uint8_t msgId, const uint8_t *payload, uint8_t len);
static void UART_PushSnapshot(void);
static void UART_PushFault(uint8_t faultCode);
void        UART2_Print(char *str);

static void vTask_CanRxProcess(void *argument);
static void vTask_UartPush(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void UART2_Print(char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

static void UART_SendFrame(uint8_t msgId, const uint8_t *payload, uint8_t len)
{
    uint8_t buf[2 + 1 + 1 + 255 + 1 + 1];
    uint8_t idx = 0;

    buf[idx++] = UART_SOF1;
    buf[idx++] = UART_SOF2;
    buf[idx++] = len;
    buf[idx++] = msgId;
    memcpy(&buf[idx], payload, len);
    idx += len;

    uint8_t crc = CRC8_Calc(&buf[3], (uint16_t)(1 + len));
    buf[idx++] = crc;
    buf[idx++] = UART_EOF;

    HAL_UART_Transmit(&huart2, buf, idx, HAL_MAX_DELAY);
}

static void UART_PushSnapshot(void)
{
    uint32_t now = HAL_GetTick();
    UART_CycleSnapshot_t snap = {0};

    xSemaphoreTake(hAggMutex, portMAX_DELAY);
    snap.cycle_time_s  = g_agg.cycle_time_s;
    snap.speed_x10_kmh = g_agg.speed_x10_kmh;
    snap.rpm           = g_agg.rpm;
    snap.phase         = g_agg.phase;
    snap.data_valid    = ((now - g_agg.last_ms) < UART_STALE_TIMEOUT_MS) ? 1 : 0;
    xSemaphoreGive(hAggMutex);

    UART_SendFrame(PC_MSG_CYCLE_SNAPSHOT, (uint8_t *)&snap, sizeof(snap));
}

static void UART_PushFault(uint8_t faultCode)
{
    UART_FaultPayload_t f = { .fault_code = faultCode };
    UART_SendFrame(PC_MSG_FAULT, (uint8_t *)&f, sizeof(f));
}

static void CAN_SendAck(uint8_t msgtypeEcho, uint8_t seqEcho, uint8_t status)
{
    CAN_TxHeaderTypeDef h = {0};
    uint8_t txData[8] = {0};
    uint32_t mailbox;
    CAN_AckPayload_t ack = { .msgtype_echo = msgtypeEcho, .seq_echo = seqEcho, .status = status };

    h.StdId = (status == CAN_ACK_OK) ? CAN_ID_GATEWAY_ACK : CAN_ID_GATEWAY_NACK;
    h.IDE = CAN_ID_STD;
    h.RTR = CAN_RTR_DATA;
    h.DLC = 8;
    memcpy(txData, &ack, sizeof(ack));
    HAL_CAN_AddTxMessage(&hcan1, &h, txData, &mailbox);

    HAL_GPIO_TogglePin(GPIOD, LD4_Pin);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    CanRxItem_t item;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, item.data) != HAL_OK) { return; }
    item.stdId = rxHeader.StdId;

    HAL_GPIO_TogglePin(GPIOD, LD5_Pin);

    xQueueSendFromISR(hCanRxQueue, &item, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ==========================================================================
 * TASKS
 * ========================================================================== */
static void vTask_CanRxProcess(void *argument)
{
    (void)argument;
    CanRxItem_t item;

    for (;;)
    {
        if (xQueueReceive(hCanRxQueue, &item, portMAX_DELAY) != pdTRUE) { continue; }

        uint32_t node = CAN_GET_NODE(item.stdId);
        uint32_t type = CAN_GET_TYPE(item.stdId);

        if (node != CAN_NODE_SIM) { continue; }

        uint8_t seq = 0;
        uint8_t status = CAN_ACK_OK;
        uint8_t gotNewData = 0;

        switch (type)
        {
            case CAN_MSG_VEHICLE_CYCLE:
            {
                CAN_VehicleCyclePayload_t *p = (CAN_VehicleCyclePayload_t *)item.data;
                seq = p->seq;

                xSemaphoreTake(hAggMutex, portMAX_DELAY);
                g_agg.cycle_time_s  = p->cycle_time_s;
                g_agg.speed_x10_kmh = p->speed_x10_kmh;
                g_agg.rpm           = p->rpm;
                g_agg.phase         = p->phase;
                g_agg.last_ms       = HAL_GetTick();
                xSemaphoreGive(hAggMutex);

                gotNewData = 1;
                break;
            }
            case CAN_MSG_SIM_FAULT:
            {
                CAN_FaultPayload_t *f = (CAN_FaultPayload_t *)item.data;
                UART_PushFault(f->fault_code);
                continue; /* fault frames are not acknowledged */
            }
            default:
                continue; /* unknown type, ignore, no ACK */
        }

        CAN_SendAck(type, seq, status);

        if (gotNewData)
        {
            xSemaphoreGive(hNewDataSem);
        }
    }
}

static void vTask_UartPush(void *argument)
{
    (void)argument;

    for (;;)
    {
        xSemaphoreTake(hNewDataSem, pdMS_TO_TICKS(UART_PUSH_INTERVAL_MS));
        UART_PushSnapshot();
    }
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

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
  UART2_Print("F407 Gateway (FreeRTOS) starting...\r\n");

   hCanRxQueue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(CanRxItem_t));
   hAggMutex   = xSemaphoreCreateMutex();
   hNewDataSem = xSemaphoreCreateBinary();

   xTaskCreate(vTask_CanRxProcess, "CanRx", TASK_STACK_SIZE, NULL, TASK_PRIO_CAN_RX,  NULL);
   xTaskCreate(vTask_UartPush,     "UartTx", TASK_STACK_SIZE, NULL, TASK_PRIO_UART_TX, NULL);

   vTaskStartScheduler();
  /* USER CODE END 2 */

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
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */
  CAN_FilterTypeDef canFilter = {0};
      canFilter.FilterBank = 0;
      canFilter.FilterMode = CAN_FILTERMODE_IDMASK;
      canFilter.FilterScale = CAN_FILTERSCALE_32BIT;
      canFilter.FilterIdHigh = (CAN_FILTER_ID_FROM_SIM << 5);
      canFilter.FilterIdLow = 0x0000;
      canFilter.FilterMaskIdHigh = (CAN_FILTER_MASK_NODE_ONLY << 5);
      canFilter.FilterMaskIdLow = 0x0000;
      canFilter.FilterFIFOAssignment = CAN_RX_FIFO0;
      canFilter.FilterActivation = ENABLE;
      canFilter.SlaveStartFilterBank = 14;
      HAL_CAN_ConfigFilter(&hcan1, &canFilter);

      HAL_CAN_Start(&hcan1);
      HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : CS_I2C_SPI_Pin */
  GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PDM_OUT_Pin */
  GPIO_InitStruct.Pin = PDM_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : I2S3_WS_Pin */
  GPIO_InitStruct.Pin = I2S3_WS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(I2S3_WS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI1_SCK_Pin SPI1_MISO_Pin SPI1_MOSI_Pin */
  GPIO_InitStruct.Pin = SPI1_SCK_Pin|SPI1_MISO_Pin|SPI1_MOSI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : CLK_IN_Pin */
  GPIO_InitStruct.Pin = CLK_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
                           Audio_RST_Pin */
  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : I2S3_MCK_Pin I2S3_SCK_Pin I2S3_SD_Pin */
  GPIO_InitStruct.Pin = I2S3_MCK_Pin|I2S3_SCK_Pin|I2S3_SD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : VBUS_FS_Pin */
  GPIO_InitStruct.Pin = VBUS_FS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(VBUS_FS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : OTG_FS_ID_Pin OTG_FS_DM_Pin OTG_FS_DP_Pin */
  GPIO_InitStruct.Pin = OTG_FS_ID_Pin|OTG_FS_DM_Pin|OTG_FS_DP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Audio_SCL_Pin */
  GPIO_InitStruct.Pin = Audio_SCL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(Audio_SCL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MEMS_INT2_Pin */
  GPIO_InitStruct.Pin = MEMS_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
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
