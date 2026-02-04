/*
 * uart_test.h
 *
 *  Created on: Feb 1, 2026
 *      Author: YC Lin
 */

#ifndef INC_UART_TEST_H_
#define INC_UART_TEST_H_

#include <stdint.h>
#include "main.h"
#include "packet.h"
#include "uart_rb.h"

/* Test case IDs */
typedef enum {
    UART_TEST_SINGLE_CMD = 0,
    UART_TEST_CMD_WITH_IDLE,
    UART_TEST_CMD_STICKY,      // Multiple commands concatenated together
    UART_TEST_CONTINUOUS_STREAM
} UART_TestCase;

/* Initialize the test module */
void uart_test_init(UART_HandleTypeDef *huart);

/* Run the specified UART test case */
void uart_test_run(UART_TestCase test_case);

/* Simulate DMA/IDLE reception, write data into RingBuffer and parse */
void uart_test_feed_data(uint8_t *data, uint16_t len);

#endif /* INC_UART_TEST_H_ */
