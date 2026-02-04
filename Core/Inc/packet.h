/*
 * packet.h
 *
 *  Created on: Jan 29, 2026
 *      Author: YC Lin
 */

#ifndef INC_PACKET_H_
#define INC_PACKET_H_

#include <stdint.h>
#include "cmd.h"     // include CMD_ID
#include "main.h"    // Includes HAL definitions for all modules

/* ============================================================
 * Packet Format Definition
 * ============================================================
 *
 *  Packet Layout (Byte Stream):
 *
 *      +--------+--------+--------+-------------+----------+
 *      | HEADER | LENGTH |  CMD   |   PAYLOAD   | CHECKSUM |
 *      +--------+--------+--------+-------------+----------+
 *        1 byte   1 byte   1 byte    N bytes       1 byte
 *
 *  ------------------------------------------------------------
 *  FIELD DESCRIPTION
 *  ------------------------------------------------------------
 *
 *  HEADER   :
 *      Fixed packet start byte.
 *      Used to indicate the beginning of a packet.
 *
 *      Value:
 *          CMD_HEADER (0xAA)
 *
 *  LENGTH   :
 *      Payload length in bytes.
 *      This length includes:
 *          - CMD field (1 byte)
 *          - PAYLOAD field (N bytes)
 *
 *      Example:
 *          LENGTH = 0x03
 *          => CMD (1) + PAYLOAD (2)
 *
 *  CMD      :
 *      Command ID.
 *      Identifies the operation or message type.
 *
 *  PAYLOAD  :
 *      Optional command parameters.
 *      Size = LENGTH - 1 bytes.
 *
 *  CHECKSUM :
 *      8-bit checksum for packet validation.
 *      Calculated as the sum of all bytes except checksum itself:
 *
 *          CHECKSUM =
 *              HEADER +
 *              LENGTH +
 *              CMD +
 *              PAYLOAD bytes
 *
 *      Only the lowest 8 bits are kept.
 *
 *  ------------------------------------------------------------
 *  PACKET LENGTH
 *  ------------------------------------------------------------
 *
 *      Total Packet Length =
 *          HEADER (1) +
 *          LENGTH (1) +
 *          PAYLOAD (LENGTH bytes) +
 *          CHECKSUM (1)
 *
 *      => TOTAL = LENGTH + 3 bytes
 *
 *  ------------------------------------------------------------
 *  EXAMPLE PACKET
 *  ------------------------------------------------------------
 *
 *      CMD_HEADER = 0xAA
 *      CMD        = 0x01
 *      PAYLOAD    = { 0x10, 0x20 }
 *
 *      LENGTH  = 1 (CMD) + 2 (PAYLOAD) = 0x03
 *
 *      CHECKSUM = 0xAA + 0x03 + 0x01 + 0x10 + 0x20 = 0x36
 *
 *      Byte Stream:
 *          AA 03 01 10 20 36
 *
 * ============================================================
 */

/* ===== Protocol define ===== */
#define CMD_HEADER  0xAA

/* ===== Packet API ===== */
void print_packet(uint8_t *buf, uint8_t len, const char *msg);

uint8_t build_packet(uint8_t *buf, CMD_ID cmd_enum, uint8_t *params, uint8_t param_len);

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

void packet_parser_init(PacketParser *p);
void packet_parser_feed(PacketParser *p, uint8_t byte);

#endif /* INC_PACKET_H_ */
