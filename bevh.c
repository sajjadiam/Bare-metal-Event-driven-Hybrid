#include "bevh.h"
static void bevh_context_clear(bevh_t *ctx) {
    ctx->queue.buffer = NULL;
    ctx->queue.capacity = 0u;
    ctx->queue.mask = 0u;
    ctx->queue.head = 0u;
    ctx->queue.tail = 0u;
    ctx->queue.count = 0u;
    ctx->queue.overflow_count = 0u;
    ctx->queue.max_used = 0u;
    ctx->queue.initialized = false;
    ctx->queue.critical.enter = NULL;
    ctx->queue.critical.exit = NULL;
    ctx->queue.critical.user = NULL;

    ctx->dispatcher.handlers = NULL;
    ctx->dispatcher.capacity = 0u;
    ctx->dispatcher.count = 0u;
    ctx->dispatcher.initialized = false;

    ctx->timer_mgr.timers = NULL;
    ctx->timer_mgr.capacity = 0u;
    ctx->timer_mgr.queue = NULL;
    ctx->timer_mgr.initialized = false;

    ctx->initialized = false;
}
static bevh_status_t bevh_process_one(bevh_t *ctx) {
    bevh_event_t event;
    bevh_status_t status;

    status = bevh_event_pop(&ctx->queue, &event);
    if(status != BEVH_OK) {
        return status;
    }

    return bevh_dispatcher_dispatch(&ctx->dispatcher, &event);
}
bevh_status_t bevh_init(bevh_t *ctx, const bevh_config_runtime_t *cfg) {
    if((ctx == NULL) || (cfg == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(ctx->initialized) {
        return BEVH_ERR_ALREADY_INITIALIZED;
    }

    bevh_status_t status = bevh_event_queue_init(&ctx->queue, cfg->event_buffer, cfg->event_capacity, &cfg->critical);
    if(status != BEVH_OK) {
        bevh_context_clear(ctx);
        return status;
    }

    status = bevh_dispatcher_init(&ctx->dispatcher, cfg->handler_buffer, cfg->handler_capacity);
    if(status != BEVH_OK) {
        bevh_context_clear(ctx);
        return status;
    }

    status = bevh_timer_mgr_init(&ctx->timer_mgr, cfg->timer_buffer, cfg->timer_capacity, &ctx->queue);
    if(status != BEVH_OK) {
        bevh_context_clear(ctx);
        return status;
    }

    ctx->initialized = true;

    return BEVH_OK;
}
bevh_status_t bevh_post(bevh_t *ctx, const bevh_event_t *event) {
    if((ctx == NULL) || (event == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_event_push(&ctx->queue, event);
}
bevh_status_t bevh_post_isr(bevh_t *ctx, const bevh_event_t *event) {
    if((ctx == NULL) || (event == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_event_push_isr(&ctx->queue, event);
}
bevh_status_t bevh_subscribe(bevh_t *ctx, bevh_event_id_t event_id, bevh_event_handler_t handler, void *user) {
    if((ctx == NULL) || (handler == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_dispatcher_subscribe(&ctx->dispatcher, event_id, handler, user);
}
bevh_status_t bevh_subscribe_table(bevh_t *ctx, const bevh_handler_entry_t *table, bevh_count_t count) {
    if((ctx == NULL) || (table == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_dispatcher_subscribe_table(&ctx->dispatcher, table, count);
}
bevh_status_t bevh_unsubscribe(bevh_t *ctx, bevh_event_id_t event_id, bevh_event_handler_t handler, void *user) {
    if((ctx == NULL) || (handler == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_dispatcher_unsubscribe(&ctx->dispatcher, event_id, handler, user);
}
bevh_status_t bevh_start_timer(bevh_t *ctx, const bevh_timer_config_t *cfg) {
    if((ctx == NULL) || (cfg == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_timer_start(&ctx->timer_mgr, cfg);
}
bevh_status_t bevh_stop_timer(bevh_t *ctx, bevh_timer_id_t timer_id) {
    if(ctx == NULL) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_timer_stop(&ctx->timer_mgr, timer_id);
}
bevh_status_t bevh_restart_timer(bevh_t *ctx, bevh_timer_id_t timer_id) {
    if(ctx == NULL) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_timer_restart(&ctx->timer_mgr, timer_id);
}
void bevh_tick(bevh_t *ctx, bevh_tick_t elapsed_ticks) {
    if((ctx == NULL) || (!ctx->initialized)) {
        return;
    }

    bevh_timer_tick(&ctx->timer_mgr, elapsed_ticks);
}
bevh_status_t bevh_run_once(bevh_t *ctx) {
    if(ctx == NULL) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_process_one(ctx);
}
bevh_status_t bevh_run(bevh_t *ctx, bevh_count_t *processed_count) {
    bevh_count_t processed = 0u;
    bevh_status_t status;

    if(processed_count != NULL) {
        *processed_count = 0u;
    }

    if(ctx == NULL) {
        return BEVH_ERR_NULL;
    }

    if(!ctx->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

#if BEVH_RUN_MAX_EVENTS_PER_CALL == 0u
    status = bevh_process_one(ctx);
    while(status == BEVH_OK) {
        processed++;
        status = bevh_process_one(ctx);
    }

    if(status == BEVH_ERR_EMPTY) {
        status = BEVH_OK;
    }
#else
    status = BEVH_OK;
    while((processed < BEVH_RUN_MAX_EVENTS_PER_CALL) && (status == BEVH_OK)) {
        status = bevh_process_one(ctx);
        if(status == BEVH_OK) {
            processed++;
        } else if(status == BEVH_ERR_EMPTY) {
            status = BEVH_OK;
        }
    }
#endif

    if(processed_count != NULL) {
        *processed_count = processed;
    }

    return status;
}