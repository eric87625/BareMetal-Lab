#ifndef INC_LOAD_TASK_H_
#define INC_LOAD_TASK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void load_task_start(void);
uint8_t load_task_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_LOAD_TASK_H_ */
