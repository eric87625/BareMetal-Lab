/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>  // Required for snprintf, sscanf()
#include <stdarg.h> // Required for variable argument handling
#include <stdlib.h>
#include "console.h"
#include "uart_rb.h"
#include "packet.h"
#include "cmd.h"
#include "uart_test.h"
#include "watchdog.h"
#include "latency.h"
#include "load_task.h"

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
IWDG_HandleTypeDef hiwdg;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart3_rx;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_IWDG_Init(void);
static void MX_TIM3_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t tx_buff[] = { 65, 66, 67, 68, 69, 70, 71, 72, 73, 74 }; //ABCDEFGHIJ in ASCII code
uint8_t rx_buff[10];

char cmd_buff[30];
int cmd_buff_idx;

static int pass_count = 0;

#define RX_BUF_SIZE 64

uint8_t uart1_rx_buf[RX_BUF_SIZE];
uint8_t uart3_rx_buf[RX_BUF_SIZE];

uint8_t uart1_data[RX_BUF_SIZE];
uint8_t uart3_data[RX_BUF_SIZE];

volatile uint16_t uart1_len = 0;
volatile uint16_t uart3_len = 0;

volatile uint8_t uart1_ready = 0;
volatile uint8_t uart3_ready = 0;

static RingBuffer rb_uart1;
static RingBuffer rb_uart3;

static PacketParser parser1;
static PacketParser parser3;

/* Helper: drain ring buffer to parser
 * max_bytes = 0 for unlimited drain in main loop
 * use a small cap (e.g., 32) when called from interrupt context */
static inline void drain_rb_parser(RingBuffer *rb, PacketParser *parser, uint16_t max_bytes)
{
    uint8_t ch;
    uint16_t count = 0;
    while (rb_pop(rb, &ch))
    {
        packet_parser_feed(parser, ch);
        if (max_bytes && (++count >= max_bytes))
            break;
    }
}

void uart_init_dma(void)
{
    print("********** Start uart_init_dma... **********\r\n");
    HAL_UART_Receive_DMA(&huart1, uart1_rx_buf, RX_BUF_SIZE);
    print("UART1 DMA RX started\r\n");

    HAL_UART_Receive_DMA(&huart3, uart3_rx_buf, RX_BUF_SIZE);
    print("UART3 DMA RX started\r\n");

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    print("UART1 IDLE enabled\r\n");

    __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
    print("UART3 IDLE enabled\r\n");

//    __HAL_DMA_ENABLE_IT(huart1.hdmarx, DMA_IT_HT);
//    __HAL_DMA_ENABLE_IT(huart1.hdmarx, DMA_IT_TC);
//
//    __HAL_DMA_ENABLE_IT(huart3.hdmarx, DMA_IT_HT);
//    __HAL_DMA_ENABLE_IT(huart3.hdmarx, DMA_IT_TC);

    print("**********End of uart_init_dma **********\r\n");
}

void uart_send(UART_HandleTypeDef *huart, const char *msg)
{
    pass_count++;
//    HAL_UART_Transmit_DMA(huart, (uint8_t *)msg, strlen(msg));
    HAL_UART_Transmit(huart, (uint8_t*) msg, strlen(msg), HAL_MAX_DELAY);
}

/* ---------- IDLE callback (no TX) ---------- */
void HAL_UART_IDLE_Callback(UART_HandleTypeDef *huart)
{
    print("\r\n ===== HAL_UART_IDLE_Callback by %d  =====\r\n\r\n",
                huart->Instance == USART1 ? 1 : 3);
    __HAL_UART_CLEAR_IDLEFLAG(huart);
    // Use pure circular DMA: compute current write position and process only new bytes
    uint16_t cur_pos = RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);
    static uint16_t last_pos_uart1 = 0;
    static uint16_t last_pos_uart3 = 0;

    if (cur_pos > RX_BUF_SIZE)
        return;

    if (huart->Instance == USART1)
    {
        uint16_t last = last_pos_uart1;
        if (cur_pos != last)
        {
            if (cur_pos > last)
            {
                for (uint16_t i = last; i < cur_pos; i++)
                    rb_push(&rb_uart1, uart1_rx_buf[i]);
            }
            else
            {
                for (uint16_t i = last; i < RX_BUF_SIZE; i++)
                    rb_push(&rb_uart1, uart1_rx_buf[i]);
                for (uint16_t i = 0; i < cur_pos; i++)
                    rb_push(&rb_uart1, uart1_rx_buf[i]);
            }
            last_pos_uart1 = cur_pos;
            // Drain a small batch in ISR context to improve responsiveness
            drain_rb_parser(&rb_uart1, &parser1, 32);
        }
    }
    else if (huart->Instance == USART3)
    {
        uint16_t last = last_pos_uart3;
        if (cur_pos != last)
        {
            if (cur_pos > last)
            {
                for (uint16_t i = last; i < cur_pos; i++)
                    rb_push(&rb_uart3, uart3_rx_buf[i]);
            }
            else
            {
                for (uint16_t i = last; i < RX_BUF_SIZE; i++)
                    rb_push(&rb_uart3, uart3_rx_buf[i]);
                for (uint16_t i = 0; i < cur_pos; i++)
                    rb_push(&rb_uart3, uart3_rx_buf[i]);
            }
            last_pos_uart3 = cur_pos;
            // Drain a small batch in ISR context to improve responsiveness
            drain_rb_parser(&rb_uart3, &parser3, 32);
        }
    }
}

//void HAL_UART_IDLE_Callback(UART_HandleTypeDef *huart)
//{
//    print("\r\n ===== HAL_UART_IDLE_Callback by %d  =====\r\n\r\n",
//            huart->Instance == USART1 ? 1 : 3);
//    __HAL_UART_CLEAR_IDLEFLAG(huart);
//
//    uint16_t cnt = RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);
//    static uint16_t cur_uart1_rx_buf_idx = 0UL;
//    static uint16_t cur_uart3_rx_buf_idx = 0UL;
//
//    if (cnt == 0)
//        return;
//
//    if (huart->Instance == USART1)
//    {
//        for (; cur_uart1_rx_buf_idx < cnt; cur_uart1_rx_buf_idx++)
//        {
//            rb_push(&rb_uart1, uart1_rx_buf[cur_uart1_rx_buf_idx]);
//        }
//        HAL_UART_Receive_DMA(huart, uart1_rx_buf, RX_BUF_SIZE);
//    }
//    else if (huart->Instance == USART3)
//    {
//        for (; cur_uart3_rx_buf_idx < cnt; cur_uart3_rx_buf_idx++)
//        {
//            rb_push(&rb_uart3, uart3_rx_buf[cur_uart3_rx_buf_idx]);
//        }
//        HAL_UART_Receive_DMA(huart, uart3_rx_buf, RX_BUF_SIZE);
//    }
//}

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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_IWDG_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
    console_init(&huart2);

    /* Level 3: check reset reason early (after console is ready). */
    System_Check_Reset_Reason();

    cmd_buff_idx = 0;
//	int number_of_dogs = 5;
//	char *dogs_name = "George";

    print("\r\n\r\n\r\n\r\n\r\n\r\n\r\nSTART ~ \r\n");

//	print("There are %d dogs, all named %s !\r\n", number_of_dogs, dogs_name);
//	print("There are %d dogs, all named %s !\r\n", number_of_dogs, dogs_name);

    print("=== UART1 <-> UART3 DMA loopback test ===\r\n");

    uart_init_dma();
    // Initialize ring buffers and streaming parsers for UART1/3
    rb_init(&rb_uart1);
    rb_init(&rb_uart3);
    packet_parser_init(&parser1);
    packet_parser_init(&parser3);

    latency_init(&htim3, &huart2);
    if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
    {
      Error_Handler();
    }

    {
        // uart_test:

        // init
//        uart_test_init(&huart3);
//
//        // UART_TEST_SINGLE_CMD
//        uart_test_run(UART_TEST_SINGLE_CMD);
//
//        // UART_TEST_CMD_WITH_IDLE
//        uart_test_run(UART_TEST_CMD_WITH_IDLE);
//
//        // CMD_STICKY
//        uart_test_run(UART_TEST_CMD_STICKY);
//
//        // UART_TEST_CONTINUOUS_STREAM
//        uart_test_run(UART_TEST_CONTINUOUS_STREAM);

    }

    // Manually send the first Ping (optional to start)
//    uart_send(&huart1, "Ping from UART1\r\n");
//    uart_send(&huart3, "Ping from UART3\r\n");

    while (HAL_GetTick() < 1000)
    {
      /* Level 1: keep feeding watchdog in the main processing loop. */
      Watchdog_Refresh();

        // Unbounded drain in main loop for steady parsing
//        drain_rb_parser(&rb_uart1, &parser1, 0);
//        drain_rb_parser(&rb_uart3, &parser3, 0);

        if (uart1_ready)
        {

            print("\r\n[UART1][PASS:%d] recv %d bytes\r\n", pass_count,
                    uart1_len);
            // print msg
            for (uint16_t i = 0; i < uart1_len; i++)
                print("%c", uart1_data[i]);
            print("\r\n");

            // Ping
            uart_send(&huart1, "Ping from UART1\r\n");

            uart1_ready = 0;
        }

        if (uart3_ready)
        {

            print("\r\n[UART3][PASS:%d] recv %d bytes\r\n", pass_count,
                    uart3_len);
            // print msg
            for (uint16_t i = 0; i < uart3_len; i++)
                print("%c", uart3_data[i]);
            print("\r\n");

            // Pong
            uart_send(&huart3, "Pong from UART3\r\n");

            uart3_ready = 0;
        }
    }

    HAL_UART_Receive_DMA(&huart2, rx_buff, 1);

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

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  load_task_start();
  latency_start_logging_task();
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
      /* Level 1: periodic watchdog refresh. */
      Watchdog_Refresh();

      /* Optional: add background tasks here. */
      HAL_Delay(10);
  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 16000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 5000;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 15;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
    memset(rx_buff, 0, sizeof(rx_buff));
    memset(cmd_buff, 0, sizeof(cmd_buff));
  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
  /* DMA1_Ch4_7_DMAMUX1_OVR_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Ch4_7_DMAMUX1_OVR_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(DMA1_Ch4_7_DMAMUX1_OVR_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_GREEN_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_GREEN_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{

    if (huart->Instance == USART2)
    {

        process_cmd();

        //	print("RX callback triggered\r\n");

        // HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
        //	HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);

        HAL_UART_Receive_DMA(&huart2, rx_buff, 1);
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
    /* After osKernelStart(), the main() infinite loop is no longer executed.
     * Refresh watchdog here to avoid periodic resets.
     * Note: current CubeMX IWDG config appears to be a short timeout, so keep
     * the refresh period comfortably below it.
     */
    Watchdog_Refresh();
    osDelay(100);
  }
  /* USER CODE END 5 */
}

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

  if (htim->Instance == TIM3)
  {
    latency_on_tim_period_elapsed_isr(htim);
  }

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
