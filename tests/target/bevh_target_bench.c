#include "bevh_target_bench.h"

#define BEVH_TARGET_BENCH_EVENT_ID      ((bevh_event_id_t)BEVH_EVENT_USER_BASE)
#define BEVH_TARGET_BENCH_TIMER_EVENT   ((bevh_event_id_t)(BEVH_EVENT_USER_BASE + 1u))
#define BEVH_TARGET_BENCH_MIN_EVENTS    ((bevh_count_t)32u)
#define BEVH_TARGET_BENCH_MIN_HANDLERS  ((bevh_count_t)4u)
#define BEVH_TARGET_BENCH_MIN_TIMERS    ((bevh_count_t)4u)

typedef struct {
    volatile uint32_t calls;
} bevh_target_bench_handler_state_t;

static void bevh_target_bench_handler(const bevh_event_t *event, void *user) {
    bevh_target_bench_handler_state_t *state = (bevh_target_bench_handler_state_t *)user;

    if((event != NULL) && (state != NULL)) {
        state->calls += event->param;
    }
}

static uint32_t bevh_target_bench_elapsed(uint32_t start, uint32_t end) {
    return (uint32_t)(end - start);
}

static uint32_t bevh_target_bench_average(uint64_t total, uint32_t iterations) {
    uint32_t average = 0u;

    if(iterations != 0u) {
        average = (uint32_t)(total / (uint64_t)iterations);
    }

    return average;
}

static void bevh_target_bench_fill_sizes(const bevh_target_bench_config_t *cfg,
                                         bevh_target_bench_result_t *result) {
    result->sizeof_event = (uint32_t)sizeof(bevh_event_t);
    result->sizeof_queue = (uint32_t)sizeof(bevh_event_queue_t);
    result->sizeof_handler_entry = (uint32_t)sizeof(bevh_handler_entry_t);
    result->sizeof_dispatcher = (uint32_t)sizeof(bevh_dispatcher_t);
    result->sizeof_timer = (uint32_t)sizeof(bevh_timer_t);
    result->sizeof_timer_mgr = (uint32_t)sizeof(bevh_timer_mgr_t);
    result->sizeof_context = (uint32_t)sizeof(bevh_t);
    result->configured_ram_bytes =
        (uint32_t)sizeof(bevh_t) +
        ((uint32_t)sizeof(bevh_event_t) * (uint32_t)cfg->event_capacity) +
        ((uint32_t)sizeof(bevh_handler_entry_t) * (uint32_t)cfg->handler_capacity) +
        ((uint32_t)sizeof(bevh_timer_t) * (uint32_t)cfg->timer_capacity);
}

static bevh_event_t bevh_target_bench_make_event(void) {
    bevh_event_t event;

    event.id = BEVH_TARGET_BENCH_EVENT_ID;
    event.source = 1u;
    event.timestamp = 0u;
    event.param = 1u;
    event.data = NULL;

    return event;
}

static bevh_status_t bevh_target_bench_validate(const bevh_target_bench_config_t *cfg,
                                                bevh_target_bench_result_t *result) {
    if((cfg == NULL) || (result == NULL) || (cfg->event_buffer == NULL) ||
       (cfg->handler_buffer == NULL) || (cfg->timer_buffer == NULL) ||
       (cfg->cycle_now == NULL)) {
        return BEVH_ERR_NULL;
    }

    if((cfg->iterations == 0u) ||
       (cfg->event_capacity < BEVH_TARGET_BENCH_MIN_EVENTS) ||
       (cfg->handler_capacity < BEVH_TARGET_BENCH_MIN_HANDLERS) ||
       (cfg->timer_capacity < BEVH_TARGET_BENCH_MIN_TIMERS)) {
        return BEVH_ERR_INVALID_ARG;
    }

    return BEVH_OK;
}

bevh_status_t bevh_target_bench_run(const bevh_target_bench_config_t *cfg,
                                    bevh_target_bench_result_t *result) {
    bevh_t ctx = {0};
    bevh_config_runtime_t runtime_cfg;
    bevh_event_t event;
    bevh_event_t popped;
    bevh_timer_config_t timer_cfg;
    bevh_target_bench_handler_state_t handler_state = {0u};
    uint32_t start;
    uint32_t end;
    uint64_t total;
    bevh_status_t status;

    status = bevh_target_bench_validate(cfg, result);
    if(status != BEVH_OK) {
        return status;
    }

    result->status = BEVH_OK;
    result->queue_push_pop_cycles = 0u;
    result->core_post_run_once_cycles = 0u;
    result->timer_tick_no_expiry_cycles = 0u;
    result->timer_tick_expiry_cycles = 0u;
    bevh_target_bench_fill_sizes(cfg, result);

    runtime_cfg.event_buffer = cfg->event_buffer;
    runtime_cfg.event_capacity = cfg->event_capacity;
    runtime_cfg.handler_buffer = cfg->handler_buffer;
    runtime_cfg.handler_capacity = cfg->handler_capacity;
    runtime_cfg.timer_buffer = cfg->timer_buffer;
    runtime_cfg.timer_capacity = cfg->timer_capacity;
    runtime_cfg.critical = cfg->critical;

    status = bevh_init(&ctx, &runtime_cfg);
    if(status != BEVH_OK) {
        result->status = status;
        return status;
    }

    status = bevh_subscribe(&ctx, BEVH_TARGET_BENCH_EVENT_ID, bevh_target_bench_handler, &handler_state);
    if(status != BEVH_OK) {
        result->status = status;
        return status;
    }

    event = bevh_target_bench_make_event();

    total = 0u;
    for(uint32_t i = 0u; i < cfg->iterations; i++) {
        start = cfg->cycle_now(cfg->cycle_user);
        status = bevh_event_push(&ctx.queue, &event);
        if(status == BEVH_OK) {
            status = bevh_event_pop(&ctx.queue, &popped);
        }
        end = cfg->cycle_now(cfg->cycle_user);

        if(status != BEVH_OK) {
            result->status = status;
            return status;
        }

        total += (uint64_t)bevh_target_bench_elapsed(start, end);
    }
    result->queue_push_pop_cycles = bevh_target_bench_average(total, cfg->iterations);

    total = 0u;
    for(uint32_t i = 0u; i < cfg->iterations; i++) {
        start = cfg->cycle_now(cfg->cycle_user);
        status = bevh_post(&ctx, &event);
        if(status == BEVH_OK) {
            status = bevh_run_once(&ctx);
        }
        end = cfg->cycle_now(cfg->cycle_user);

        if(status != BEVH_OK) {
            result->status = status;
            return status;
        }

        total += (uint64_t)bevh_target_bench_elapsed(start, end);
    }
    result->core_post_run_once_cycles = bevh_target_bench_average(total, cfg->iterations);

    timer_cfg.timer_id = 1u;
    timer_cfg.delay_ticks = 0x7fffffffu;
    timer_cfg.periodic = true;
    timer_cfg.event_id = BEVH_TARGET_BENCH_TIMER_EVENT;
    timer_cfg.source = 2u;
    timer_cfg.param = 1u;
    timer_cfg.data = NULL;

    for(bevh_count_t i = 0u; i < BEVH_TARGET_BENCH_MIN_TIMERS; i++) {
        timer_cfg.timer_id = (bevh_timer_id_t)(i + 1u);
        status = bevh_start_timer(&ctx, &timer_cfg);
        if(status != BEVH_OK) {
            result->status = status;
            return status;
        }
    }

    total = 0u;
    for(uint32_t i = 0u; i < cfg->iterations; i++) {
        start = cfg->cycle_now(cfg->cycle_user);
        bevh_tick(&ctx, 1u);
        end = cfg->cycle_now(cfg->cycle_user);
        total += (uint64_t)bevh_target_bench_elapsed(start, end);
    }
    result->timer_tick_no_expiry_cycles = bevh_target_bench_average(total, cfg->iterations);

    timer_cfg.delay_ticks = 1u;
    for(bevh_count_t i = 0u; i < BEVH_TARGET_BENCH_MIN_TIMERS; i++) {
        timer_cfg.timer_id = (bevh_timer_id_t)(i + 1u);
        status = bevh_start_timer(&ctx, &timer_cfg);
        if(status != BEVH_OK) {
            result->status = status;
            return status;
        }
    }

    total = 0u;
    for(uint32_t i = 0u; i < cfg->iterations; i++) {
        start = cfg->cycle_now(cfg->cycle_user);
        bevh_tick(&ctx, 1u);
        end = cfg->cycle_now(cfg->cycle_user);
        bevh_event_queue_clear(&ctx.queue);
        total += (uint64_t)bevh_target_bench_elapsed(start, end);
    }
    result->timer_tick_expiry_cycles = bevh_target_bench_average(total, cfg->iterations);

    return BEVH_OK;
}
