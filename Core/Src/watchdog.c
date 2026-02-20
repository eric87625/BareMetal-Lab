/*
 * watchdog.c
 *
 * Advanced IWDG stability utilities:
 * - Configure ~2s timeout
 * - Refresh from main loop only
 * - Detect and report watchdog reset
 */

#include "watchdog.h"

#include "main.h"     // HAL / RCC flags / IWDG
#include "console.h"  // print() (used for reset reason warning)

extern IWDG_HandleTypeDef hiwdg;

static uint8_t s_watchdog_started = 0;

void Watchdog_Init(void)
{
    /*
     * IWDG timeout formula:
     *   T = (Reload + 1) / (LSI / Prescaler)
     * LSI is typically ~32 kHz (device dependent).
     * With Prescaler=32 and Reload=1999:
     *   T ≈ (2000) / (32000 / 32) = 2.0 s
     */
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    hiwdg.Init.Window = IWDG_WINDOW_DISABLE;
    hiwdg.Init.Reload = 1999;

    if (HAL_IWDG_Init(&hiwdg) == HAL_OK)
    {
        s_watchdog_started = 1;
    }
    else
    {
        s_watchdog_started = 0;
        /* If watchdog init fails, treat it as a fatal configuration error. */
        Error_Handler();
    }
}

void Watchdog_Refresh(void)
{
    /* In CubeMX-generated projects, IWDG might be initialized by MX_IWDG_Init()
     * (not via Watchdog_Init()). In that case, still allow refresh to prevent
     * unintended resets after the RTOS scheduler starts.
     */
    if (!s_watchdog_started)
    {
        if (HAL_IWDG_Refresh(&hiwdg) == HAL_OK)
        {
            s_watchdog_started = 1;
        }
        return;
    }

    (void)HAL_IWDG_Refresh(&hiwdg);
}

void System_Check_Reset_Reason(void)
{
    /* Check watchdog reset flag at early boot.
     * Must clear flags after evaluation.
     */
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0U)
    {
        print("\r\n\r\n[SYSTEM] Critical Error: Watchdog Timeout Detected! System Recovered.\r\n\r\n");
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();
}

void System_Simulate_Deadlock(void)
{
    /* Intentionally stop all progress to validate watchdog recovery.
     * We do NOT refresh IWDG here.
     */
    __disable_irq();
    while (1)
    {
        /* deadlock */
    }
}
