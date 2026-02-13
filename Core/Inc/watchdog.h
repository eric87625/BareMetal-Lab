/*
 * watchdog.h
 *
 * Advanced IWDG stability utilities:
 * - Periodic refresh (kick)
 * - Reset reason check & logging
 * - Deadlock simulation for validation
 */

#ifndef INC_WATCHDOG_H_
#define INC_WATCHDOG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize watchdog. Must be called after HAL_Init() and clock setup.
 * Timeout is configured to ~2 seconds (LSI dependent).
 */
void Watchdog_Init(void);

/* Refresh watchdog counter. Call periodically in main loop.
 * Safe to call even if watchdog wasn't initialized yet.
 */
void Watchdog_Refresh(void);

/* Check reset source at boot and log warning if watchdog reset occurred.
 * Must clear reset flags after evaluation.
 * Call early in main() after console is ready.
 */
void System_Check_Reset_Reason(void);

/* Enter an intentional deadlock to verify IWDG recovery.
 * This function does not return.
 */
void System_Simulate_Deadlock(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_WATCHDOG_H_ */
