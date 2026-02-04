/*
 * uart_test.c
 *
 *  Created on: Feb 1, 2026
 *      Author: YC Lin
 */
#include "uart_test.h"
#include "console.h"
#include "packet.h"
#include "cmd.h"
#include <string.h>
#include <stdio.h>

#define TEST_PKT_MAX 64

static UART_HandleTypeDef *test_huart;
static RingBuffer test_rb;
static PacketParser test_parser;

void uart_test_init(UART_HandleTypeDef *huart)
{
    test_huart = huart;
    rb_init(&test_rb);
    packet_parser_init(&test_parser);

    print("UART test module initialized\n");
}

/* Feed data into RingBuffer to simulate DMA/IDLE reception */
void uart_test_feed_data(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
        rb_push(&test_rb, data[i]);

    // Simulate the parser processing continuously
    uint8_t ch;
    while (rb_pop(&test_rb, &ch))
        packet_parser_feed(&test_parser, ch);
}

/* -------------------- Test Cases -------------------- */
void uart_test_run(UART_TestCase test_case)
{
    uint8_t packet[TEST_PKT_MAX];
    uint8_t len;
//    uint8_t payload;

    switch (test_case)
    {
    case UART_TEST_SINGLE_CMD:
        print("\r\n=== UART_TEST_SINGLE_CMD ===\r\n");
        len = build_packet(packet, LED_ON, NULL, 0);
        uart_send_bytes(test_huart, packet, len);
        break;

    case UART_TEST_CMD_WITH_IDLE:
        print("\r\n=== UART_TEST_CMD_WITH_IDLE (split packets) ===\r\n");
        // Simulate LED_ON being sent in two parts with idle in between
        len = build_packet(packet, LED_ON, NULL, 0);
        uart_send_bytes(test_huart, packet, 2);      // Send the first two bytes
        print("Delay ~ ~\r\n");
        HAL_Delay(10);
        uart_send_bytes(test_huart, packet+2, len-2);// Then send the remainder
        break;

    case UART_TEST_CMD_STICKY:
        print("\r\n=== UART_TEST_CMD_STICKY (multi packets in one go) ===\r\n");
        // Concatenate LED_ON + SET_LED + UART_TX together
        len = build_packet(packet, LED_ON, NULL, 0);
        uint8_t len2 = build_packet(packet+len, SET_LED, (uint8_t[]){1}, 1);
        uint8_t tx_msg[] = "Hi!\r\n";
        uint8_t len3 = build_packet(packet+len+len2, UART_TX, tx_msg, sizeof(tx_msg));
        uart_send_bytes(test_huart, packet, len+len2+len3);
        break;

    case UART_TEST_CONTINUOUS_STREAM:
        print("\r\n=== UART_TEST_CONTINUOUS_STREAM (burst data) ===\r\n");
        // Send 10 times of LED_ON + UART_TX continuously
        for (int i=0; i<10; i++)
        {
            len = build_packet(packet, LED_ON, NULL, 0);
            uint8_t tx_msg[] = "Test\r\n";
            uint8_t len2 = build_packet(packet+len, UART_TX, tx_msg, sizeof(tx_msg));
            uart_send_bytes(test_huart, packet, len+len2);
        }
        break;

    default:
        print("\r\nUnknown test case\r\n");
        break;
    }
}


