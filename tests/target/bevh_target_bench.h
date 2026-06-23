/**
 * @file bevh_target_bench.h
 * @brief Target-side benchmark harness for BEVH.
 *
 * This file is intended to be compiled into a real MCU test firmware. It does
 * not access HAL, LL, CMSIS, registers, GPIO, UART, heap, or an RTOS. The
 * application supplies all buffers and a cycle-counter callback.
 *
 * On Cortex-M targets, the cycle callback is normally backed by DWT->CYCCNT.
 * On smaller MCUs without DWT, use a free-running hardware timer and interpret
 * the result as timer ticks instead of CPU cycles.
 */

#ifndef BEVH_TARGET_BENCH_H
#define BEVH_TARGET_BENCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bevh.h"

/**
 * @typedef bevh_target_cycle_now_fn
 * @brief Return the current target-side cycle or timer counter value.
 *
 * The counter must be monotonic modulo uint32_t. Wraparound is handled by
 * unsigned subtraction as long as each measured section is shorter than one
 * full counter period.
 *
 * @param user Application pointer supplied in @ref bevh_target_bench_config_t.
 *
 * @return Current cycle or timer counter value.
 */
typedef uint32_t (*bevh_target_cycle_now_fn)(void *user);

/**
 * @struct bevh_target_bench_config_t
 * @brief Application-supplied configuration for target-side benchmarking.
 */
typedef struct {
    /**
     * @brief Event buffer used by the benchmark context.
     *
     * Capacity must be a power of two and should be at least 32 for the default
     * benchmark workload.
     */
    bevh_event_t *event_buffer;

    /**
     * @brief Number of entries in @ref event_buffer.
     */
    bevh_count_t event_capacity;

    /**
     * @brief Handler-entry buffer used by the benchmark context.
     *
     * Capacity should be at least 4.
     */
    bevh_handler_entry_t *handler_buffer;

    /**
     * @brief Number of entries in @ref handler_buffer.
     */
    bevh_count_t handler_capacity;

    /**
     * @brief Timer buffer used by the benchmark context.
     *
     * Capacity should be at least 4.
     */
    bevh_timer_t *timer_buffer;

    /**
     * @brief Number of entries in @ref timer_buffer.
     */
    bevh_count_t timer_capacity;

    /**
     * @brief Critical-section callbacks used by the event queue.
     */
    bevh_critical_t critical;

    /**
     * @brief Target cycle or timer counter callback.
     */
    bevh_target_cycle_now_fn cycle_now;

    /**
     * @brief User pointer passed to @ref cycle_now.
     */
    void *cycle_user;

    /**
     * @brief Number of iterations per benchmark.
     *
     * Use a value large enough to reduce counter-read overhead. A practical
     * starting point on Cortex-M is 1000 to 10000.
     */
    uint32_t iterations;
} bevh_target_bench_config_t;

/**
 * @struct bevh_target_bench_result_t
 * @brief Target-side benchmark and RAM result.
 */
typedef struct {
    /**
     * @brief Final status of the benchmark run.
     */
    bevh_status_t status;

    /**
     * @brief sizeof(bevh_event_t) on this target ABI.
     */
    uint32_t sizeof_event;

    /**
     * @brief sizeof(bevh_event_queue_t) on this target ABI.
     */
    uint32_t sizeof_queue;

    /**
     * @brief sizeof(bevh_handler_entry_t) on this target ABI.
     */
    uint32_t sizeof_handler_entry;

    /**
     * @brief sizeof(bevh_dispatcher_t) on this target ABI.
     */
    uint32_t sizeof_dispatcher;

    /**
     * @brief sizeof(bevh_timer_t) on this target ABI.
     */
    uint32_t sizeof_timer;

    /**
     * @brief sizeof(bevh_timer_mgr_t) on this target ABI.
     */
    uint32_t sizeof_timer_mgr;

    /**
     * @brief sizeof(bevh_t) on this target ABI.
     */
    uint32_t sizeof_context;

    /**
     * @brief RAM used by context plus supplied buffers.
     */
    uint32_t configured_ram_bytes;

    /**
     * @brief Average cycles for one push+pop pair.
     */
    uint32_t queue_push_pop_cycles;

    /**
     * @brief Average cycles for one post+run_once pair through the top API.
     */
    uint32_t core_post_run_once_cycles;

    /**
     * @brief Average cycles for one timer tick scanning active timers without
     * expiration.
     */
    uint32_t timer_tick_no_expiry_cycles;

    /**
     * @brief Average cycles for one timer tick with timer expirations.
     */
    uint32_t timer_tick_expiry_cycles;
} bevh_target_bench_result_t;

/**
 * @brief Run the BEVH target-side benchmark.
 *
 * @param cfg Application-supplied buffers and cycle-counter callback.
 * @param result Destination for benchmark results.
 *
 * @retval BEVH_OK Benchmark completed.
 * @retval BEVH_ERR_NULL @p cfg, @p result, a required buffer, or
 *         @c cfg->cycle_now was NULL.
 * @retval BEVH_ERR_INVALID_ARG A capacity or iteration count was invalid.
 * @retval BEVH_ERR_INVALID_CONFIG A BEVH submodule rejected the configuration.
 */
bevh_status_t bevh_target_bench_run(const bevh_target_bench_config_t *cfg,
                                    bevh_target_bench_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* BEVH_TARGET_BENCH_H */
