/**
 * @file bevh.h
 * @brief Top-level public API for the BEVH framework.
 *
 * This header combines the BEVH event queue, dispatcher, and cooperative timer
 * manager into one runtime context. It is a convenience layer; it must not add
 * hardware-specific behavior or application-specific policy.
 *
 * BEVH is not an RTOS. The application owns the main loop, the hardware
 * timebase, and all real-time hardware actions.
 */

#ifndef BEVH_H
#define BEVH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bevh_config.h"
#include "bevh_critical.h"
#include "bevh_dispatcher.h"
#include "bevh_event.h"
#include "bevh_event_queue.h"
#include "bevh_timer.h"
#include "bevh_types.h"

/**
 * @struct bevh_t
 * @brief Top-level BEVH runtime context.
 *
 * A BEVH context owns one event queue, one dispatcher, and one software timer
 * manager. The buffers used by those modules are supplied by the application
 * during @ref bevh_init.
 *
 * A context object must be zero-initialized before the first call to
 * @ref bevh_init. Static storage duration objects satisfy this requirement
 * automatically. Stack or dynamically supplied objects must be cleared by the
 * application before first initialization.
 */
typedef struct {
    /**
     * @brief Event queue used by the context.
     */
    bevh_event_queue_t queue;

    /**
     * @brief Dispatcher used by the context.
     */
    bevh_dispatcher_t dispatcher;

    /**
     * @brief Cooperative software timer manager used by the context.
     */
    bevh_timer_mgr_t timer_mgr;

    /**
     * @brief True after successful initialization.
     */
    bool initialized;
} bevh_t;

/**
 * @struct bevh_config_runtime_t
 * @brief Runtime buffer and port configuration for @ref bevh_init.
 *
 * BEVH does not allocate memory. The application must provide fixed-size
 * buffers for events, handler entries, and timers.
 */
typedef struct {
    /**
     * @brief Event queue storage.
     */
    bevh_event_t *event_buffer;

    /**
     * @brief Number of entries in @ref event_buffer.
     *
     * Must be a power of two because the event queue uses mask-based wrapping.
     */
    bevh_count_t event_capacity;

    /**
     * @brief Dispatcher handler entry storage.
     */
    bevh_handler_entry_t *handler_buffer;

    /**
     * @brief Number of entries in @ref handler_buffer.
     */
    bevh_count_t handler_capacity;

    /**
     * @brief Software timer storage.
     */
    bevh_timer_t *timer_buffer;

    /**
     * @brief Number of entries in @ref timer_buffer.
     */
    bevh_count_t timer_capacity;

    /**
     * @brief Critical-section callbacks used by the event queue.
     *
     * These callbacks are required when critical sections are enabled and null
     * critical sections are not allowed by configuration.
     */
    bevh_critical_t critical;
} bevh_config_runtime_t;

/**
 * @brief Initialize a BEVH context.
 *
 * This initializes the event queue, dispatcher, and software timer manager using
 * application-supplied buffers.
 *
 * @param ctx BEVH context to initialize.
 * @param cfg Runtime configuration and buffers.
 *
 * @retval BEVH_OK Context initialized successfully.
 * @retval BEVH_ERR_NULL @p ctx, @p cfg, or a required buffer was NULL.
 * @retval BEVH_ERR_INVALID_ARG A supplied capacity was invalid.
 * @retval BEVH_ERR_INVALID_CONFIG A submodule configuration was invalid.
 * @retval BEVH_ERR_ALREADY_INITIALIZED @p ctx was already initialized.
 */
bevh_status_t bevh_init(bevh_t *ctx, const bevh_config_runtime_t *cfg);

/**
 * @brief Post an event from normal context.
 *
 * This is a convenience wrapper around @ref bevh_event_push.
 *
 * @param ctx Initialized BEVH context.
 * @param event Event to copy into the context queue.
 *
 * @retval BEVH_OK Event was queued.
 * @retval BEVH_ERR_NULL @p ctx or @p event was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p ctx was not initialized.
 * @retval BEVH_ERR_FULL Queue was full and the event was dropped.
 */
bevh_status_t bevh_post(bevh_t *ctx, const bevh_event_t *event);

/**
 * @brief Post an event from ISR context.
 *
 * This is a convenience wrapper around @ref bevh_event_push_isr. It must remain
 * short and non-blocking.
 *
 * @param ctx Initialized BEVH context.
 * @param event Event to copy into the context queue.
 *
 * @retval BEVH_OK Event was queued.
 * @retval BEVH_ERR_NULL @p ctx or @p event was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p ctx was not initialized.
 * @retval BEVH_ERR_FULL Queue was full and the event was dropped.
 */
bevh_status_t bevh_post_isr(bevh_t *ctx, const bevh_event_t *event);

/**
 * @brief Subscribe a handler to an event ID.
 *
 * This is a convenience wrapper around @ref bevh_dispatcher_subscribe.
 *
 * @param ctx Initialized BEVH context.
 * @param event_id Event ID to subscribe to.
 * @param handler Handler callback.
 * @param user User pointer passed to @p handler.
 *
 * @retval BEVH_OK Handler was registered.
 * @retval BEVH_ERR_NULL @p ctx or @p handler was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p ctx was not initialized.
 * @retval BEVH_ERR_INVALID_ARG @p event_id was @ref BEVH_EVENT_NONE.
 * @retval BEVH_ERR_BUSY The same subscription already exists.
 * @retval BEVH_ERR_FULL No free handler entry was available.
 */
bevh_status_t bevh_subscribe(bevh_t *ctx, bevh_event_id_t event_id, bevh_event_handler_t handler, void *user);

/**
 * @brief Remove a handler subscription.
 *
 * This is a convenience wrapper around @ref bevh_dispatcher_unsubscribe.
 *
 * @param ctx Initialized BEVH context.
 * @param event_id Event ID used in the subscription.
 * @param handler Handler callback used in the subscription.
 * @param user User pointer used in the subscription.
 *
 * @return Status returned by @ref bevh_dispatcher_unsubscribe.
 */
bevh_status_t bevh_unsubscribe(bevh_t *ctx,
                               bevh_event_id_t event_id,
                               bevh_event_handler_t handler,
                               void *user);

/**
 * @brief Load multiple handler subscriptions from a table.
 *
 * This is a convenience wrapper around @ref bevh_dispatcher_subscribe_table.
 *
 * @param ctx Initialized BEVH context.
 * @param table Handler entry table to load.
 * @param count Number of entries in @p table.
 *
 * @return Status returned by @ref bevh_dispatcher_subscribe_table.
 */
bevh_status_t bevh_subscribe_table(bevh_t *ctx, const bevh_handler_entry_t *table, bevh_count_t count);

/**
 * @brief Start or reconfigure a software timer.
 *
 * This is a convenience wrapper around @ref bevh_timer_start.
 *
 * @param ctx Initialized BEVH context.
 * @param cfg Timer configuration.
 *
 * @return Status returned by @ref bevh_timer_start.
 */
bevh_status_t bevh_start_timer(bevh_t *ctx, const bevh_timer_config_t *cfg);

/**
 * @brief Stop a software timer.
 *
 * This is a convenience wrapper around @ref bevh_timer_stop.
 *
 * @param ctx Initialized BEVH context.
 * @param timer_id Timer ID to stop.
 *
 * @return Status returned by @ref bevh_timer_stop.
 */
bevh_status_t bevh_stop_timer(bevh_t *ctx,
                              bevh_timer_id_t timer_id);

/**
 * @brief Restart a previously configured software timer.
 *
 * This is a convenience wrapper around @ref bevh_timer_restart.
 *
 * @param ctx Initialized BEVH context.
 * @param timer_id Timer ID to restart.
 *
 * @return Status returned by @ref bevh_timer_restart.
 */
bevh_status_t bevh_restart_timer(bevh_t *ctx,
                                 bevh_timer_id_t timer_id);

/**
 * @brief Advance the context software timers by elapsed ticks.
 *
 * This is a convenience wrapper around @ref bevh_timer_tick.
 *
 * The same context rules as @ref bevh_timer_tick apply: do not call this from
 * one context while modifying timers from another unless the application
 * provides protection.
 *
 * @param ctx Initialized BEVH context.
 * @param elapsed_ticks Number of elapsed application-defined ticks.
 */
void bevh_tick(bevh_t *ctx, bevh_tick_t elapsed_ticks);

/**
 * @brief Process at most one queued event.
 *
 * This function pops one event from the context queue and dispatches it if one
 * was available. It returns immediately and reports the actual result.
 *
 * @param ctx Initialized BEVH context.
 *
 * @retval BEVH_OK One event was popped and dispatched successfully, or dispatch
 *         returned OK.
 * @retval BEVH_ERR_NULL @p ctx was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p ctx was not initialized.
 * @retval BEVH_ERR_EMPTY No event was available.
 * @retval BEVH_ERR_NOT_FOUND No handler matched and dispatcher configuration
 *         treats missing handlers as an error.
 * @retval BEVH_ERR_INVALID_ARG The popped event was invalid.
 */
bevh_status_t bevh_run_once(bevh_t *ctx);

/**
 * @brief Process queued events and return.
 *
 * If @ref BEVH_RUN_MAX_EVENTS_PER_CALL is 0, this function drains the queue and
 * returns when it is empty. Otherwise it processes at most that many events.
 *
 * This function must not contain an infinite loop. The application owns the main
 * loop.
 *
 * @param ctx Initialized BEVH context.
 * @param processed_count Optional destination for the number of events
 *        successfully dispatched. May be NULL if the caller does not need it.
 *
 * @retval BEVH_OK No processing error occurred. The queue may have become empty
 *         or the configured per-call event budget may have been reached.
 * @retval BEVH_ERR_NULL @p ctx was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p ctx was not initialized.
 * @retval BEVH_ERR_NOT_FOUND A dispatch operation failed because no handler
 *         matched and dispatcher configuration treats that as an error.
 * @retval BEVH_ERR_INVALID_ARG A popped event was invalid.
 */
bevh_status_t bevh_run(bevh_t *ctx,
                       bevh_count_t *processed_count);

#ifdef __cplusplus
}
#endif

#endif /* BEVH_H */
