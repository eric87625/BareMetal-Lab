#ifndef INC_LATENCY_H_
#define INC_LATENCY_H_

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void latency_init(TIM_HandleTypeDef *timer_handle, UART_HandleTypeDef *log_uart);
void latency_on_tim_period_elapsed_isr(TIM_HandleTypeDef *htim);
void latency_start_logging_task(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_LATENCY_H_ */
