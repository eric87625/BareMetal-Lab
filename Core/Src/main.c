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
#include <string.h>
#include <stdio.h>  // Required for snprintf, sscanf()
#include <stdarg.h> // Required for variable argument handling
#include <stdlib.h>


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t tx_buff[] = { 65, 66, 67, 68, 69, 70, 71, 72, 73, 74 }; //ABCDEFGHIJ in ASCII code
uint8_t rx_buff[10];
char cmd_buff[30];
int cmd_buff_idx;
//static int pcount;

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

//static int print(char *str, ...) {
//
//    char buf[256];
//    va_list args; // Declare a va_list to hold the variable arguments
//
//    va_start(args, str); // Initialize va_list with the last fixed argument (str)
//    int cur_buf_len = vsnprintf(buf, sizeof(buf), str, args); // Use vsnprintf for variable arguments
//    va_end(args); // Clean up the va_list
//
//    if (cur_buf_len < 0) {
//        // Handle error during formatting
//        return -1;
//    }
//
//    // Ensure null termination in case of truncation by snprintf
//    if (cur_buf_len >= sizeof(buf)) {
//        buf[sizeof(buf) - 1] = '\0';
//    }
//
//    // Print the formatted string to uart output
////  HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart2, (uint8_t *)buf, len);
////  HAL_StatusTypeDef ret = HAL_UART_Transmit_IT(&huart2, (uint8_t *)buf, len);
//	int ret = HAL_UART_Transmit(&huart2, (uint8_t*) buf, cur_buf_len, HAL_MAX_DELAY) == HAL_OK;
//	return ret;
//}

static int print(char *str, ...) {

	int count = 0;
	int cur_buf_len = 0;
	char buf[256];
	int len = strlen(str);

	memset(buf, 0, sizeof(buf));

//	print_val("pcount = %d\r\n", pcount++);

	va_list args;
	va_start(args, str);

	for (int i = 0; i < len; i++) {
		if (str[i] == '%') {
			i++;

			/* Print variable to buffer BEGIN */
			switch (str[i]) {
			case 'd':
				int val = va_arg(args, int);

				int prev_len = strlen(buf);
				itoa(val, buf + cur_buf_len, 10);
				int post_len = strlen(buf);
				cur_buf_len += post_len - prev_len;

//				cur_buf_len += snprintf(buf + cur_buf_len, sizeof(buf) - cur_buf_len, "%d", val);
				break;
			case 'c':
				int ch = va_arg(args, int);

				buf[cur_buf_len] = ch;
				cur_buf_len ++;

//				cur_buf_len += snprintf(buf + cur_buf_len, sizeof(buf) - cur_buf_len, "%c", ch);
				break;
			case 's':
				char *string = va_arg(args, char*);

				strcpy(buf + cur_buf_len, string);
				cur_buf_len += strlen(string);

//				cur_buf_len += snprintf(buf + cur_buf_len, sizeof(buf) - cur_buf_len, "%s", string);
				break;
			}
			/* Print variable to buffer END */
			count++;
		} else {
			buf[cur_buf_len] = str[i];
			cur_buf_len ++;
//			cur_buf_len += snprintf(buf + cur_buf_len, sizeof(buf) - cur_buf_len, "%c", str[i]);
		}
	}
	va_end(args);

//  print_val("the number of variables within print is %d\r\n", count);

//  HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart2, (uint8_t *)buf, len);
//  HAL_StatusTypeDef ret = HAL_UART_Transmit_IT(&huart2, (uint8_t *)buf, len);
	int ret = HAL_UART_Transmit(&huart2, (uint8_t*) buf, cur_buf_len, HAL_MAX_DELAY) == HAL_OK;
	return ret;
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

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
	/* USER CODE BEGIN 2 */
	cmd_buff_idx = 0;
//	pcount = 0;

	int number_of_dogs = 5;
	char *dogs_name = "George";
	print("There are %d dogs, all named %s !\r\n", number_of_dogs, dogs_name);
	print("There are %d dogs, all named %s !\r\n", number_of_dogs, dogs_name);

	HAL_UART_Receive_DMA(&huart2, rx_buff, 1);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
//	HAL_UART_Transmit_IT(&huart2, tx_buff, 10);
//	HAL_Delay(10000);
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

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
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

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
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */
	memset(rx_buff, 0, sizeof(rx_buff));
	memset(cmd_buff, 0, sizeof(cmd_buff));
	/* USER CODE END USART2_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel1_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
	/* DMA1_Channel2_3_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
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

void func_invalid(int para_count, char **para){
	// TODO: weather or not
	return;
}

CmdEntry cmd_table[] = {
		{"LED_ON", func_led_on },
		{"LED_OFF", func_led_off },
		{"SET_LED", func_set_led },
		{"UART_TX", func_uart_tx },
		{"INVALID_CMD", func_invalid },
};

enum CMD {
	LED_ON = 0,    // 0
	LED_OFF,       // 1
	SET_LED,       // 2
	UART_TX,       // 3
	INVALID_CMD,   // 4
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
//  print("Hello! HAL_UART_RxCpltCallback is called. pcount = %d\r\n", pcount++);
	process_cmd(); //You need to toggle a breakpoint on this line!

	HAL_UART_Receive_DMA(&huart2, rx_buff, 1);
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
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
