#include "bevh_event_queue.h"

static bool bevh_event_queue_critical_is_valid(const bevh_event_queue_t *q) {
#if BEVH_ENABLE_CRITICAL
    return bevh_critical_is_valid(&q->critical);
#else
    BEVH_UNUSED(q);
    return false;
#endif
}
static bevh_status_t bevh_event_queue_validate_critical(const bevh_event_queue_t *q) {
#if BEVH_ENABLE_CRITICAL && !BEVH_ALLOW_NULL_CRITICAL
    if(!bevh_event_queue_critical_is_valid(q)) {
        return BEVH_ERR_INVALID_CONFIG;
    }
#else
    BEVH_UNUSED(q);
#endif

    return BEVH_OK;
}
static bevh_critical_state_t bevh_event_queue_lock(bevh_event_queue_t *q) {
#if BEVH_ENABLE_CRITICAL
    if(bevh_critical_is_valid(&q->critical)) {
        return bevh_critical_enter(&q->critical);
    }
#endif

    BEVH_UNUSED(q);
    return 0u;
}
static void bevh_event_queue_unlock(bevh_event_queue_t *q, bevh_critical_state_t state) {
#if BEVH_ENABLE_CRITICAL
    if(bevh_critical_is_valid(&q->critical)) {
        bevh_critical_exit(&q->critical, state);
        return;
    }
#endif

    BEVH_UNUSED(q);
    BEVH_UNUSED(state);
}
static void bevh_event_queue_reset_indexes(bevh_event_queue_t *q) {
    q->head = 0u;
    q->tail = 0u;
    q->count = 0u;
}
bevh_status_t bevh_event_queue_init(bevh_event_queue_t *q,
                                    bevh_event_t *buffer,
                                    bevh_count_t capacity,
                                    const bevh_critical_t *critical)
{
    if((q == NULL) || (buffer == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(q->initialized) {
        return BEVH_ERR_ALREADY_INITIALIZED;
    }

    if((capacity == 0u) || ((capacity & (capacity - 1u)) != 0u)) {
        return BEVH_ERR_INVALID_ARG;
    }

#if BEVH_ENABLE_CRITICAL
#if BEVH_ALLOW_NULL_CRITICAL
    if(critical != NULL) {
        if((critical->enter == NULL) || (critical->exit == NULL)) {
            return BEVH_ERR_INVALID_CONFIG;
        }

        q->critical = *critical;
    } else {
        q->critical.enter = NULL;
        q->critical.exit = NULL;
        q->critical.user = NULL;
    }
#else
    if(critical == NULL) {
        return BEVH_ERR_NULL;
    }

    if((critical->enter == NULL) || (critical->exit == NULL)) {
        return BEVH_ERR_INVALID_CONFIG;
    }

    q->critical = *critical;
#endif
#else
    BEVH_UNUSED(critical);
    q->critical.enter = NULL;
    q->critical.exit = NULL;
    q->critical.user = NULL;
#endif

    q->buffer = buffer;
    q->capacity = capacity;
    q->mask = (bevh_count_t)(capacity - 1u);

    bevh_event_queue_reset_indexes(q);
    q->overflow_count = 0u;
    q->max_used = 0u;

    q->initialized = true;

    return BEVH_OK;
}
bool bevh_event_queue_is_empty(const bevh_event_queue_t *q) {
    return ((q == NULL) || (!q->initialized) || (q->count == 0U));
}
bool bevh_event_queue_is_full(const bevh_event_queue_t *q) {
    return (q != NULL) && q->initialized && (q->count == q->capacity);
}
void bevh_event_queue_clear(bevh_event_queue_t *q) {
    if((q == NULL) || (!q->initialized)) {
        return;
    }

    bevh_critical_state_t state = bevh_event_queue_lock(q);

    bevh_event_queue_reset_indexes(q);

    bevh_event_queue_unlock(q, state);
}
bevh_count_t bevh_event_queue_count(const bevh_event_queue_t *q) {
    if((q == NULL) || (!q->initialized)) {
        return 0U;
    }

    return q->count;
}
bevh_count_t bevh_event_queue_capacity(const bevh_event_queue_t *q) {
    if((q == NULL) || (!q->initialized)) {
        return 0U;
    }

    return q->capacity;
}
uint32_t bevh_event_queue_overflow_count(const bevh_event_queue_t *q) {
    if((q == NULL) || (!q->initialized)) {
        return 0U;
    }

    return q->overflow_count;
}
bevh_count_t bevh_event_queue_max_used(const bevh_event_queue_t *q) {
    if((q == NULL) || (!q->initialized)) {
        return 0U;
    }

    return q->max_used;
}
static bevh_status_t bevh_event_queue_push_common(bevh_event_queue_t *q, const bevh_event_t *event) {
    bevh_status_t  status = bevh_event_queue_validate_critical(q);
    if(status != BEVH_OK) {
        return status;
    }

    bevh_critical_state_t state = bevh_event_queue_lock(q);

    if(q->count == q->capacity) {
        q->overflow_count++;
        bevh_event_queue_unlock(q, state);
        return BEVH_ERR_FULL;
    }

    q->buffer[q->head] = *event;
    q->head = (bevh_count_t)((q->head + 1u) & q->mask);
    q->count++;

    if(q->count > q->max_used) {
        q->max_used = q->count;
    }

    bevh_event_queue_unlock(q, state);

    return BEVH_OK;
}
bevh_status_t bevh_event_push(bevh_event_queue_t *q, const bevh_event_t *event) {
    if((q == NULL) || (event == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!q->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_event_queue_push_common(q, event);
}
bevh_status_t bevh_event_push_isr(bevh_event_queue_t *q, const bevh_event_t *event) {
    if((q == NULL) || (event == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!q->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    return bevh_event_queue_push_common(q, event);
}
bevh_status_t bevh_event_pop(bevh_event_queue_t *q, bevh_event_t *event) {
    if((q == NULL) || (event == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!q->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    bevh_status_t  status = bevh_event_queue_validate_critical(q);
    if(status != BEVH_OK) {
        return status;
    }

    bevh_critical_state_t state = bevh_event_queue_lock(q);

    if(q->count == 0U) {
        bevh_event_queue_unlock(q, state);
        return BEVH_ERR_EMPTY;
    }

    *event = q->buffer[q->tail];
    q->tail = (bevh_count_t)((q->tail + 1u) & q->mask);
    q->count--;

    bevh_event_queue_unlock(q, state);

    return BEVH_OK;
}
