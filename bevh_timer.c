#include "bevh_timer.h"

#if BEVH_TIMER_MAX_CATCHUP_EVENTS == 0u
#error "BEVH_TIMER_MAX_CATCHUP_EVENTS must be greater than zero"
#endif

static void bevh_timer_clear(bevh_timer_t *timer) {
    timer->id = BEVH_TIMER_ID_NONE;
    timer->remaining_ticks = 0u;
    timer->period_ticks = 0u;
    timer->event_id = BEVH_EVENT_NONE;
    timer->source = (bevh_source_id_t)0u;
    timer->param = 0u;
    timer->data = NULL;
    timer->missed_count = 0u;
    timer->periodic = false;
    timer->active = false;
}
static bevh_timer_t *bevh_timer_find(bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id) {
    for(bevh_count_t i = 0u; i < mgr->capacity; i++) {
        if(mgr->timers[i].id == timer_id) {
            return &mgr->timers[i];
        }
    }

    return NULL;
}
static bevh_timer_t *bevh_timer_find_free(bevh_timer_mgr_t *mgr) {
    for(bevh_count_t i = 0u; i < mgr->capacity; i++) {
        if(mgr->timers[i].id == BEVH_TIMER_ID_NONE) {
            return &mgr->timers[i];
        }
    }

    return NULL;
}
static const bevh_timer_t *bevh_timer_find_const(const bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id) {
    for(bevh_count_t i = 0u; i < mgr->capacity; i++) {
        if(mgr->timers[i].id == timer_id) {
            return &mgr->timers[i];
        }
    }

    return NULL;
}
static bevh_status_t bevh_timer_post_event(bevh_timer_mgr_t *mgr, const bevh_timer_t *timer) {
    bevh_event_t event;

    event.id = timer->event_id;
    event.source = timer->source;
    event.timestamp = 0u;
    event.param = timer->param;
    event.data = timer->data;

    return bevh_event_push(mgr->queue, &event);
}
static void bevh_timer_process_one_shot(bevh_timer_mgr_t *mgr, bevh_timer_t *timer) {
    if(bevh_timer_post_event(mgr, timer) != BEVH_OK) {
        timer->missed_count++;
    }

    timer->active = false;
    timer->remaining_ticks = 0u;
}
static void bevh_timer_process_periodic_expired(bevh_timer_mgr_t *mgr, bevh_timer_t *timer, bevh_tick_t elapsed_ticks) {
    bevh_tick_t overdue_ticks;
    bevh_tick_t consumed_after_expiry;
    uint32_t expired_count;
    uint32_t post_count;

    if(timer->period_ticks == 0u) {
        timer->active = false;
        timer->remaining_ticks = 0u;
    }
    else {
        overdue_ticks = elapsed_ticks - timer->remaining_ticks;
        expired_count = 1u + (uint32_t)(overdue_ticks / timer->period_ticks);
        consumed_after_expiry = overdue_ticks % timer->period_ticks;

        post_count = expired_count;

#if BEVH_TIMER_ENABLE_CATCHUP
        if(post_count > BEVH_TIMER_MAX_CATCHUP_EVENTS) {
            post_count = BEVH_TIMER_MAX_CATCHUP_EVENTS;
        }
#else
        post_count = 1u;
#endif

        timer->missed_count += (expired_count - post_count);

        for(uint32_t n = 0u; n < post_count; n++) {
            if(bevh_timer_post_event(mgr, timer) != BEVH_OK) {
                timer->missed_count += (post_count - n);
                n = post_count;
            }
        }

        if(consumed_after_expiry == 0u) {
            timer->remaining_ticks = timer->period_ticks;
        }
        else {
            timer->remaining_ticks = timer->period_ticks - consumed_after_expiry;
        }
    }
}
static void bevh_timer_process_one(bevh_timer_mgr_t *mgr, bevh_timer_t *timer, bevh_tick_t elapsed_ticks) {
    if(timer->active) {
        if(timer->event_id == BEVH_EVENT_NONE) {
            timer->active = false;
            timer->remaining_ticks = 0u;
        }
        else if(timer->remaining_ticks > elapsed_ticks) {
            timer->remaining_ticks -= elapsed_ticks;
        }
        else if(!timer->periodic) {
            bevh_timer_process_one_shot(mgr, timer);
        }
        else {
            bevh_timer_process_periodic_expired(mgr, timer, elapsed_ticks);
        }
    }
}
bevh_status_t bevh_timer_mgr_init(bevh_timer_mgr_t *mgr, bevh_timer_t *timers, bevh_count_t capacity, bevh_event_queue_t *queue) {
    if ((mgr == NULL) || (timers == NULL) || (queue == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(mgr->initialized) {
        return BEVH_ERR_ALREADY_INITIALIZED;
    }

    if (capacity == 0) {
        return BEVH_ERR_INVALID_ARG;
    }

    if(!queue->initialized) {
        return BEVH_ERR_INVALID_CONFIG;
    }

    mgr->timers = timers;
    mgr->capacity = capacity;
    mgr->queue = queue;

    for (bevh_count_t i = 0; i < capacity; i++) {
        bevh_timer_clear(&timers[i]);
    }

    mgr->initialized = true;

    return BEVH_OK;
}
bevh_status_t bevh_timer_start(bevh_timer_mgr_t *mgr, const bevh_timer_config_t *cfg) {
    if((mgr == NULL) || (cfg == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(mgr->initialized == false) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    if((cfg->timer_id == BEVH_TIMER_ID_NONE) || (cfg->event_id == BEVH_EVENT_NONE)) {
        return BEVH_ERR_INVALID_ARG;
    }

    if(cfg->delay_ticks == 0u) {
#if BEVH_TIMER_ALLOW_ZERO_DELAY
        if(cfg->periodic) {
            return BEVH_ERR_INVALID_ARG;
        }
#else
        return BEVH_ERR_INVALID_ARG;
#endif
    }

    bevh_timer_t* timer = bevh_timer_find(mgr, cfg->timer_id);
    if(timer == NULL) {
        timer = bevh_timer_find_free(mgr);
        if(timer == NULL) {
            return BEVH_ERR_FULL;
        }
    }

    timer->id = cfg->timer_id;
    timer->remaining_ticks = cfg->delay_ticks;
    timer->period_ticks = cfg->delay_ticks;
    timer->event_id = cfg->event_id;
    timer->source = cfg->source;
    timer->param = cfg->param;
    timer->data = cfg->data;
    timer->missed_count = 0u;
    timer->periodic = cfg->periodic;
    timer->active = true;

    return BEVH_OK;
}
bevh_status_t bevh_timer_stop(bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id) {
    if(mgr == NULL) {
        return BEVH_ERR_NULL;
    }

    if(!mgr->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    if(timer_id == BEVH_TIMER_ID_NONE) {
        return BEVH_ERR_INVALID_ARG;
    }

    bevh_timer_t* timer = bevh_timer_find(mgr,timer_id);
    if(timer == NULL) {
        return BEVH_ERR_NOT_FOUND;
    }

    timer->active = false;
    return BEVH_OK;
}
bevh_status_t bevh_timer_restart(bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id) {
    if(mgr == NULL) {
        return BEVH_ERR_NULL;
    }

    if(!mgr->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    if(timer_id == BEVH_TIMER_ID_NONE) {
        return BEVH_ERR_INVALID_ARG;
    }

    bevh_timer_t* timer = bevh_timer_find(mgr,timer_id);
    if(timer == NULL) {
        return BEVH_ERR_NOT_FOUND;
    }

    if((timer->period_ticks == (bevh_tick_t)0u) || (timer->event_id == BEVH_EVENT_NONE)) {
        return BEVH_ERR_INVALID_CONFIG ;
    }

    timer->remaining_ticks = timer->period_ticks;
    timer->missed_count = 0u;
    timer->active = true;

    return BEVH_OK;
}
bool bevh_timer_is_active(const bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id) {
    if((mgr == NULL) || (!mgr->initialized) || (timer_id == BEVH_TIMER_ID_NONE)) {
        return false;
    }

    const bevh_timer_t* timer = bevh_timer_find_const(mgr,timer_id);
    if((timer == NULL) || (timer->event_id == BEVH_EVENT_NONE)) {
        return false;
    }

    return timer->active;
}
bevh_tick_t bevh_timer_remaining(const bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id) {
    if((mgr == NULL) || (!mgr->initialized) || (timer_id == BEVH_TIMER_ID_NONE)) {
        return (bevh_tick_t)0u;
    }

    const bevh_timer_t* timer = bevh_timer_find_const(mgr,timer_id);
    if((timer == NULL) || (timer->event_id == BEVH_EVENT_NONE) || (!timer->active)) {
        return (bevh_tick_t)0u;
    }

    return timer->remaining_ticks;
}
bevh_count_t bevh_timer_capacity(const bevh_timer_mgr_t *mgr) {
    if((mgr == NULL) || (!mgr->initialized)) {
        return (bevh_count_t)0u;
    }

    return mgr->capacity;
}
void bevh_timer_tick(bevh_timer_mgr_t *mgr, bevh_tick_t elapsed_ticks) {
    if((mgr != NULL) && mgr->initialized && (elapsed_ticks != 0u)) {
        for(bevh_count_t i = 0u; i < mgr->capacity; i++) {
            bevh_timer_process_one(mgr, &mgr->timers[i], elapsed_ticks);
        }
    }
}
