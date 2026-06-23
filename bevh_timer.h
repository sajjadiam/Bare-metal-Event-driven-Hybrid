/**
 * @file bevh_timer.h
 * @brief Cooperative software timer manager for the BEVH framework.
 *
 * The BEVH timer manager provides one-shot and periodic software timers that
 * generate events when they expire. It is not a hardware timer driver and does
 * not access SysTick, MCU registers, HAL, LL, CMSIS, RTOS, or board code.
 *
 * The application owns the real timebase. It advances this module by calling
 * @ref bevh_timer_tick with the number of elapsed application-defined ticks.
 *
 * Expired timers post events into a BEVH event queue. Timer callbacks are not
 * called directly by this module.
 *
 * Periodic timers use configurable catch-up behavior. If
 * @ref BEVH_TIMER_ENABLE_CATCHUP is 0, each periodic timer posts at most one
 * event per @ref bevh_timer_tick call and coalesces extra elapsed periods into
 * missed-count state. If catch-up is enabled, each periodic timer may post up to
 * @ref BEVH_TIMER_MAX_CATCHUP_EVENTS events per tick call.
 *
 * This module is suitable for cooperative timing such as communication
 * timeouts, retry delays, UI refresh, diagnostics, and slow monitoring. It is
 * not suitable for precision PWM, input capture, thyristor firing, gate pulse
 * timing, ADC trigger timing, inverter timing, or safety shutdown.
 */

#ifndef BEVH_TIMER_H
#define BEVH_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bevh_config.h"
#include "bevh_event.h"
#include "bevh_event_queue.h"
#include "bevh_types.h"

/**
 * @def BEVH_TIMER_ID_NONE
 * @brief Invalid or unused timer identifier.
 *
 * Timer slots reset to this ID during manager initialization. Applications
 * should not use this value as a real timer ID.
 */
#define BEVH_TIMER_ID_NONE    ((bevh_timer_id_t)0u)

/**
 * @struct bevh_timer_t
 * @brief One cooperative software timer object.
 *
 * Timer objects are owned by a @ref bevh_timer_mgr_t. Applications should not
 * modify fields directly after manager initialization, except through the timer
 * manager API.
 */
typedef struct {
    /**
     * @brief Application-defined timer identifier.
     *
     * Timer IDs are unique within one timer manager. A cleared or unused timer
     * slot has @ref BEVH_TIMER_ID_NONE.
     */
    bevh_timer_id_t id;

    /**
     * @brief Ticks remaining before the timer expires.
     */
    bevh_tick_t remaining_ticks;

    /**
     * @brief Reload period in application-defined ticks.
     *
     * For one-shot timers this stores the original delay. For periodic timers
     * this is the reload value after expiration.
     */
    bevh_tick_t period_ticks;

    /**
     * @brief Event ID posted when the timer expires.
     */
    bevh_event_id_t event_id;

    /**
     * @brief Source ID stored in the generated event.
     */
    bevh_source_id_t source;

    /**
     * @brief Lightweight parameter stored in the generated event.
     *
     * This value is copied into each generated event. Catch-up metadata is kept
     * separately in @ref missed_count and does not overwrite this parameter.
     */
    bevh_param_t param;

    /**
     * @brief Optional application-owned data pointer stored in generated events.
     *
     * BEVH does not allocate, free, copy, or own this pointer. The application
     * must guarantee that the pointed-to object remains valid until generated
     * timer events are consumed.
     */
    void *data;

    /**
     * @brief Number of elapsed periodic expirations coalesced by timer policy.
     *
     * When more periods elapsed than the configured timer policy is allowed to
     * post in one @ref bevh_timer_tick call, the extra expirations are
     * accumulated here. This is diagnostic/state information for the timer
     * manager; applications should read it only through future accessor APIs if
     * needed.
     */
    uint32_t missed_count;

    /**
     * @brief True for periodic timers, false for one-shot timers.
     *
     * A cleared or unused timer slot sets this field to false.
     */
    bool periodic;

    /**
     * @brief True while the timer is running.
     *
     * A cleared, stopped, or unused timer slot sets this field to false.
     */
    bool active;
} bevh_timer_t;

/**
 * @struct bevh_timer_config_t
 * @brief Configuration used to start or reconfigure one timer.
 *
 * This structure groups timer start parameters to avoid long positional
 * argument lists and reduce accidental parameter swaps in application code.
 */
typedef struct {
    /**
     * @brief Application-defined timer identifier.
     *
     * Must not be @ref BEVH_TIMER_ID_NONE.
     */
    bevh_timer_id_t timer_id;

    /**
     * @brief Delay before expiration in application-defined ticks.
     *
     * Must be nonzero unless @ref BEVH_TIMER_ALLOW_ZERO_DELAY is enabled.
     * Zero-delay periodic timers are always invalid.
     */
    bevh_tick_t delay_ticks;

    /**
     * @brief True for periodic timer, false for one-shot timer.
     */
    bool periodic;

    /**
     * @brief Event ID posted when the timer expires.
     *
     * Must not be @ref BEVH_EVENT_NONE.
     */
    bevh_event_id_t event_id;

    /**
     * @brief Source ID stored in generated events.
     */
    bevh_source_id_t source;

    /**
     * @brief Lightweight parameter stored in generated events.
     */
    bevh_param_t param;

    /**
     * @brief Optional application-owned data pointer stored in generated events.
     */
    void *data;
} bevh_timer_config_t;

/**
 * @struct bevh_timer_mgr_t
 * @brief Runtime object for managing multiple cooperative timers.
 *
 * The manager uses a user-supplied fixed-size timer array and posts expiration
 * events into a user-supplied BEVH event queue.
 *
 * A timer manager object must be zero-initialized before the first call to
 * @ref bevh_timer_mgr_init. Static storage duration objects satisfy this
 * requirement automatically. Stack or dynamically supplied objects must be
 * cleared by the application before first initialization.
 */
typedef struct {
    /**
     * @brief User-supplied timer storage.
     *
     * The buffer must contain at least @ref capacity entries and must remain
     * valid for the lifetime of the timer manager.
     */
    bevh_timer_t *timers;

    /**
     * @brief Number of timer entries in @ref timers.
     */
    bevh_count_t capacity;

    /**
     * @brief Queue used for posting timer expiration events.
     */
    bevh_event_queue_t *queue;

    /**
     * @brief True after successful initialization.
     */
    bool initialized;
} bevh_timer_mgr_t;

/**
 * @brief Initialize a timer manager.
 *
 * The timer manager object must be zero-initialized before the first
 * initialization attempt. The timer buffer is reset during initialization.
 *
 * @param mgr Timer manager object to initialize.
 * @param timers User-supplied timer buffer.
 * @param capacity Number of entries in @p timers.
 * @param queue Initialized event queue used for expiration events.
 *
 * @retval BEVH_OK Timer manager initialized successfully.
 * @retval BEVH_ERR_NULL @p mgr, @p timers, or @p queue was NULL.
 * @retval BEVH_ERR_INVALID_ARG @p capacity was zero.
 * @retval BEVH_ERR_INVALID_CONFIG @p queue was not initialized.
 * @retval BEVH_ERR_ALREADY_INITIALIZED @p mgr was already initialized.
 */
bevh_status_t bevh_timer_mgr_init(bevh_timer_mgr_t *mgr, bevh_timer_t *timers, bevh_count_t capacity, bevh_event_queue_t *queue);

/**
 * @brief Start or reconfigure a timer.
 *
 * If a timer with @c cfg->timer_id already exists in the manager, it is
 * overwritten with the new configuration. Otherwise an unused timer slot is
 * allocated.
 *
 * When the timer expires, the manager posts an event using the event fields
 * stored in @p cfg into the configured event queue.
 *
 * @param mgr Initialized timer manager.
 * @param cfg Timer configuration.
 *
 * @retval BEVH_OK Timer was started.
 * @retval BEVH_ERR_NULL @p mgr or @p cfg was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p mgr was not initialized.
 * @retval BEVH_ERR_INVALID_ARG @c cfg->timer_id was @ref BEVH_TIMER_ID_NONE,
 *         @c cfg->delay_ticks was zero when zero-delay timers are disabled,
 *         @c cfg requested a zero-delay periodic timer, or @c cfg->event_id was
 *         @ref BEVH_EVENT_NONE.
 * @retval BEVH_ERR_FULL No free timer slot was available.
 */
bevh_status_t bevh_timer_start(bevh_timer_mgr_t *mgr, const bevh_timer_config_t *cfg);

/**
 * @brief Stop a timer.
 *
 * Stopping a timer makes it inactive but keeps its configuration so it may be
 * restarted later with @ref bevh_timer_restart.
 *
 * @param mgr Initialized timer manager.
 * @param timer_id Timer ID to stop.
 *
 * @retval BEVH_OK Timer was stopped.
 * @retval BEVH_ERR_NULL @p mgr was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p mgr was not initialized.
 * @retval BEVH_ERR_NOT_FOUND No timer with @p timer_id exists.
 */
bevh_status_t bevh_timer_stop(bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id);

/**
 * @brief Restart a previously configured timer.
 *
 * The timer is restarted using its stored @c period_ticks, @c periodic,
 * @c event_id, @c source, @c param, and @c data fields.
 *
 * @param mgr Initialized timer manager.
 * @param timer_id Timer ID to restart.
 *
 * @retval BEVH_OK Timer was restarted.
 * @retval BEVH_ERR_NULL @p mgr was NULL.
 * @retval BEVH_ERR_NOT_INITIALIZED @p mgr was not initialized.
 * @retval BEVH_ERR_NOT_FOUND No timer with @p timer_id exists.
 * @retval BEVH_ERR_INVALID_CONFIG Timer exists but has zero period or invalid
 *         event ID.
 */
bevh_status_t bevh_timer_restart(bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id);

/**
 * @brief Advance all active timers by elapsed ticks.
 *
 * This function updates active timers and posts events for timers that expire.
 * It does not call event handlers directly. If posting to the event queue fails
 * because the queue is full, this function does not block.
 *
 * The timer manager does not contain its own critical-section protection.
 * Calls that modify the same timer manager, including @ref bevh_timer_tick,
 * @ref bevh_timer_start, @ref bevh_timer_stop, and @ref bevh_timer_restart,
 * must be made from one execution context or externally protected by the
 * application. Do not call this function from an ISR while modifying the same
 * manager from the main loop unless the application provides that protection.
 *
 * Periodic timers use configurable catch-up behavior. If
 * @ref BEVH_TIMER_ENABLE_CATCHUP is 0, each periodic timer posts at most one
 * event during this call; additional elapsed periods are coalesced into the
 * timer's missed-count state. If catch-up is enabled, each periodic timer may
 * post up to @ref BEVH_TIMER_MAX_CATCHUP_EVENTS events during this call.
 *
 * A one-shot timer posts at most one event and then becomes inactive.
 *
 * @param mgr Initialized timer manager.
 * @param elapsed_ticks Number of ticks elapsed since the previous call.
 */
void bevh_timer_tick(bevh_timer_mgr_t *mgr, bevh_tick_t elapsed_ticks);

/**
 * @brief Return whether a timer exists and is active.
 *
 * @param mgr Timer manager object.
 * @param timer_id Timer ID to query.
 *
 * @retval true Timer exists and is active.
 * @retval false Manager is NULL, uninitialized, timer was not found, or timer is
 *         inactive.
 */
bool bevh_timer_is_active(const bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id);

/**
 * @brief Return the remaining ticks for a timer.
 *
 * @param mgr Timer manager object.
 * @param timer_id Timer ID to query.
 *
 * @return Remaining ticks, or 0 if manager is NULL, uninitialized, timer was not
 *         found, or timer is inactive.
 */
bevh_tick_t bevh_timer_remaining(const bevh_timer_mgr_t *mgr, bevh_timer_id_t timer_id);

/**
 * @brief Return the number of timer slots managed by this object.
 *
 * @param mgr Timer manager object.
 *
 * @return Timer capacity, or 0 if @p mgr is NULL or uninitialized.
 */
bevh_count_t bevh_timer_capacity(const bevh_timer_mgr_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* BEVH_TIMER_H */
