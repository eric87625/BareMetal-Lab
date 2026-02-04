/*
 * console.c
 *
 *  Created on: Jan 29, 2026
 *      Author: YC Lin
 */
#include "main.h"    // Includes HAL definitions for all modules
#include "console.h"
#include <stdio.h>

static UART_HandleTypeDef *console_uart = NULL;

void console_init(void *uart_handle)
{
    console_uart = (UART_HandleTypeDef *)uart_handle;
}

int print(const char *str, ...) {

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
	int ret = HAL_UART_Transmit(console_uart, (uint8_t*) buf, cur_buf_len, HAL_MAX_DELAY) == HAL_OK;
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
