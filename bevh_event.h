/**
 * @file bevh_event.h
 * @brief Event identifiers and event object definition for BEVH.
 *
 * This header defines the generic event contract used by the BEVH event queue,
 * dispatcher, timer manager, and top-level framework context.
 *
 * The framework owns only the reserved event ID range below
 * @c BEVH_EVENT_USER_BASE. Application-specific event IDs must start from
 * @c BEVH_EVENT_USER_BASE and must be defined outside this framework.
 */

#ifndef BEVH_EVENT_H
#define BEVH_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bevh_types.h"

/**
 * @def BEVH_EVENT_NONE
 * @brief Invalid or empty event ID.
 *
 * This value may be used to initialize storage or mark an event slot as empty.
 * It should not be used for normal application events.
 */
#define BEVH_EVENT_NONE       ((bevh_event_id_t)0u)

/**
 * @def BEVH_EVENT_TICK
 * @brief Reserved event ID for framework or application tick notifications.
 *
 * BEVH does not require tick events for normal timer operation. This ID is
 * reserved for applications that explicitly choose to publish tick events.
 */
#define BEVH_EVENT_TICK       ((bevh_event_id_t)1u)

/**
 * @def BEVH_EVENT_TIMER
 * @brief Reserved event ID for generic software timer expiration events.
 *
 * Timer users may also choose application-specific event IDs when starting a
 * timer. This ID exists as a generic default.
 */
#define BEVH_EVENT_TIMER      ((bevh_event_id_t)2u)

/**
 * @def BEVH_EVENT_FAULT
 * @brief Reserved event ID for generic fault notification events.
 *
 * BEVH does not define project fault meanings. Fault IDs, severity, and safety
 * behavior belong to the application or a dedicated fault module.
 */
#define BEVH_EVENT_FAULT      ((bevh_event_id_t)3u)

/**
 * @def BEVH_EVENT_USER_BASE
 * @brief First event ID available for application-specific events.
 *
 * Applications must define their event IDs at or above this value.
 */
#define BEVH_EVENT_USER_BASE  ((bevh_event_id_t)1000u)

/**
 * @struct bevh_event_t
 * @brief Generic event object passed through BEVH.
 *
 * Events are copied into the event queue by value. The @c data pointer is copied
 * as a pointer only; BEVH never allocates, frees, copies, or owns the pointed-to
 * object.
 */
typedef struct {
    /**
     * @brief Event type identifier.
     *
     * Framework-reserved IDs are below @c BEVH_EVENT_USER_BASE. Application
     * event IDs must start from @c BEVH_EVENT_USER_BASE.
     */
    bevh_event_id_t id;

    /**
     * @brief Event producer identifier.
     *
     * Source IDs are application-defined. BEVH stores and forwards this value
     * without interpreting it.
     */
    bevh_source_id_t source;

    /**
     * @brief Event timestamp in application-defined ticks.
     *
     * The timestamp unit is owned by the application. BEVH does not assume
     * milliseconds, microseconds, CPU cycles, or hardware timer ticks.
     */
    bevh_tick_t timestamp;

    /**
     * @brief Lightweight event parameter.
     *
     * This field is intended for small values such as indexes, flags, captured
     * counts, command IDs, or application-defined codes.
     */
    bevh_param_t param;

    /**
     * @brief Optional pointer to application-owned data.
     *
     * BEVH does not take ownership of this pointer. The producer and consumer
     * must agree on lifetime, mutability, and type.
     */
    void *data;
} bevh_event_t;

/**
 * @brief Initialize a BEVH event object.
 *
 * This helper only fills the event fields. It does not validate whether the
 * event ID is registered or whether the source ID is meaningful to the
 * application.
 *
 * @param event Pointer to the event object to initialize.
 * @param id Event type identifier.
 * @param source Event producer identifier.
 * @param timestamp Application-defined timestamp.
 * @param param Lightweight event parameter.
 * @param data Optional application-owned data pointer.
 *
 * @retval BEVH_OK Event object was initialized.
 * @retval BEVH_ERR_NULL @p event was NULL.
 */
static inline bevh_status_t bevh_event_init(bevh_event_t *event,
                                            bevh_event_id_t id,
                                            bevh_source_id_t source,
                                            bevh_tick_t timestamp,
                                            bevh_param_t param,
                                            void *data)
{
    if(event == NULL) {
        return BEVH_ERR_NULL;
    }

    event->id = id;
    event->source = source;
    event->timestamp = timestamp;
    event->param = param;
    event->data = data;

    return BEVH_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* BEVH_EVENT_H */
