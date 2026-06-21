/**
 * @file bevh_event_queue.h
 * @brief Fixed-size FIFO event queue API for BEVH.
 *
 * The BEVH event queue moves events from producers, including ISR-level
 * producers, to the cooperative main-loop dispatcher. Events are copied into a
 * user-supplied fixed-size ring buffer.
 *
 * Queue capacity must be a power of two. This lets the implementation wrap ring
 * indexes with a mask instead of a modulo operation.
 *
 * This module must not dispatch events, call user handlers, allocate memory, or
 * block. It only stores and retrieves events.
 */

#ifndef BEVH_EVENT_QUEUE_H
#define BEVH_EVENT_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bevh_config.h"
#include "bevh_critical.h"
#include "bevh_event.h"
#include "bevh_types.h"

/**
 * @struct bevh_event_queue_t
 * @brief Runtime object for a fixed-size event FIFO.
 *
 * The queue uses a ring buffer supplied by the caller. Public functions validate
 * initialization before using the queue.
 *
 * A queue object must be zero-initialized before the first call to
 * @ref bevh_event_queue_init. Static storage duration objects satisfy this
 * requirement automatically. Stack or dynamically supplied objects must be
 * cleared by the application before first initialization.
 */
typedef struct {
    /**
     * @brief User-supplied event storage.
     *
     * The buffer must contain at least @ref capacity entries and must remain
     * valid for the lifetime of the queue.
     */
    bevh_event_t *buffer;

    /**
     * @brief Number of events that can be stored in @ref buffer.
     *
     * This value must be a power of two.
     */
    bevh_count_t capacity;

    /**
     * @brief Ring-buffer wrapping mask.
     *
     * This value is initialized to @c capacity - 1 and is used to wrap indexes
     * with bitwise AND. It is valid only when @ref capacity is a power of two.
     */
    bevh_count_t mask;

    /**
     * @brief Write index of the ring buffer.
     *
     * This field may be accessed from ISR and main context, so queue operations
     * protect it through the configured critical section when enabled.
     */
    volatile bevh_count_t head;

    /**
     * @brief Read index of the ring buffer.
     *
     * This field may be accessed from ISR and main context, so queue operations
     * protect it through the configured critical section when enabled.
     */
    volatile bevh_count_t tail;

    /**
     * @brief Current number of queued events.
     */
    volatile bevh_count_t count;

    /**
     * @brief Number of events dropped because the queue was full.
     *
     * This counter must increment when a push operation fails with
     * @ref BEVH_ERR_FULL.
     */
    volatile uint32_t overflow_count;

    /**
     * @brief Highest observed queue usage.
     *
     * This value is diagnostic information and may be compiled or updated
     * depending on configuration.
     */
    bevh_count_t max_used;

    /**
     * @brief True after successful initialization.
     */
    bool initialized;

    /**
     * @brief Critical-section callbacks used to protect shared queue state.
     */
    bevh_critical_t critical;
} bevh_event_queue_t;

/**
 * @brief Initialize a fixed-size event queue.
 *
 * The queue object must be zero-initialized before the first initialization
 * attempt. This is required because the initialization function checks the
 * existing @c initialized flag to reject accidental double initialization.
 *
 * @param q Queue object to initialize.
 * @param buffer User-supplied event buffer.
 * @param capacity Number of event entries in @p buffer. Must be a power of two.
 * @param critical Critical-section callbacks used to protect shared state.
 *
 * @retval BEVH_OK Queue initialized successfully.
 * @retval BEVH_ERR_NULL @p q or @p buffer was NULL, or @p critical was NULL
 *         when null critical sections are not allowed.
 * @retval BEVH_ERR_INVALID_ARG @p capacity was zero or not a power of two.
 * @retval BEVH_ERR_INVALID_CONFIG Critical-section callbacks were incomplete.
 * @retval BEVH_ERR_ALREADY_INITIALIZED @p q was already initialized.
 */
bevh_status_t bevh_event_queue_init(bevh_event_queue_t *q,
                                    bevh_event_t *buffer,
                                    bevh_count_t capacity,
                                    const bevh_critical_t *critical);

/**
 * @brief Push an event into the queue from normal context.
 *
 * The event is copied by value into the queue. If @c event->data is not NULL,
 * only the pointer is copied; BEVH does not own the pointed-to object.
 *
 * @param q Initialized queue object.
 * @param event Event to copy into the queue.
 *
 * @retval BEVH_OK Event was queued.
 * @retval BEVH_ERR_NULL @p q or @p event was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p q was not initialized.
 * @retval BEVH_ERR_FULL Queue was full and the event was dropped.
 */
bevh_status_t bevh_event_push(bevh_event_queue_t *q,
                              const bevh_event_t *event);

/**
 * @brief Push an event into the queue from ISR context.
 *
 * This function must remain short and non-blocking. It must not call event
 * handlers, dispatch events, allocate memory, or wait for space.
 *
 * @param q Initialized queue object.
 * @param event Event to copy into the queue.
 *
 * @retval BEVH_OK Event was queued.
 * @retval BEVH_ERR_NULL @p q or @p event was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p q was not initialized.
 * @retval BEVH_ERR_FULL Queue was full and the event was dropped.
 */
bevh_status_t bevh_event_push_isr(bevh_event_queue_t *q, const bevh_event_t *event);

/**
 * @brief Pop one event from the queue.
 *
 * This function is intended for main-loop context. It copies the oldest queued
 * event into @p event.
 *
 * @param q Initialized queue object.
 * @param event Destination for the popped event.
 *
 * @retval BEVH_OK Event was popped.
 * @retval BEVH_ERR_NULL @p q or @p event was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p q was not initialized.
 * @retval BEVH_ERR_EMPTY Queue was empty.
 */
bevh_status_t bevh_event_pop(bevh_event_queue_t *q, bevh_event_t *event);

/**
 * @brief Remove all queued events and reset queue indexes.
 *
 * This function does not clear the user-supplied event buffer contents. It only
 * resets queue bookkeeping. Diagnostic counters such as overflow count and
 * maximum observed usage are preserved.
 *
 * @param q Queue object.
 */
void bevh_event_queue_clear(bevh_event_queue_t *q);

/**
 * @brief Return whether the queue contains no events.
 *
 * @param q Queue object.
 *
 * @retval true Queue is NULL, uninitialized, or empty.
 * @retval false Queue contains at least one event.
 */
bool bevh_event_queue_is_empty(const bevh_event_queue_t *q);

/**
 * @brief Return whether the queue is full.
 *
 * @param q Queue object.
 *
 * @retval true Queue is initialized and full.
 * @retval false Queue is NULL, uninitialized, or not full.
 */
bool bevh_event_queue_is_full(const bevh_event_queue_t *q);

/**
 * @brief Return the current number of queued events.
 *
 * @param q Queue object.
 *
 * @return Current event count, or 0 if @p q is NULL or uninitialized.
 */
bevh_count_t bevh_event_queue_count(const bevh_event_queue_t *q);

/**
 * @brief Return the queue capacity.
 *
 * @param q Queue object.
 *
 * @return Queue capacity, or 0 if @p q is NULL or uninitialized.
 */
bevh_count_t bevh_event_queue_capacity(const bevh_event_queue_t *q);

/**
 * @brief Return the number of dropped events caused by queue overflow.
 *
 * @param q Queue object.
 *
 * @return Overflow counter, or 0 if @p q is NULL or uninitialized.
 */
uint32_t bevh_event_queue_overflow_count(const bevh_event_queue_t *q);

/**
 * @brief Return the highest observed queue usage.
 *
 * @param q Queue object.
 *
 * @return Maximum observed count, or 0 if @p q is NULL or uninitialized.
 */
bevh_count_t bevh_event_queue_max_used(const bevh_event_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* BEVH_EVENT_QUEUE_H */
