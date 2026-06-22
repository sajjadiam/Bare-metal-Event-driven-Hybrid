#include "bevh_dispatcher.h"

static bool bevh_dispatcher_entry_matches(const bevh_handler_entry_t *entry,
                                          bevh_event_id_t event_id,
                                          bevh_event_handler_t handler,
                                          void *user)
{
    return entry->used &&
           (entry->event_id == event_id) &&
           (entry->handler == handler) &&
           (entry->user == user);
}
bevh_status_t bevh_dispatcher_init(bevh_dispatcher_t *d, bevh_handler_entry_t *handlers, bevh_count_t capacity) {
    if((d == NULL) || (handlers == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(d->initialized) {
        return BEVH_ERR_ALREADY_INITIALIZED;
    }

    if(capacity == 0u) {
        return BEVH_ERR_INVALID_ARG;
    }

    d->handlers = handlers;
    d->capacity = capacity;
    d->count = 0u;

    for(bevh_count_t i = 0u; i < capacity; i++) {
        handlers[i].event_id = BEVH_EVENT_NONE;
        handlers[i].handler = NULL;
        handlers[i].user = NULL;
        handlers[i].used = false;
    }
    d->initialized = true;

    return BEVH_OK;
}
bevh_status_t bevh_dispatcher_subscribe(bevh_dispatcher_t *d, bevh_event_id_t event_id, bevh_event_handler_t handler, void *user) {
    if((d == NULL) || (handler == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!d->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    if(event_id == BEVH_EVENT_NONE) {
        return BEVH_ERR_INVALID_ARG;
    }

    for(bevh_count_t i = 0u; i < d->capacity; i++) {
        const bevh_handler_entry_t *entry = &d->handlers[i];

        if(bevh_dispatcher_entry_matches(entry, event_id, handler, user)) {
            return BEVH_ERR_BUSY;
        }
    }

    if(d->count == d->capacity) {
        return BEVH_ERR_FULL;
    }

    for(bevh_count_t i = 0u; i < d->capacity; i++) {
        bevh_handler_entry_t *entry = &d->handlers[i];

        if(!entry->used) {
            entry->event_id = event_id;
            entry->handler = handler;
            entry->user = user;
            entry->used = true;
            d->count++;
            return BEVH_OK;
        }
    }

    return BEVH_ERR_FULL;
}
bevh_status_t bevh_dispatcher_subscribe_table(bevh_dispatcher_t *d, const bevh_handler_entry_t *table, bevh_count_t count) {
    bevh_count_t used_count = 0u;

    if((d == NULL) || (table == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!d->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    if(count == 0u) {
        return BEVH_ERR_INVALID_ARG;
    }

    for(bevh_count_t i = 0u; i < count; i++) {
        const bevh_handler_entry_t *entry = &table[i];

        if(entry->used) {
            if(entry->event_id == BEVH_EVENT_NONE) {
                return BEVH_ERR_INVALID_ARG;
            }

            if(entry->handler == NULL) {
                return BEVH_ERR_INVALID_CONFIG;
            }

            used_count++;

            for(bevh_count_t j = 0u; j < d->capacity; j++) {
                const bevh_handler_entry_t *existing = &d->handlers[j];

                if(bevh_dispatcher_entry_matches(existing, entry->event_id, entry->handler, entry->user)) {
                    return BEVH_ERR_BUSY;
                }
            }

            for(bevh_count_t j = (bevh_count_t)(i + 1u); j < count; j++) {
                const bevh_handler_entry_t *other = &table[j];

                if(bevh_dispatcher_entry_matches(other, entry->event_id, entry->handler, entry->user)) {
                    return BEVH_ERR_BUSY;
                }
            }
        }
    }

    if(used_count > (bevh_count_t)(d->capacity - d->count)) {
        return BEVH_ERR_FULL;
    }

    for(bevh_count_t i = 0u; i < count; i++) {
        const bevh_handler_entry_t *entry = &table[i];

        if(entry->used) {
            bevh_status_t status;

            status = bevh_dispatcher_subscribe(d, entry->event_id, entry->handler, entry->user);
            if(status != BEVH_OK) {
                return status;
            }
        }
    }

    return BEVH_OK;
}
bevh_status_t bevh_dispatcher_unsubscribe(bevh_dispatcher_t *d, bevh_event_id_t event_id, bevh_event_handler_t handler, void *user) {
    if((d == NULL) || (handler == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!d->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    if(event_id == BEVH_EVENT_NONE) {
        return BEVH_ERR_INVALID_ARG;
    }

    for(bevh_count_t i = 0u; i < d->capacity; i++) {
        bevh_handler_entry_t *entry = &d->handlers[i];

        if(bevh_dispatcher_entry_matches(entry, event_id, handler, user)) {
            entry->event_id = BEVH_EVENT_NONE;
            entry->handler = NULL;
            entry->user = NULL;
            entry->used = false;
            d->count--;
            return BEVH_OK;
        }
    }

    return BEVH_ERR_NOT_FOUND;
}
static bool bevh_dispatcher_entry_handles_event(const bevh_handler_entry_t *entry, bevh_event_id_t event_id) {
    return entry->used && (entry->event_id == event_id);
}
bevh_status_t bevh_dispatcher_dispatch(bevh_dispatcher_t *d, const bevh_event_t *event) {
    if((d == NULL) || (event == NULL)) {
        return BEVH_ERR_NULL;
    }

    if(!d->initialized) {
        return BEVH_ERR_NOT_INITIALIZED;
    }

    if(event->id == BEVH_EVENT_NONE) {
        return BEVH_ERR_INVALID_ARG;
    }

    bool handler_found = false;

    for(bevh_count_t i = 0u; i < d->capacity; i++) {
        const bevh_handler_entry_t *entry = &d->handlers[i];

        if(bevh_dispatcher_entry_handles_event(entry, event->id)) {
            entry->handler(event, entry->user);
            handler_found = true;
        }
    }

    if(!handler_found && BEVH_DISPATCH_MISSING_HANDLER_IS_ERROR) {
        return BEVH_ERR_NOT_FOUND;
    }

    return BEVH_OK;
}
bevh_count_t bevh_dispatcher_count(const bevh_dispatcher_t *d) {
    if((d == NULL) || (!d->initialized)) {
        return 0U;
    }

    return d->count;
}
bevh_count_t bevh_dispatcher_capacity(const bevh_dispatcher_t *d) {
    if((d == NULL) || (!d->initialized)) {
        return 0U;
    }

    return d->capacity;
}