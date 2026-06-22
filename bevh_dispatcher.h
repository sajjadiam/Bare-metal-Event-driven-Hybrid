/**
 * @file bevh_dispatcher.h
 * @brief Event dispatcher API for the BEVH framework.
 *
 * The dispatcher routes an already-popped event to registered event handlers.
 * It does not read from an event queue, post events, allocate memory, block, or
 * contain application-specific logic.
 *
 * A dispatcher owns no handler storage by itself. The application supplies a
 * fixed-size handler entry buffer during initialization.
 *
 * Applications that prefer reviewable static routing may define a const table
 * of handler entries and load it during startup with
 * @ref bevh_dispatcher_subscribe_table. The dispatcher still uses its mutable
 * runtime buffer internally.
 */

#ifndef BEVH_DISPATCHER_H
#define BEVH_DISPATCHER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bevh_config.h"
#include "bevh_event.h"
#include "bevh_types.h"

/**
 * @typedef bevh_event_handler_t
 * @brief Callback type for handling dispatched events.
 *
 * Handlers are called from the context that invokes
 * @ref bevh_dispatcher_dispatch. In a normal BEVH application this is the
 * cooperative main loop, not an ISR.
 *
 * Handlers must be short and non-blocking. They must not perform hard-real-time
 * power-electronics actions such as gate timing, PWM edge timing, or emergency
 * shutdown sequencing.
 *
 * @param event Event being dispatched. The handler must not modify it.
 * @param user User pointer supplied during subscription.
 */
typedef void (*bevh_event_handler_t)(const bevh_event_t *event, void *user);

/**
 * @struct bevh_handler_entry_t
 * @brief One dispatcher subscription entry.
 *
 * Each entry binds one event ID to one callback and one user pointer. Multiple
 * entries may use the same event ID, allowing multiple handlers to receive the
 * same event.
 */
typedef struct {
    /**
     * @brief Event ID matched by this entry.
     */
    bevh_event_id_t event_id;

    /**
     * @brief Handler called when a matching event is dispatched.
     */
    bevh_event_handler_t handler;

    /**
     * @brief User pointer passed to @ref handler.
     */
    void *user;

    /**
     * @brief True when this entry is occupied.
     */
    bool used;
} bevh_handler_entry_t;

/**
 * @struct bevh_dispatcher_t
 * @brief Runtime object for event handler registration and dispatch.
 *
 * The dispatcher uses a user-supplied fixed-size array of
 * @ref bevh_handler_entry_t entries. It does not allocate memory internally.
 *
 * A dispatcher object must be zero-initialized before the first call to
 * @ref bevh_dispatcher_init. Static storage duration objects satisfy this
 * requirement automatically. Stack or dynamically supplied objects must be
 * cleared by the application before first initialization.
 */
typedef struct {
    /**
     * @brief User-supplied handler entry storage.
     *
     * The buffer must contain at least @ref capacity entries and must remain
     * valid for the lifetime of the dispatcher.
     */
    bevh_handler_entry_t *handlers;

    /**
     * @brief Number of entries in @ref handlers.
     */
    bevh_count_t capacity;

    /**
     * @brief Number of currently used handler entries.
     */
    bevh_count_t count;

    /**
     * @brief True after successful initialization.
     */
    bool initialized;
} bevh_dispatcher_t;

/**
 * @brief Initialize an event dispatcher.
 *
 * The dispatcher object must be zero-initialized before the first initialization
 * attempt. The handler buffer is cleared during initialization, even when the
 * application plans to load a static subscription table afterward.
 *
 * @param d Dispatcher object to initialize.
 * @param handlers User-supplied handler entry buffer.
 * @param capacity Number of entries in @p handlers.
 *
 * @retval BEVH_OK Dispatcher initialized successfully.
 * @retval BEVH_ERR_NULL @p d or @p handlers was NULL.
 * @retval BEVH_ERR_INVALID_ARG @p capacity was zero.
 * @retval BEVH_ERR_ALREADY_INITIALIZED @p d was already initialized.
 */
bevh_status_t bevh_dispatcher_init(bevh_dispatcher_t *d, bevh_handler_entry_t *handlers, bevh_count_t capacity);

/**
 * @brief Subscribe a handler to an event ID.
 *
 * Multiple handlers may subscribe to the same event ID. The same tuple of
 * event ID, handler, and user pointer should not be registered more than once.
 *
 * @param d Initialized dispatcher object.
 * @param event_id Event ID to subscribe to.
 * @param handler Callback to invoke when a matching event is dispatched.
 * @param user User pointer passed to @p handler.
 *
 * @retval BEVH_OK Handler was registered.
 * @retval BEVH_ERR_NULL @p d or @p handler was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p d was not initialized.
 * @retval BEVH_ERR_INVALID_ARG @p event_id was @ref BEVH_EVENT_NONE.
 * @retval BEVH_ERR_BUSY The same subscription already exists.
 * @retval BEVH_ERR_FULL No free handler entry was available.
 */
bevh_status_t bevh_dispatcher_subscribe(bevh_dispatcher_t *d, bevh_event_id_t event_id, bevh_event_handler_t handler, void *user);

/**
 * @brief Subscribe multiple handlers from a static or read-only table.
 *
 * This helper is intended for applications that want a reviewable startup route
 * table instead of scattered individual subscribe calls. Each used entry in
 * @p table is registered using the same rules as
 * @ref bevh_dispatcher_subscribe.
 *
 * The table is not stored by pointer. Entries are copied into the dispatcher's
 * mutable runtime handler buffer. Therefore @p table may be const and may live
 * in read-only memory.
 *
 * Entries whose @c used field is false are ignored. Entries with
 * @ref BEVH_EVENT_NONE or NULL handlers are rejected.
 *
 * @param d Initialized dispatcher object.
 * @param table Handler entry table to load.
 * @param count Number of entries in @p table.
 *
 * @retval BEVH_OK All used table entries were registered.
 * @retval BEVH_ERR_NULL @p d or @p table was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p d was not initialized.
 * @retval BEVH_ERR_INVALID_ARG @p count was zero, or a used table entry had
 *         @ref BEVH_EVENT_NONE.
 * @retval BEVH_ERR_INVALID_CONFIG A used table entry had a NULL handler.
 * @retval BEVH_ERR_BUSY A duplicate subscription was found.
 * @retval BEVH_ERR_FULL The dispatcher did not have enough free entries.
 */
bevh_status_t bevh_dispatcher_subscribe_table(bevh_dispatcher_t *d, const bevh_handler_entry_t *table, bevh_count_t count);

/**
 * @brief Remove a handler subscription.
 *
 * The subscription is identified by the exact event ID, handler pointer, and
 * user pointer tuple.
 *
 * @param d Initialized dispatcher object.
 * @param event_id Event ID used in the subscription.
 * @param handler Callback used in the subscription.
 * @param user User pointer used in the subscription.
 *
 * @retval BEVH_OK Handler subscription was removed.
 * @retval BEVH_ERR_NULL @p d or @p handler was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p d was not initialized.
 * @retval BEVH_ERR_INVALID_ARG @p event_id was @ref BEVH_EVENT_NONE.
 * @retval BEVH_ERR_NOT_FOUND No matching subscription was found.
 */
bevh_status_t bevh_dispatcher_unsubscribe(bevh_dispatcher_t *d, bevh_event_id_t event_id, bevh_event_handler_t handler, void *user);

/**
 * @brief Dispatch one event to all matching handlers.
 *
 * Dispatch scans the registered handler table and calls every used entry whose
 * event ID matches @c event->id. The event object is not modified.
 *
 * This function must not be called from hard-real-time ISR paths if handlers
 * can block or take nontrivial time. In normal BEVH usage, the top-level core
 * calls it from the cooperative main loop after popping an event from the queue.
 *
 * @param d Initialized dispatcher object.
 * @param event Event to dispatch.
 *
 * @retval BEVH_OK Event was dispatched, or no handler was found and
 *         @ref BEVH_DISPATCH_MISSING_HANDLER_IS_ERROR is 0.
 * @retval BEVH_ERR_NULL @p d or @p event was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p d was not initialized.
 * @retval BEVH_ERR_INVALID_ARG @c event->id was @ref BEVH_EVENT_NONE.
 * @retval BEVH_ERR_NOT_FOUND No handler matched and
 *         @ref BEVH_DISPATCH_MISSING_HANDLER_IS_ERROR is 1.
 */
bevh_status_t bevh_dispatcher_dispatch(bevh_dispatcher_t *d, const bevh_event_t *event);

/**
 * @brief Return the current number of registered handler entries.
 *
 * @param d Dispatcher object.
 *
 * @return Registered handler count, or 0 if @p d is NULL or uninitialized.
 */
bevh_count_t bevh_dispatcher_count(const bevh_dispatcher_t *d);

/**
 * @brief Return the handler entry capacity.
 *
 * @param d Dispatcher object.
 *
 * @return Handler capacity, or 0 if @p d is NULL or uninitialized.
 */
bevh_count_t bevh_dispatcher_capacity(const bevh_dispatcher_t *d);

#ifdef __cplusplus
}
#endif

#endif /* BEVH_DISPATCHER_H */
