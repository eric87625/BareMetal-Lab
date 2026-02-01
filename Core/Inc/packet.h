/*
 * packet.h
 *
 *  Created on: Jan 29, 2026
 *      Author: pega_user
 */

#ifndef INC_PACKET_H_
#define INC_PACKET_H_

#include <stdint.h>
#include "main.h"    // 包含了 HAL 所有 module 的 def

/* ===== Protocol define ===== */
#define CMD_HEADER  0xAA

/* ===== Packet API ===== */
void print_packet(uint8_t *buf, uint8_t len, const char *msg);

uint8_t build_packet(uint8_t *buf, uint8_t cmd_enum, uint8_t *params,
        uint8_t param_len);

void uart_send_bytes(UART_HandleTypeDef *huart, uint8_t *buf, uint8_t len);

int parse_packet(uint8_t *buf, uint8_t len);

/* ===== Streaming parser ===== */
typedef enum
{
    PKT_WAIT_HEADER,
    PKT_WAIT_LEN,
    PKT_WAIT_PAYLOAD,
    PKT_WAIT_CSUM
} pkt_state_t;

typedef struct
{
    pkt_state_t state;
    uint8_t buf[64];
    uint8_t idx;
    uint8_t payload_len;
} PacketParser;

void packet_parser_feed(PacketParser *p, uint8_t byte);

#endif /* INC_PACKET_H_ */
