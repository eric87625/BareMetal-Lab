/*
 * console.h
 *
 *  Created on: Jan 29, 2026
 *      Author: YC Lin
 */

#ifndef INC_CONSOLE_H_
#define INC_CONSOLE_H_

#include <stdarg.h>
#include <stdint.h>

void console_init(void *uart_handle);
int print(const char *fmt, ...);

#endif /* INC_CONSOLE_H_ */
