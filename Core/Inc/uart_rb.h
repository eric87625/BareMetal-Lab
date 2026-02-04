/*
 * uart_rb.h
 *
 *  Created on: Jan 29, 2026
 *      Author: YC Lin
 */

#ifndef INC_UART_RB_H_
#define INC_UART_RB_H_


#include <stdint.h>

#define RB_SIZE 128
#define RB_MASK (RB_SIZE - 1)

typedef struct {
    uint8_t buf[RB_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

/* API */
void rb_init(RingBuffer *rb);
void rb_push(RingBuffer *rb, uint8_t data);
int  rb_pop(RingBuffer *rb, uint8_t *out);





#endif /* INC_UART_RB_H_ */
