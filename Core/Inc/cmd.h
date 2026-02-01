/*
 * cmd.h
 *
 *  Created on: Jan 30, 2026
 *      Author: pega_user
 */

#ifndef INC_CMD_H_
#define INC_CMD_H_

#include <stdint.h>

/* ---------- CMD enum ---------- */
enum CMD
{
    LED_ON = 0,
    LED_OFF,
    SET_LED,
    UART_TX,
    PWM_ON,
    INVALID_CMD,
};

/* ---------- Public APIs ---------- */
void process_cmd(void);
void handle_binary_cmd(uint8_t *payload, uint8_t length);




#endif /* INC_CMD_H_ */
