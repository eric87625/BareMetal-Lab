/*
 * phase2_pi.c
 *
 * Phase2: Priority Inversion + Mutex Behavior experiment.
 *
 * Three tasks:
 * - Low    (osPriorityLow)     : takes lock first and holds it
 * - Medium (osPriorityNormal)  : CPU load while High is blocked
 * - High   (osPriorityHigh)    : waits for lock and logs CSV
 */

#include "phase2_pi.h"
#include "phase2_pi_config.h"

#include "cmsis_os2.h"
#include "main.h"
#include "console.h"
#include "watchdog.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <stdint.h>

/* --------------------------- Thread flags ----------------------------- */

#define FLAG_LOW_START        (1UL << 0)
#define FLAG_HIGH_LOW_LOCKED  (1UL << 1)
#define FLAG_MEDIUM_START     (1UL << 2)

/* --------------------------- Globals ---------------------------------- */

static SemaphoreHandle_t g_lock = NULL;

static osThreadId_t g_low_handle = NULL;
static osThreadId_t g_medium_handle = NULL;
static osThreadId_t g_high_handle = NULL;

static volatile uint32_t g_iter = 0U;
static volatile uint32_t g_low_lock_tick = 0U;
static volatile uint32_t g_low_unlock_tick = 0U;
static volatile uint32_t g_medium_spin_count = 0U;
static volatile uint8_t g_high_waiting = 0U;

/* Dynamic per-iteration knobs (set by High, consumed by Low/Medium). */
static volatile uint32_t g_low_hold_ms = (uint32_t) LOW_HOLD_MS;
static volatile uint32_t g_medium_spin_factor = (uint32_t) MEDIUM_SPIN_FACTOR;

/* Simple PRNG (xorshift32) to add small jitter without pulling in libc rand(). */
static uint32_t g_rng_state = 2463534242UL;

static uint32_t rng_next_u32(void)
{
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

static uint32_t rng_range_u32(uint32_t lo, uint32_t hi)
{
    if (hi <= lo)
    {
        return lo;
    }

    uint32_t span = (hi - lo) + 1UL;
    return lo + (rng_next_u32() % span);
}

static void do_spin_steps(uint32_t steps, uint32_t factor)
{
    for (uint32_t s = 0; s < steps; s++)
    {
        for (uint32_t i = 0; i < factor; i++)
        {
            __NOP();
        }
    }
}

/* --------------------------- Stats ------------------------------------ */

typedef struct
{
    uint32_t count;
    uint32_t min;
    uint32_t max;
    uint64_t sum;
} PiStats;

static void stats_reset(PiStats *s)
{
    s->count = 0U;
    s->min = 0xFFFFFFFFUL;
    s->max = 0U;
    s->sum = 0ULL;
}

static void stats_add(PiStats *s, uint32_t v)
{
    if (v < s->min)
        s->min = v;
    if (v > s->max)
        s->max = v;
    s->sum += (uint64_t) v;
    s->count++;
}

/* --------------------------- Tasks ------------------------------------ */

static const osThreadAttr_t g_low_attr = {
    .name = "piLow",
    .priority = (osPriority_t) osPriorityLow,   /* REQUIRED: explicit priority */
    .stack_size = 256 * 4
};

static const osThreadAttr_t g_medium_attr = {
    .name = "piMed",
    .priority = (osPriority_t) osPriorityNormal, /* REQUIRED: explicit priority */
    .stack_size = 256 * 4
};

static const osThreadAttr_t g_high_attr = {
    .name = "piHigh",
    .priority = (osPriority_t) osPriorityHigh,  /* REQUIRED: explicit priority */
    .stack_size = 512 * 4
};

static void low_task(void *argument)
{
    (void) argument;

    for (;;)
    {
        (void) osThreadFlagsWait(FLAG_LOW_START, osFlagsWaitAny, osWaitForever);

        if (g_lock == NULL)
        {
            continue;
        }

        (void) xSemaphoreTake(g_lock, portMAX_DELAY);
        g_low_lock_tick = osKernelGetTickCount();

        /* Tell High that Low has acquired the lock and is holding it. */
        if (g_high_handle != NULL)
        {
            (void) osThreadFlagsSet(g_high_handle, FLAG_HIGH_LOW_LOCKED);
        }

        /* Model real work done while holding a lock (optional). */
        if (((uint32_t) LOW_CRITICAL_SPIN_STEPS) != 0U)
        {
            do_spin_steps((uint32_t) LOW_CRITICAL_SPIN_STEPS, (uint32_t) LOW_CRITICAL_SPIN_FACTOR);
        }

        /* Hold time (legacy: fixed LOW_HOLD_MS; realistic: per-iteration set by High). */
        osDelay((uint32_t) g_low_hold_ms);

        g_low_unlock_tick = osKernelGetTickCount();
        (void) xSemaphoreGive(g_lock);
    }
}

static void medium_task(void *argument)
{
    (void) argument;

    for (;;)
    {
        (void) osThreadFlagsWait(FLAG_MEDIUM_START, osFlagsWaitAny, osWaitForever);

        uint32_t last_tick = osKernelGetTickCount();
        uint32_t tick_run = 0U;

        while (g_high_waiting != 0U)
        {
            /* CPU load while High is waiting (legacy: fixed; realistic: per-iteration set by High). */
            for (uint32_t i = 0; i < (uint32_t) g_medium_spin_factor; i++)
            {
                __NOP();
            }

            g_medium_spin_count++;

            uint32_t now = osKernelGetTickCount();
            if (now != last_tick)
            {
                tick_run += (uint32_t) (now - last_tick);
                last_tick = now;

                if (tick_run >= (uint32_t) MEDIUM_PAUSE_EVERY_TICKS)
                {
                    tick_run = 0U;
                    osDelay(1U);
                }
            }
        }
    }
}

static void high_task(void *argument)
{
    (void) argument;

    /* Ensure watchdog is serviced even if UART printing dominates CPU time. */
    Watchdog_Refresh();

    /* Seed jitter PRNG from current tick (fine for experiment use). */
    g_rng_state ^= osKernelGetTickCount();

    /* Emit one-time configuration line (not a CSV row). */
    Watchdog_Refresh();
    print(
        "CFG,"
        "pi_mode=%u,"
        "realistic=%u,"
        "low_hold_ms=%u,"
        "low_hold_jitter_ms=%u,"
        "high_start_jitter_ms=%u,"
        "medium_spin_factor=%u,"
        "medium_spin_min=%u,"
        "medium_spin_max=%u,"
        "low_critical_steps=%u,"
        "low_critical_factor=%u,"
        "iteration_count=%u,"
        "stats_window=%u,"
        "medium_pause_ticks=%u"
        "\r\n",
        (unsigned int) PI_MODE,
        (unsigned int) PHASE2_REALISTIC_PROFILE,
        (unsigned int) LOW_HOLD_MS,
        (unsigned int) LOW_HOLD_JITTER_MS,
        (unsigned int) HIGH_START_JITTER_MS,
        (unsigned int) MEDIUM_SPIN_FACTOR,
        (unsigned int) MEDIUM_SPIN_FACTOR_MIN,
        (unsigned int) MEDIUM_SPIN_FACTOR_MAX,
        (unsigned int) LOW_CRITICAL_SPIN_STEPS,
        (unsigned int) LOW_CRITICAL_SPIN_FACTOR,
        (unsigned int) ITERATION_COUNT,
        (unsigned int) STATS_WINDOW,
        (unsigned int) MEDIUM_PAUSE_EVERY_TICKS
    );

    Watchdog_Refresh();

    /* CSV header: print exactly once at experiment start. */
    print("iter,mode,high_wait_ticks,low_hold_ticks,medium_spin_count\r\n");

    Watchdog_Refresh();

    PiStats stats;
    stats_reset(&stats);

    for (g_iter = 1U; g_iter <= (uint32_t) ITERATION_COUNT; g_iter++)
    {
        /* Feed watchdog regularly; do not emit any extra UART text. */
        Watchdog_Refresh();

#if (PHASE2_REALISTIC_PROFILE != 0)
        /* De-phase the schedule a bit (prevents perfectly periodic lockstep). */
        if ((uint32_t) HIGH_START_JITTER_MS != 0U)
        {
            osDelay(rng_range_u32(0U, (uint32_t) HIGH_START_JITTER_MS));
        }

        /* Per-iteration Low hold time jitter (ms). */
        uint32_t base_ms = (uint32_t) LOW_HOLD_MS;
        uint32_t jitter = (uint32_t) LOW_HOLD_JITTER_MS;
        uint32_t hold_ms = base_ms;
        if (jitter != 0U)
        {
            uint32_t span = (2U * jitter) + 1U;
            uint32_t r = rng_next_u32() % span; /* 0..2*jitter */
            int32_t delta = (int32_t) r - (int32_t) jitter; /* -jitter..+jitter */
            int32_t tmp = (int32_t) base_ms + delta;
            hold_ms = (tmp < 0) ? 0U : (uint32_t) tmp;
        }
        g_low_hold_ms = hold_ms;

        /* Per-iteration Medium burst intensity. */
        g_medium_spin_factor = rng_range_u32((uint32_t) MEDIUM_SPIN_FACTOR_MIN, (uint32_t) MEDIUM_SPIN_FACTOR_MAX);
#else
        g_low_hold_ms = (uint32_t) LOW_HOLD_MS;
        g_medium_spin_factor = (uint32_t) MEDIUM_SPIN_FACTOR;
#endif

        g_medium_spin_count = 0U;

        if (g_low_handle != NULL)
        {
            (void) osThreadFlagsSet(g_low_handle, FLAG_LOW_START);
        }

        /* Wait until Low confirms it has the lock (Scenario step 1). */
        (void) osThreadFlagsWait(FLAG_HIGH_LOW_LOCKED, osFlagsWaitAny, osWaitForever);

        /* Scenario step 3: High tries to lock and measure wait ticks. */
        uint32_t wait_start = osKernelGetTickCount();

        g_high_waiting = 1U;
        if (g_medium_handle != NULL)
        {
            (void) osThreadFlagsSet(g_medium_handle, FLAG_MEDIUM_START);
        }

        (void) xSemaphoreTake(g_lock, portMAX_DELAY);
        uint32_t wait_end = osKernelGetTickCount();

        g_high_waiting = 0U;

        /* Keep lock hold minimal; release immediately, and log outside lock. */
        (void) xSemaphoreGive(g_lock);

        uint32_t high_wait_ticks = (uint32_t) (wait_end - wait_start);
        uint32_t low_hold_ticks = (uint32_t) (g_low_unlock_tick - g_low_lock_tick);
        uint32_t medium_spin_count = (uint32_t) g_medium_spin_count;

        print("%lu,%u,%lu,%lu,%lu\r\n",
              (unsigned long) g_iter,
              (unsigned int) PI_MODE,
              (unsigned long) high_wait_ticks,
              (unsigned long) low_hold_ticks,
              (unsigned long) medium_spin_count);

          /* Printing is blocking; refresh again after TX work. */
          Watchdog_Refresh();

        stats_add(&stats, high_wait_ticks);

        if ((g_iter % (uint32_t) STATS_WINDOW) == 0U)
        {
            uint32_t avg = (stats.count != 0U) ? (uint32_t) (stats.sum / stats.count) : 0U;
            print("STATS,mode=%u,min=%lu,max=%lu,avg=%lu\r\n",
                  (unsigned int) PI_MODE,
                  (unsigned long) stats.min,
                  (unsigned long) stats.max,
                  (unsigned long) avg);

            Watchdog_Refresh();
            stats_reset(&stats);
        }
    }

    /* Stop after ITERATION_COUNT to keep output deterministic. */
    osThreadExit();
}

/* --------------------------- Public API -------------------------------- */

void phase2_pi_start(void)
{
    if (g_lock != NULL)
    {
        return;
    }

    if ((uint32_t) PI_MODE == (uint32_t) MODE_MUTEX_PI)
    {
        g_lock = xSemaphoreCreateMutex();
    }
    else
    {
        g_lock = xSemaphoreCreateBinary();
        if (g_lock != NULL)
        {
            (void) xSemaphoreGive(g_lock);
        }
    }

    /* Create tasks (order does not matter before osKernelStart). */
    g_low_handle = osThreadNew(low_task, NULL, &g_low_attr);
    g_medium_handle = osThreadNew(medium_task, NULL, &g_medium_attr);
    g_high_handle = osThreadNew(high_task, NULL, &g_high_attr);
}
