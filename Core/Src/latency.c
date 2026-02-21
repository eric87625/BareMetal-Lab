#include "latency.h"

#include "cmsis_os2.h"
#include "console.h"
#include "load_task.h"

#define LATENCY_RING_SIZE 512U
#define LATENCY_STATS_WINDOW 200U

typedef struct
{
    uint32_t seq;
    uint32_t systick_ms;
    uint8_t load_active;
    uint16_t latency_ticks;
    uint16_t exec_ticks;
    int16_t latency_delta_ticks;
} LatencySample;

typedef struct
{
    uint32_t count;
    uint16_t latency_min;
    uint16_t latency_max;
    uint32_t latency_sum;
    uint16_t exec_min;
    uint16_t exec_max;
    uint32_t exec_sum;
} LatencyStats;

static volatile LatencySample g_ring[LATENCY_RING_SIZE];
static volatile uint16_t g_head = 0U;
static volatile uint16_t g_tail = 0U;
static volatile uint32_t g_overwrite_count = 0U;

static TIM_HandleTypeDef *g_tim = NULL;
static UART_HandleTypeDef *g_log_uart = NULL;

static volatile uint32_t g_seq = 0U;
static volatile uint16_t g_prev_latency_ticks = 0U;

static osThreadId_t g_logging_task_handle;

static const osThreadAttr_t g_logging_task_attr = {
    .name = "latLogTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 256 * 4
};

static uint32_t latency_get_timer_clk_hz(TIM_HandleTypeDef *htim)
{
    (void) htim;

    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t presc = (RCC->CFGR & RCC_CFGR_PPRE) >> RCC_CFGR_PPRE_Pos;

    if (presc >= 0x4U)
    {
        pclk *= 2U;
    }

    return pclk;
}

static void latency_push_from_isr(const LatencySample *sample)
{
    uint16_t next_head = (uint16_t) ((g_head + 1U) % LATENCY_RING_SIZE);

    if (next_head == g_tail)
    {
        g_tail = (uint16_t) ((g_tail + 1U) % LATENCY_RING_SIZE);
        g_overwrite_count++;
    }

    g_ring[g_head] = *sample;
    g_head = next_head;
}

static uint8_t latency_pop(LatencySample *sample)
{
    uint8_t has_data;

    __disable_irq();
    has_data = (g_head != g_tail) ? 1U : 0U;
    if (has_data != 0U)
    {
        *sample = g_ring[g_tail];
        g_tail = (uint16_t) ((g_tail + 1U) % LATENCY_RING_SIZE);
    }
    __enable_irq();

    return has_data;
}

static void latency_stats_init(LatencyStats *stats)
{
    stats->count = 0U;
    stats->latency_min = 0xFFFFU;
    stats->latency_max = 0U;
    stats->latency_sum = 0U;
    stats->exec_min = 0xFFFFU;
    stats->exec_max = 0U;
    stats->exec_sum = 0U;
}

static void latency_stats_update(LatencyStats *stats, const LatencySample *sample)
{
    if (sample->latency_ticks < stats->latency_min)
    {
        stats->latency_min = sample->latency_ticks;
    }
    if (sample->latency_ticks > stats->latency_max)
    {
        stats->latency_max = sample->latency_ticks;
    }

    if (sample->exec_ticks < stats->exec_min)
    {
        stats->exec_min = sample->exec_ticks;
    }
    if (sample->exec_ticks > stats->exec_max)
    {
        stats->exec_max = sample->exec_ticks;
    }

    stats->latency_sum += sample->latency_ticks;
    stats->exec_sum += sample->exec_ticks;
    stats->count++;
}

static void latency_logging_task(void *argument)
{
    (void) argument;

    if (g_log_uart != NULL)
    {
        console_init(g_log_uart);
    }

    uint32_t timer_clk_hz = latency_get_timer_clk_hz(g_tim);
    uint32_t tick_hz = timer_clk_hz / (g_tim->Init.Prescaler + 1U);

    LatencyStats stats;
    latency_stats_init(&stats);

    print("# latency_log_start,timer=%s,tick_hz=%lu\r\n",
          (g_tim->Instance == TIM3) ? "TIM3" : "UNKNOWN",
          tick_hz);
    print("seq,systick_ms,load_active,latency_ticks,latency_us,exec_ticks,exec_us,latency_delta_ticks\r\n");

    for (;;)
    {
        LatencySample sample;

        if (latency_pop(&sample) == 0U)
        {
            osDelay(10U);
            continue;
        }

        latency_stats_update(&stats, &sample);

        uint32_t latency_us_x1000 = (uint32_t) (((uint64_t) sample.latency_ticks * 1000000000ULL) / tick_hz);
        uint32_t exec_us_x1000 = (uint32_t) (((uint64_t) sample.exec_ticks * 1000000000ULL) / tick_hz);

        print("%lu,%lu,%u,%u,%lu.%03lu,%u,%lu.%03lu,%d\r\n",
              sample.seq,
              sample.systick_ms,
              sample.load_active,
              sample.latency_ticks,
              latency_us_x1000 / 1000U,
              latency_us_x1000 % 1000U,
              sample.exec_ticks,
              exec_us_x1000 / 1000U,
              exec_us_x1000 % 1000U,
              sample.latency_delta_ticks);

        if (stats.count >= LATENCY_STATS_WINDOW)
        {
            uint32_t latency_avg_x1000 = (stats.latency_sum * 1000U) / stats.count;
            uint32_t exec_avg_x1000 = (stats.exec_sum * 1000U) / stats.count;
            uint32_t overwrite_snapshot = g_overwrite_count;

            print("# stats,window=%lu,load_active=%u,latency_min=%u,latency_max=%u,latency_avg=%lu.%03lu,exec_min=%u,exec_max=%u,exec_avg=%lu.%03lu,rb_overwrite=%lu\r\n",
                  stats.count,
                  load_task_is_active(),
                  stats.latency_min,
                  stats.latency_max,
                  latency_avg_x1000 / 1000U,
                  latency_avg_x1000 % 1000U,
                  stats.exec_min,
                  stats.exec_max,
                  exec_avg_x1000 / 1000U,
                  exec_avg_x1000 % 1000U,
                  overwrite_snapshot);

            latency_stats_init(&stats);
        }
    }
}

void latency_init(TIM_HandleTypeDef *timer_handle, UART_HandleTypeDef *log_uart)
{
    g_tim = timer_handle;
    g_log_uart = log_uart;
    g_head = 0U;
    g_tail = 0U;
    g_overwrite_count = 0U;
    g_seq = 0U;
    g_prev_latency_ticks = 0U;
}

void latency_on_tim_period_elapsed_isr(TIM_HandleTypeDef *htim)
{
    if ((g_tim == NULL) || (htim != g_tim) || (htim->Instance != TIM3))
    {
        return;
    }

    uint16_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
    uint16_t entry_cnt = __HAL_TIM_GET_COUNTER(htim);

    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);

    LatencySample sample;
    sample.seq = ++g_seq;
    sample.systick_ms = HAL_GetTick();
    sample.load_active = load_task_is_active();
    sample.latency_ticks = entry_cnt;
    sample.latency_delta_ticks = (int16_t) ((int32_t) entry_cnt - (int32_t) g_prev_latency_ticks);

    uint16_t exit_cnt = __HAL_TIM_GET_COUNTER(htim);
    if (exit_cnt >= entry_cnt)
    {
        sample.exec_ticks = (uint16_t) (exit_cnt - entry_cnt);
    }
    else
    {
        sample.exec_ticks = (uint16_t) (exit_cnt + (arr + 1U) - entry_cnt);
    }

    g_prev_latency_ticks = entry_cnt;

    latency_push_from_isr(&sample);
}

void latency_start_logging_task(void)
{
    if (g_logging_task_handle == NULL)
    {
        g_logging_task_handle = osThreadNew(latency_logging_task, NULL, &g_logging_task_attr);
    }
}
