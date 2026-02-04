/*
 * uart_rb.c
 *
 *  Created on: Jan 29, 2026
 *      Author: YC Lin
 */
#include "uart_rb.h"

void rb_init(RingBuffer *rb)
{
    rb->head = 0;
    rb->tail = 0;
    memset(rb->buf, 0, sizeof(rb->buf));
}
void rb_push(RingBuffer *rb, uint8_t data)
{
    rb->buf[rb->head & RB_MASK] = data;
    rb->head++;
}

int rb_pop(RingBuffer *rb, uint8_t *out)
{
    if (rb->head == rb->tail)
        return 0;

    *out = rb->buf[rb->tail & RB_MASK];
    rb->tail++;
    return 1;
}

