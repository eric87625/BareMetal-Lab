#include "load_task.h"

#include "main.h"
#include "cmsis_os2.h"

#ifndef LOAD_TASK_BUSY_MS
#define LOAD_TASK_BUSY_MS (10U)
#endif

static volatile uint8_t g_load_active = 0U;

static osThreadId_t g_load_task_handle;

static const osThreadAttr_t g_load_task_attr = {
    .name = "loadTask",
    .priority = (osPriority_t) osPriorityAboveNormal,
    .stack_size = 128 * 4
};

static void load_task_entry(void *argument)
{
    (void) argument;

    uint32_t phase_start = osKernelGetTickCount();

    for (;;)
    {
        uint32_t now = osKernelGetTickCount();
        uint32_t elapsed = now - phase_start;

        if (g_load_active == 0U) // idle phase
        {
            if (elapsed >= 5000U) //5s idle, 5s active
            {
                g_load_active = 1U;
                phase_start = now;
            }
            osDelay(50U);
        }
        else
        {
            if (elapsed >= 5000U) //5s active, 5s idle
            {
                g_load_active = 0U;
                phase_start = now;
                osDelay(10U);
                continue;
            }

            uint32_t busy_start = osKernelGetTickCount();
            while ((osKernelGetTickCount() - busy_start) < LOAD_TASK_BUSY_MS) // ms
            {
                __NOP();
            }

            osDelay(1U);
        }
    }
}

void load_task_start(void)
{
    if (g_load_task_handle == NULL)
    {
        g_load_task_handle = osThreadNew(load_task_entry, NULL, &g_load_task_attr);
    }
}

uint8_t load_task_is_active(void)
{
    return g_load_active;
}
