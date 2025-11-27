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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>  // Required for snprintf, sscanf()
#include <stdarg.h> // Required for variable argument handling
#include <stdlib.h>
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
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart3_rx;

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

//static int print_val(char *s, int var) {
//	char buf[256];
//	int len = snprintf(buf, sizeof(buf), s, var);
////  HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart2, (uint8_t *)buf, len);
////	HAL_StatusTypeDef ret = HAL_UART_Transmit_IT(&huart2, (uint8_t *)buf, len);
//	HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart2, (uint8_t*) buf, len,
//	HAL_MAX_DELAY);
//
//	return (ret == HAL_OK) ? len : -1;
//}

static int print(const char *str, ...) {

    char buf[256];
    va_list args; // Declare a va_list to hold the variable arguments

    va_start(args, str); // Initialize va_list with the last fixed argument (str)
    int cur_buf_len = vsnprintf(buf, sizeof(buf), str, args); // Use vsnprintf for variable arguments
    va_end(args); // Clean up the va_list

    if (cur_buf_len < 0) {
        // Handle error during formatting
        return -1;
    }

    // Ensure null termination in case of truncation by snprintf
    if (cur_buf_len >= sizeof(buf)) {
        buf[sizeof(buf) - 1] = '\0';
    }

    // Print the formatted string to uart output
//  HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart2, (uint8_t *)buf, len);
//  HAL_StatusTypeDef ret = HAL_UART_Transmit_IT(&huart2, (uint8_t *)buf, len);
	int ret = HAL_UART_Transmit(&huart2, (uint8_t*) buf, cur_buf_len, HAL_MAX_DELAY) == HAL_OK;
	return ret;
}

//int print(const char *str, ...) {
//
//	int count = 0;
//	int cur_buf_len = 0;
//	char buf[256];
//	int len = strlen(str);
//
//	memset(buf, 0, sizeof(buf));
//
//	va_list args;
//	va_start(args, str);
//
//	for (int i = 0; i < len; i++) {
//		if (str[i] == '%') {
//			i++;
//
//			/* Print variable to buffer BEGIN */
//			switch (str[i]) {
//			case 'd':
//				int val = va_arg(args, int);
//
//				int prev_len = strlen(buf);
//				itoa(val, buf + cur_buf_len, 10);
//				int post_len = strlen(buf);
//				cur_buf_len += post_len - prev_len;
//
////				cur_buf_len += snprintf(buf + cur_buf_len, sizeof(buf) - cur_buf_len, "%d", val);
//				break;
//			case 'c':
//				int ch = va_arg(args, int);
//
//				buf[cur_buf_len] = ch;
//				cur_buf_len ++;
//
////				cur_buf_len += snprintf(buf + cur_buf_len, sizeof(buf) - cur_buf_len, "%c", ch);
//				break;
//			case 's':
//				char *string = va_arg(args, char*);
//
//				strcpy(buf + cur_buf_len, string);
//				cur_buf_len += strlen(string);
//
////				cur_buf_len += snprintf(buf + cur_buf_len, sizeof(buf) - cur_buf_len, "%s", string);
//				break;
//			}
//			/* Print variable to buffer END */
//			count++;
//		} else {
//			buf[cur_buf_len] = str[i];
//			cur_buf_len ++;
////			cur_buf_len += snprintf(buf + cur_buf_len, sizeof(buf) - cur_buf_len, "%c", str[i]);
//		}
//	}
//	va_end(args);
//
////  print_val("the number of variables within print is %d\r\n", count);
//
////    int ret = HAL_UART_Transmit_DMA(&huart2, (uint8_t *)buf, cur_buf_len) == HAL_OK;
////    int ret = HAL_UART_Transmit_IT(&huart2, (uint8_t *)buf, cur_buf_len);
//	int ret = HAL_UART_Transmit(&huart2, (uint8_t*) buf, cur_buf_len, HAL_MAX_DELAY) == HAL_OK;
//	return ret;
//}

void uart_init_dma(void)
{
    print("Start UART1 DMA RX...\r\n");
    HAL_UART_Receive_DMA(&huart1, uart1_rx_buf, RX_BUF_SIZE);
    print("UART1 DMA RX started\r\n");

    HAL_UART_Receive_DMA(&huart3, uart3_rx_buf, RX_BUF_SIZE);
    print("UART3 DMA RX started\r\n");

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    print("UART1 IDLE enabled\r\n");

    __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
    print("UART3 IDLE enabled\r\n");

    print("end of uart_init_dma\r\n");
}


void uart_send(UART_HandleTypeDef *huart, const char *msg)
{
	pass_count ++;
//    HAL_UART_Transmit_DMA(huart, (uint8_t *)msg, strlen(msg));
    HAL_UART_Transmit(huart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/* ---------- IDLE callback (no TX) ---------- */
void HAL_UART_IDLE_Callback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        __HAL_UART_CLEAR_IDLEFLAG(huart);
        uint16_t received = RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);

        memcpy(uart1_data, uart1_rx_buf, received);
        uart1_len = received;
        uart1_ready = 1;      // flag，只做這個

        // 重新啟動 DMA
        HAL_UART_DMAStop(huart);
        HAL_UART_Receive_DMA(huart, uart1_rx_buf, RX_BUF_SIZE);
    }

    else if (huart->Instance == USART3) {
        __HAL_UART_CLEAR_IDLEFLAG(huart);
        uint16_t received = RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);

        memcpy(uart3_data, uart3_rx_buf, received);
        uart3_len = received;
        uart3_ready = 1;      // flag

        HAL_UART_DMAStop(huart);
        HAL_UART_Receive_DMA(huart, uart3_rx_buf, RX_BUF_SIZE);
    }
}

void dma_rx_reset(UART_HandleTypeDef *huart)
{
    __HAL_DMA_DISABLE(huart->hdmarx);
    huart->hdmarx->Instance->CNDTR = RX_BUF_SIZE;  // reset DMA counter
    __HAL_DMA_ENABLE(huart->hdmarx);
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
	cmd_buff_idx = 0;
	int number_of_dogs = 5;
	char *dogs_name = "George";

	print("\r\nSTART ~ \r\n");

	print("There are %d dogs, all named %s !\r\n", number_of_dogs, dogs_name);
	print("There are %d dogs, all named %s !\r\n", number_of_dogs, dogs_name);

	print("=== UART1 <-> UART3 DMA loopback test ===\r\n");

	uart_init_dma();

	while (HAL_GetTick() < 3000) {

	    if (uart1_ready) {
	        uart1_ready = 0;

	        print("\r\n[UART1][PASS:%d] recv %d bytes\r\n", pass_count, uart1_len);

	        // 打印資料
	        for (uint16_t i = 0; i < uart1_len; i++)
	            print("%c", uart1_data[i]);

	        print("\r\n");

	        // 回傳 Ping
	        uart_send(&huart3, "Ping from UART1\r\n");

	        // 清空 UART3 的 RX buffer（避免下一輪越來越長）
	        dma_rx_reset(&huart3);
	    }

	    if (uart3_ready) {
	        uart3_ready = 0;

	        print("\r\n[UART3][PASS:%d] recv %d bytes\r\n", pass_count, uart3_len);

	        // 打印資料
	        for (uint16_t i = 0; i < uart3_len; i++)
	            print("%c", uart3_data[i]);

	        print("\r\n");

	        // 回傳 Pong
	        uart_send(&huart1, "Pong from UART3\r\n");

	        // 清空 UART1 RX buffer
	        dma_rx_reset(&huart1);
	    }
	}



	HAL_UART_Receive_DMA(&huart2, rx_buff, 1);
//	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
//	HAL_UART_Transmit_IT(&huart2, tx_buff, 10);
//	HAL_Delay(10000);

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
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
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
  /* DMA1_Ch4_7_DMAMUX1_OVR_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Ch4_7_DMAMUX1_OVR_IRQn, 0, 0);
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




/* Definition of CMDs BEGIN*/
typedef void (*CmdHandler)(int argc, char **argv);

// The way to link between CMD(enum type) and CMD(char *) -> struct include both cmd_name & handler.
typedef struct {
	char *cmd_name;
	CmdHandler handler;
} CmdEntry;

void func_led_on(int para_count, char **para){
	if(para_count != 0){
		print("error: func_led_on: para_count != 0, must check!\r\n");
		return;
	}
	HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
}
void func_led_off(int para_count, char **para){
	if(para_count != 0){
		print("error: func_led_off: para_count != 0, must check!\r\n");
		return;
	}
	HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
}
void func_set_led(int para_count, char **para){
	if(para_count != 1){
		print("error: func_led_off: para_count = %d != 1, must check!\r\n", para_count);
		return;
	}
	int led_num;
    sscanf(para[0], "%d", &led_num); // led_num will be 1
	if (led_num == 1) HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
//	else if (led_num == 2) HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
}
void func_uart_tx(int para_count, char **para){
	char *paraStr = malloc(strlen(para[0]) + 1);  // +1 for '\0'
	if (paraStr != NULL) {
	    strcpy(paraStr, para[0]);
	}
	HAL_UART_Transmit(&huart2, (uint8_t*)paraStr, strlen(paraStr), HAL_MAX_DELAY);
	// free
	free(paraStr);
}
void func_pwm_on(int para_count, char **para){
	if(para_count != 2){
		print("error: func_led_on: para_count != 2, must check!\r\n");
		return;
	}
	int Duty;
	int Freq;

	sscanf(para[0], "%d", &Duty);
	sscanf(para[1], "%d", &Freq);
	if (Duty > 100)
	{
		Duty = 100;
		print("Warning: Max Duty is 100\r\n");
	}
	if (Freq < 0 || Freq > 10000)
	{
		Freq = 1000;
		print("Warning: Freq is out-of-range\r\n");
	}
	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
	// ARR is time range.
//	Duty(%)=ARR/CCR​×100%
//	ARR (Period) sets the PWM frequency, CCR1 (Pulse) sets the duty cycle.
//	htim2.Init.Period = 16000;
//	sConfigOC.Pulse = 5000;
	htim2.Instance->ARR = 16000000 / Freq;
	htim2.Instance->CCR1 = htim2.Instance->ARR / 100 * Duty;
	//
	//htim2.Instance->CCR1 = htim2.Instance->ARR; //99.99%

	if (Duty == 100 || Duty ==0 ){
		// Stop PWM
		HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);

		// set GPIO mode
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = GPIO_PIN_0;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 普通 GPIO Output
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		// Duty 100 -> High, Duty 0 -> Low
		if (Duty == 100)
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	}
	//__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, htim2.Instance->ARR / 100 * Duty);
	// Start PWM
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

}
void func_invalid(int para_count, char **para){
	// TODO: whether or not
	return;
}

CmdEntry cmd_table[] = {
		{"LED_ON", func_led_on },
		{"LED_OFF", func_led_off },
		{"SET_LED", func_set_led },
		{"UART_TX", func_uart_tx },
		{"PWM_ON",  func_pwm_on  },
		{"INVALID_CMD", func_invalid },
};

enum CMD {
	LED_ON = 0,    // 0
	LED_OFF,       // 1
	SET_LED,       // 2
	UART_TX,       // 3
	PWM_ON,        // 4
	INVALID_CMD,   // 5
};
/* Definition of CMDs END*/


static int is_valid_input(char c){

	if( (c >= 32 && c <= 126) || c == '\r' || c == '\n' || c == '\t'){
		print("%c", (int) c);
		return 1;
	} else if (c == 3){ // ctrl-c
		memset(cmd_buff, 0, sizeof(cmd_buff));
		memset(rx_buff, 0, sizeof(rx_buff));
		cmd_buff_idx = 0;
		print("^C\r\n");
	} else {
		print("\r\nInvalid input: %d\r\n", c);
		int i=0;
		while(i < cmd_buff_idx){
			print("%c", (int) cmd_buff[i]);
			i++;
		}
		memset(rx_buff, 0, sizeof(rx_buff));
	}
	return 0;
}

void process_cmd(void) {

	char rx_char = rx_buff[0];

	if(!is_valid_input(rx_char)){
		return;
	}

	if (rx_char == '\r' || rx_char == '\n' || rx_char == '\t') { // Ready to parse CMD_HEAD,  CMD_PARA
		cmd_buff[cmd_buff_idx] = '\0'; // overwrite last byte

		/* CMD compare BEGIN */
		const char *delim = " "; // only accept " " for separate parameters
		char *token;
		char cmd_head[10];
		char *cmd_para[5];
		int para_count = 0;

		print("\r\nTokens:\n\r");

		// First call to get the first token: CMD_HEAD
		token = strtok(cmd_buff, delim);
		print("first token is %s\r\n", token);
		// Check token in CMD_table
		CmdHandler cmd_handler;
		int cmd_idx=0;
		while(cmd_idx != INVALID_CMD) {
			if(strcmp(token, cmd_table[cmd_idx].cmd_name)==0) {
				strcpy(cmd_head, token);
				cmd_handler = cmd_table[cmd_idx].handler;
				break;
			}
			cmd_idx++;
		}
		if(cmd_idx == INVALID_CMD){
			print("Invalid CMD !\r\n");
			memset(cmd_buff, 0, sizeof(cmd_buff));
			memset(rx_buff, 0, sizeof(rx_buff));
			cmd_buff_idx = 0;
			return;
		}


		token = strtok(NULL, delim);
		while (token != NULL) {
			print("next token is %s\r\n", token);
			// Subsequent calls with NULL to get the next tokens: CMD_PARA
			cmd_para[para_count++] = token;
			token = strtok(NULL, delim);
		}
		cmd_handler(para_count, cmd_para);
		memset(cmd_buff, 0, sizeof(cmd_buff));
		cmd_buff_idx = 0;
		/* CMD compare END */

	} else {
		cmd_buff[cmd_buff_idx++] = rx_char;
	}

	return;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

	if (huart->Instance == USART2) {

		process_cmd();

	//	print("RX callback triggered\r\n");

		HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
	//	HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);


		HAL_UART_Receive_DMA(&huart2, rx_buff, 1);
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
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
