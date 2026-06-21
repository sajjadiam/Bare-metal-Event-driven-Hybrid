#include "bevh_event_queue.h"
#include "bevh_types.h"
#include <stdbool.h>


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

    q->buffer = buffer;
    q->capacity = capacity;
    q->mask = (bevh_count_t)(capacity - 1u);

    bevh_event_queue_clear(q);
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
    if(q == NULL) {
        return;
    }

    q->head = 0u;
    q->tail = 0u;
    q->count = 0u;
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
bevh_status_t bevh_event_push(bevh_event_queue_t *q, const bevh_event_t *event) {
    bevh_critical_state_t state;

    if((q == NULL) || (event == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!q->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

#if BEVH_ENABLE_CRITICAL
    if(bevh_critical_is_valid(&q->critical)) {
        state = bevh_critical_enter(&q->critical);
    } 
    else {
        state = 0u;
    }
#else
    state = 0u;
#endif

    if(q->count == q->capacity) {
        q->overflow_count++;

#if BEVH_ENABLE_CRITICAL
        if(bevh_critical_is_valid(&q->critical)) {
            bevh_critical_exit(&q->critical, state);
        }
#endif

        return BEVH_ERR_FULL;
    }

    q->buffer[q->head] = *event;
    q->head = (bevh_count_t)((q->head + 1u) & q->mask);
    q->count++;

    if(q->count > q->max_used) {
        q->max_used = q->count;
    }

#if BEVH_ENABLE_CRITICAL
    if(bevh_critical_is_valid(&q->critical)) {
        bevh_critical_exit(&q->critical, state);
    }
#else
    BEVH_UNUSED(state);
#endif

    return BEVH_OK;
}
bevh_status_t bevh_event_push_isr(bevh_event_queue_t *q, const bevh_event_t *event) {

    return BEVH_OK;
}
bevh_status_t bevh_event_pop(bevh_event_queue_t *q, bevh_event_t *event) {
    return BEVH_OK;
}
