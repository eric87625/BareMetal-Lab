/*
 * packet.c
 *
 *  Created on: Jan 29, 2026
 *      Author: YC Lin
 */
#include "packet.h"

#include <string.h>   // memcpy, memset
#include <stddef.h>   // NULL
#include "console.h"  // print()
#include "cmd.h"  // handle_binary_cmd()


void packet_parser_init(PacketParser *p)
{
    p->state = PKT_WAIT_HEADER; // Start by waiting for the header
    p->idx = 0;                 // Reset buffer index
    p->payload_len = 0;         // Reset payload length
    memset(p->buf, 0, sizeof(p->buf)); // Optionally clear the buffer
}


/* ----------------------
 * Hex dump utility
 * ---------------------- */
void print_packet(uint8_t *buf, uint8_t len, const char *msg)
{
    print("%s (%d bytes): ", msg, len);
    for (uint8_t i = 0; i < len; i++)
    {
        print("%02X ", buf[i]);
    }
    print("\r\n");
}

/* ----------------------
 * Build packet
 * ---------------------- */
uint8_t build_packet(uint8_t *buf, CMD_ID cmd_enum, uint8_t *params,
        uint8_t param_len)
{
    uint8_t idx = 0;
    buf[idx++] = CMD_HEADER;

    uint8_t payload_len = 1 + param_len; // cmd_id + params
    buf[idx++] = payload_len;

    buf[idx++] = cmd_enum;

    if (param_len > 0 && params != NULL)
    {
        memcpy(&buf[idx], params, param_len);
        idx += param_len;
    }

    uint8_t csum = CMD_HEADER + payload_len + cmd_enum;
    for (uint8_t i = 0; i < param_len; i++)
        csum += params[i];

    buf[idx++] = csum & 0xFF;

    print_packet(buf, idx, "Built Packet");
    return idx;
}

/* ----------------------
 * Send packet
 * ---------------------- */
void uart_send_bytes(UART_HandleTypeDef *huart, uint8_t *buf, uint8_t len)
{
    HAL_UART_Transmit(huart, buf, len, HAL_MAX_DELAY);
}

/* ----------------------
 * Parse packet
 * ---------------------- */
int parse_packet(uint8_t *buf, uint8_t len)
{
    if (len < 3)
        return -1;

    if (buf[0] != CMD_HEADER)
        return -2;

    uint8_t payload_len = buf[1];
    if (payload_len + 3 != len)
        return -3;

    uint8_t csum = buf[0] + buf[1];
    for (uint8_t i = 0; i < payload_len; i++)
    {
        csum += buf[2 + i];
    }

    if ((csum & 0xFF) != buf[len - 1])
        return -4;

    print_packet(buf, len, "Parse Packet");
    return 0;
}

/* ----------------------
 * Streaming parser
 * ---------------------- */
void packet_parser_feed(PacketParser *p, uint8_t byte)
{
    switch (p->state)
    {

    case PKT_WAIT_HEADER:
        print("case %d: PKT_WAIT_HEADER\r\n", PKT_WAIT_HEADER);
        if (byte == CMD_HEADER)
        {
            p->buf[0] = byte;
            p->idx = 1;
            p->state = PKT_WAIT_LEN;
        }
        break;

    case PKT_WAIT_LEN:
        print("case %d: PKT_WAIT_LEN\r\n", PKT_WAIT_LEN);
        p->payload_len = byte;
        p->buf[p->idx++] = byte;
        p->state = PKT_WAIT_PAYLOAD;

        break;

    case PKT_WAIT_PAYLOAD:
        print("case %d: PKT_WAIT_PAYLOAD\r\n", PKT_WAIT_PAYLOAD);
        p->buf[p->idx++] = byte;
        if (p->idx == p->payload_len + 2)
            p->state = PKT_WAIT_CSUM;

        break;

    case PKT_WAIT_CSUM:
        print("case %d: PKT_WAIT_CSUM\r\n", PKT_WAIT_CSUM);
        p->buf[p->idx++] = byte;
        if (parse_packet(p->buf, p->idx) == 0)
        {
            handle_binary_cmd(&p->buf[2], p->payload_len);
        }
        p->state = PKT_WAIT_HEADER;
        p->idx = 0;

        break;
    }
}
