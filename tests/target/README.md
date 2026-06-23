# BEVH Target-Side Benchmark

This folder contains a small benchmark harness for running BEVH measurements on
a real MCU target. It does not depend on HAL, LL, CMSIS, heap, printf, UART, or
an RTOS. The application provides buffers, critical-section callbacks, and a
cycle-counter callback.

## What It Measures

- Average cycles for one event queue push+pop pair
- Average cycles for one `bevh_post()` + `bevh_run_once()` pair
- Average cycles for `bevh_tick()` scanning active timers without expiration
- Average cycles for `bevh_tick()` when timers expire and post events
- ABI-specific `sizeof(...)` values and configured RAM usage

## Cortex-M DWT Example

Use this in board/application test code, not inside the BEVH core.

```c
#include "tests/target/bevh_target_bench.h"

static bevh_event_t g_events[32];
static bevh_handler_entry_t g_handlers[4];
static bevh_timer_t g_timers[4];
static bevh_target_bench_result_t g_result;

static void dwt_cycle_counter_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t cycle_now(void *user)
{
    (void)user;
    return DWT->CYCCNT;
}

void app_run_bevh_benchmark(void)
{
    bevh_target_bench_config_t cfg;

    dwt_cycle_counter_init();

    cfg.event_buffer = g_events;
    cfg.event_capacity = 32u;
    cfg.handler_buffer = g_handlers;
    cfg.handler_capacity = 4u;
    cfg.timer_buffer = g_timers;
    cfg.timer_capacity = 4u;
    cfg.critical = app_bevh_critical;
    cfg.cycle_now = cycle_now;
    cfg.cycle_user = NULL;
    cfg.iterations = 10000u;

    (void)bevh_target_bench_run(&cfg, &g_result);
}
```

After `app_run_bevh_benchmark()` returns, inspect `g_result` with a debugger or
print it through the project's own UART/log system.

## Important Rules

- Run this benchmark in a test firmware, not in production control firmware.
- Do not run it while gate timing, protection timing, or ADC trigger timing is
  active.
- Use compiler optimization close to production, for example `-O2` or `-Os`.
- Repeat the run with interrupts enabled and disabled if you need worst-case
  jitter information.
- Treat PC host timing as a relative smoke test only. Target-side cycle counts
  are the numbers that matter for an induction furnace controller.
