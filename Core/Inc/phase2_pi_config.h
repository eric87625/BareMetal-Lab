/*
 * phase2_pi_config.h
 *
 * Phase2: Priority Inversion + Mutex Behavior experiment parameters.
 * Keep all tunables in this single header.
 */

#ifndef INC_PHASE2_PI_CONFIG_H_
#define INC_PHASE2_PI_CONFIG_H_

/* --------------------------- Mode selection --------------------------- */

#define MODE_MUTEX_PI        1
#define MODE_BINARY_NO_PI    2

/* Compile-time selection:
 * - MODE_MUTEX_PI      : FreeRTOS mutex with priority inheritance
 * - MODE_BINARY_NO_PI  : Binary semaphore used as mutex (no PI)
 */
#ifndef PI_MODE
#define PI_MODE MODE_MUTEX_PI
#endif

/* --------------------------- Experiment knobs ------------------------- */

/* Realistic profile:
 * 0: legacy deterministic behavior (fixed LOW_HOLD_MS and fixed MEDIUM_SPIN_FACTOR)
 * 1: add small per-iteration jitter to better mimic real systems (timing/load are not perfectly periodic)
 */
#ifndef PHASE2_REALISTIC_PROFILE
#define PHASE2_REALISTIC_PROFILE  1
#endif

/* Low task holds the lock for this many ms (tick-based delay). */
#ifndef LOW_HOLD_MS
#define LOW_HOLD_MS          50
#endif

/* When realistic profile is enabled, Low's hold time becomes:
 *   LOW_HOLD_MS +/- LOW_HOLD_JITTER_MS (clamped to >= 0)
 */
#ifndef LOW_HOLD_JITTER_MS
#define LOW_HOLD_JITTER_MS   5
#endif

/* Optional: de-phase the schedule a bit by adding a small jitter before each iteration.
 * High will osDelay(0..HIGH_START_JITTER_MS) before kicking Low.
 */
#ifndef HIGH_START_JITTER_MS
#define HIGH_START_JITTER_MS  3
#endif

/* Busy-loop inner work per medium-spin step (load indicator). */
#ifndef MEDIUM_SPIN_FACTOR
#define MEDIUM_SPIN_FACTOR   10000
#endif

/* When realistic profile is enabled, Medium's spin factor is randomized each iteration:
 *   uniform in [MEDIUM_SPIN_FACTOR_MIN, MEDIUM_SPIN_FACTOR_MAX]
 */
#ifndef MEDIUM_SPIN_FACTOR_MIN
#define MEDIUM_SPIN_FACTOR_MIN  8000
#endif

#ifndef MEDIUM_SPIN_FACTOR_MAX
#define MEDIUM_SPIN_FACTOR_MAX  14000
#endif

/* Optional extra CPU work inside Low's critical section.
 * This models real "hold the lock while doing compute" behavior.
 * Set to 0 to disable.
 */
#ifndef LOW_CRITICAL_SPIN_STEPS
#define LOW_CRITICAL_SPIN_STEPS  0
#endif

#ifndef LOW_CRITICAL_SPIN_FACTOR
#define LOW_CRITICAL_SPIN_FACTOR  5000
#endif

/* Number of CSV records to emit. */
#ifndef ITERATION_COUNT
#define ITERATION_COUNT      500
#endif

/* Print STATS every N iterations (STATS lines are not CSV rows). */
#ifndef STATS_WINDOW
#define STATS_WINDOW         50
#endif

/* Medium task starvation guard:
 * In MODE_BINARY_NO_PI, if Medium never blocks, Low may never run to release
 * the lock (hard starvation). Medium therefore sleeps 1 tick every
 * MEDIUM_PAUSE_EVERY_TICKS while High is waiting.
 */
#ifndef MEDIUM_PAUSE_EVERY_TICKS
#define MEDIUM_PAUSE_EVERY_TICKS  20
#endif

#endif /* INC_PHASE2_PI_CONFIG_H_ */
