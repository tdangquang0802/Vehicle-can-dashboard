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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "can_protocol.h"

#include "FreeRTOS.h"
#include "task.h"
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
#define TASK_PRIO_CYCLE     (tskIDLE_PRIORITY + 2)
#define WHEEL_CIRCUMFERENCE_M   1.885f

typedef struct {
    uint16_t time_s;
    uint16_t speed_x10_kmh;
} CycleWaypoint_t;

static const CycleWaypoint_t g_cycleTable[] = {
    {  0,    0 },  /* t=0s   : standing still                */
    {  5,    0 },  /* t=5s   : still idle                    */
    { 20,  500 },  /* t=20s  : accelerate up to 50.0 km/h    */
    { 35,  500 },  /* t=35s  : cruise at 50.0 km/h           */
    { 50,  900 },  /* t=50s  : accelerate up to 90.0 km/h    */
    { 70,  900 },  /* t=70s  : cruise at 90.0 km/h           */
    { 85,    0 },  /* t=85s  : decelerate down to a stop     */
    { 95,    0 },  /* t=95s  : idle before the loop repeats  */
};
#define CYCLE_POINTS  (sizeof(g_cycleTable) / sizeof(g_cycleTable[0]))

/* RTOS synchronization objects */
static SemaphoreHandle_t hCanTxMutex;
static SemaphoreHandle_t hAckSemaphore;

/* ACK-matching state, only valid while hCanTxMutex is held */
static volatile uint8_t ack_msgtype_exp = 0;
static volatile uint8_t ack_seq_exp     = 0;
static volatile uint8_t ack_ok          = 0;

static uint8_t seq_cycle = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void    Cycle_Evaluate(uint16_t time_s, uint16_t *speedX10_out, uint8_t *phase_out);
static uint8_t CAN_SendAndWaitAck(uint32_t stdId, uint8_t msgtype, uint8_t seq, uint8_t *payload8);
static void    CAN_SendFault(uint8_t faultCode);
void            UART2_Print(char *str);

static void vTask_VehicleCycle(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void UART2_Print(char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

static void Cycle_Evaluate(uint16_t time_s, uint16_t *speedX10_out, uint8_t *phase_out)
{
    uint16_t totalDuration = g_cycleTable[CYCLE_POINTS - 1].time_s;
    uint16_t t = (totalDuration > 0) ? (time_s % totalDuration) : 0;

    for (uint32_t i = 0; i < CYCLE_POINTS - 1; i++)
    {
        uint16_t t0 = g_cycleTable[i].time_s;
        uint16_t t1 = g_cycleTable[i + 1].time_s;

        if (t >= t0 && t <= t1)
        {
            uint16_t v0 = g_cycleTable[i].speed_x10_kmh;
            uint16_t v1 = g_cycleTable[i + 1].speed_x10_kmh;

            uint16_t speed = (t1 == t0) ? v0 :
                (uint16_t)(v0 + ((int32_t)(v1 - v0) * (int32_t)(t - t0)) / (int32_t)(t1 - t0));

            uint8_t phase;
            if (v1 > v0)      { phase = CYCLE_PHASE_ACCEL; }
            else if (v1 < v0) { phase = CYCLE_PHASE_DECEL; }
            else              { phase = (v0 == 0) ? CYCLE_PHASE_IDLE : CYCLE_PHASE_CRUISE; }

            *speedX10_out = speed;
            *phase_out = phase;
            return;
        }
    }

    *speedX10_out = 0;
    *phase_out = CYCLE_PHASE_IDLE;
}

/* ---- CAN RX ISR callback: only handles ACK/NACK from the Gateway board ---- */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8] = {0};
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) != HAL_OK) { return; }

    uint32_t node = CAN_GET_NODE(rxHeader.StdId);
    uint32_t type = CAN_GET_TYPE(rxHeader.StdId);

    if (node == CAN_NODE_GATEWAY && (type == CAN_MSG_ACK || type == CAN_MSG_NACK))
    {
        HAL_GPIO_TogglePin(GPIOD, LD4_Pin);

        CAN_AckPayload_t *ack = (CAN_AckPayload_t *)rxData;
        if (ack->msgtype_echo == ack_msgtype_exp && ack->seq_echo == ack_seq_exp)
        {
            ack_ok = (type == CAN_MSG_ACK) && (ack->status == CAN_ACK_OK);
            xSemaphoreGiveFromISR(hAckSemaphore, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ---- Send one frame and wait for the matching ACK, auto-retry up to
 *      CAN_MAX_RETRY times. ---- */
static uint8_t CAN_SendAndWaitAck(uint32_t stdId, uint8_t msgtype, uint8_t seq, uint8_t *payload8)
{
    CAN_TxHeaderTypeDef h = {0};
    uint32_t mailbox;
    uint8_t result = 0;

    xSemaphoreTake(hCanTxMutex, portMAX_DELAY);

    h.StdId = stdId;
    h.IDE = CAN_ID_STD;
    h.RTR = CAN_RTR_DATA;
    h.DLC = 8;

    ack_msgtype_exp = msgtype;
    ack_seq_exp = seq;

    for (uint8_t attempt = 0; attempt < CAN_MAX_RETRY; attempt++)
    {
        xSemaphoreTake(hAckSemaphore, 0); /* drain any stale give */
        ack_ok = 0;

        if (HAL_CAN_AddTxMessage(&hcan1, &h, payload8, &mailbox) != HAL_OK) { continue; }

        HAL_GPIO_TogglePin(GPIOD, LD5_Pin);

        if (xSemaphoreTake(hAckSemaphore, pdMS_TO_TICKS(CAN_ACK_TIMEOUT_MS)) == pdTRUE)
        {
            if (ack_ok) { result = 1; break; }
        }
    }

    xSemaphoreGive(hCanTxMutex);
    return result;
}

static void CAN_SendFault(uint8_t faultCode)
{
    CAN_TxHeaderTypeDef h = {0};
    uint8_t txData[8] = {0};
    uint32_t mailbox;
    CAN_FaultPayload_t f = { .fault_code = faultCode };

    h.StdId = CAN_ID_SIM_FAULT;
    h.IDE = CAN_ID_STD;
    h.RTR = CAN_RTR_DATA;
    h.DLC = 8;
    memcpy(txData, &f, sizeof(f));

    xSemaphoreTake(hCanTxMutex, portMAX_DELAY);
    HAL_CAN_AddTxMessage(&hcan1, &h, txData, &mailbox);
    HAL_GPIO_TogglePin(TX_LED_GPIO_Port, TX_LED_Pin);
    xSemaphoreGive(hCanTxMutex);
}

/* ==========================================================================
 * TASK
 * ========================================================================== */
static void vTask_VehicleCycle(void *argument)
{
    (void)argument;
    UART2_Print("[DBG] task entered\r\n");
    TickType_t lastWake = xTaskGetTickCount();
    uint16_t cycleTimeS = 0;

    for (;;)
    {
        uint16_t speedX10;
        uint8_t phase;
        Cycle_Evaluate(cycleTimeS, &speedX10, &phase);

        float speed_kmh = speedX10 / 10.0f;
        float rpm_f = (speed_kmh * 1000.0f / 60.0f) / WHEEL_CIRCUMFERENCE_M;

        uint8_t txData[8];
        char msg[80];

        CAN_VehicleCyclePayload_t cP = {0};
        cP.seq = seq_cycle++;
        cP.cycle_time_s = cycleTimeS;
        cP.speed_x10_kmh = speedX10;
        cP.rpm = (uint16_t)rpm_f;
        cP.phase = phase;
        memcpy(txData, &cP, sizeof(cP));

        uint32_t t0 = HAL_GetTick();
        uint8_t sendOk = CAN_SendAndWaitAck(CAN_ID_SIM_VEHICLE_CYCLE, CAN_MSG_VEHICLE_CYCLE, cP.seq, txData);
        uint32_t elapsed = HAL_GetTick() - t0;

        if (sendOk)
        {
            sprintf(msg, "[CYCLE] t=%us speed=%u.%ukm/h rpm=%u phase=%u\r\n",
                    cycleTimeS, speedX10 / 10, speedX10 % 10, cP.rpm, phase);
        }
        else
        {
            sprintf(msg, "[CYCLE] send failed after %u retries\r\n", CAN_MAX_RETRY);
            CAN_SendFault(1);
        }
        UART2_Print(msg);

        /* ---- debug: timing + bus error state ---- */
        char dbg[96];
        sprintf(dbg, "[DBG] send took %lums\r\n", (unsigned long)elapsed);
        UART2_Print(dbg);

        sprintf(dbg, "[DBG] ESR=0x%08lX TEC=%lu REC=%lu\r\n",
                (unsigned long)hcan1.Instance->ESR,
                (unsigned long)((hcan1.Instance->ESR >> 16) & 0xFF),
                (unsigned long)((hcan1.Instance->ESR >> 24) & 0xFF));
        UART2_Print(dbg);

        cycleTimeS++; /* advance the virtual clock by one sample period (1s) */

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CAN_CYCLE_SAMPLE_INTERVAL_MS));
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
  MX_CAN1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  UART2_Print("F407 Vehicle Cycle Simulator (FreeRTOS) starting...\r\n");

  hCanTxMutex   = xSemaphoreCreateMutex();
  hAckSemaphore = xSemaphoreCreateBinary();
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  char dbg[64];
  sprintf(dbg, "[DBG] mutex=%p sem=%p\r\n", (void*)hCanTxMutex, (void*)hAckSemaphore);
  UART2_Print(dbg);

      BaseType_t taskResult = xTaskCreate(vTask_VehicleCycle, "Cycle", TASK_STACK_SIZE, NULL, TASK_PRIO_CYCLE, NULL);
      sprintf(dbg, "[DBG] xTaskCreate result=%ld (pdPASS=%ld)\r\n", (long)taskResult, (long)pdPASS);
      UART2_Print(dbg);
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
      canFilter.FilterIdHigh = (CAN_FILTER_ID_FROM_GATEWAY << 5);
      canFilter.FilterIdLow = 0x0000;
      canFilter.FilterMaskIdHigh = (CAN_FILTER_MASK_NODE_ONLY << 5);
      canFilter.FilterMaskIdLow = 0x0000;
      canFilter.FilterFIFOAssignment = CAN_RX_FIFO0;
      canFilter.FilterActivation = ENABLE;
      canFilter.SlaveStartFilterBank = 14;
      HAL_CAN_ConfigFilter(&hcan1, &canFilter);


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

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

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

  /*Configure GPIO pin : BOOT1_Pin */
  GPIO_InitStruct.Pin = BOOT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

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
