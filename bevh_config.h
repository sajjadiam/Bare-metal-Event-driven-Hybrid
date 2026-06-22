/**
 * @file bevh_config.h
 * @brief Compile-time configuration defaults for the BEVH framework.
 *
 * This header provides portable default configuration values for BEVH. Every
 * setting is override-friendly: a project or build system may define the macro
 * before including this header or pass it through compiler flags.
 *
 * This file must stay hardware-independent. Do not include MCU, HAL, LL, CMSIS,
 * RTOS, board, or application headers here.
 */

#ifndef BEVH_CONFIG_H
#define BEVH_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def BEVH_EVENT_QUEUE_SIZE
 * @brief Default number of events in a statically configured event queue.
 *
 * This value is only a default. Runtime APIs may still accept user-supplied
 * buffers with different capacities.
 *
 * The value must be a power of two. The event queue uses a ring-buffer mask
 * instead of a modulo operation for deterministic and efficient index wrapping.
 */
#ifndef BEVH_EVENT_QUEUE_SIZE
#define BEVH_EVENT_QUEUE_SIZE 32u
#endif

/**
 * @def BEVH_MAX_HANDLERS
 * @brief Default maximum number of dispatcher handler registrations.
 *
 * Multiple handlers may subscribe to the same event ID, and each subscription
 * consumes one handler slot.
 */
#ifndef BEVH_MAX_HANDLERS
#define BEVH_MAX_HANDLERS 16u
#endif

/**
 * @def BEVH_MAX_TIMERS
 * @brief Default maximum number of cooperative software timers.
 *
 * These timers are for cooperative scheduling and soft timeouts only. They are
 * not suitable for precision PWM, input capture, gate pulses, ADC triggers, or
 * safety shutdown timing.
 */
#ifndef BEVH_MAX_TIMERS
#define BEVH_MAX_TIMERS 16u
#endif

/**
 * @def BEVH_ENABLE_DIAG
 * @brief Enable or disable generic framework diagnostic counters.
 *
 * Set to 1 to compile diagnostic support. Set to 0 for a smaller build.
 */
#ifndef BEVH_ENABLE_DIAG
#define BEVH_ENABLE_DIAG 1u
#endif

/**
 * @def BEVH_ENABLE_CRITICAL
 * @brief Enable or disable critical-section protection hooks.
 *
 * Set to 1 when events can be posted from ISR context or from multiple
 * execution contexts. Set to 0 only for single-context host tests or tightly
 * controlled applications that do not need protection.
 */
#ifndef BEVH_ENABLE_CRITICAL
#define BEVH_ENABLE_CRITICAL 1u
#endif

/**
 * @def BEVH_DISPATCH_MISSING_HANDLER_IS_ERROR
 * @brief Configure dispatcher behavior when no handler matches an event.
 *
 * When set to 0, dispatching an event with no handler returns BEVH_OK. This is
 * the default because unhandled events are often harmless in event-driven
 * firmware.
 *
 * When set to 1, dispatching an event with no handler returns
 * BEVH_ERR_NOT_FOUND.
 */
#ifndef BEVH_DISPATCH_MISSING_HANDLER_IS_ERROR
#define BEVH_DISPATCH_MISSING_HANDLER_IS_ERROR 0u
#endif

/**
 * @def BEVH_ALLOW_NULL_CRITICAL
 * @brief Allow initialization without critical-section callbacks.
 *
 * When set to 1, queue initialization may accept a NULL critical-section
 * callback table and operate without protection. This is useful for host tests
 * and strictly single-context applications.
 *
 * For real embedded systems that call bevh_post_isr(), keep this disabled
 * unless the queue implementation provides another proven protection method.
 */
#ifndef BEVH_ALLOW_NULL_CRITICAL
#define BEVH_ALLOW_NULL_CRITICAL 0u
#endif

/**
 * @def BEVH_TIMER_ALLOW_ZERO_DELAY
 * @brief Configure whether software timers may be started with zero delay.
 *
 * When set to 0, zero-delay timers are rejected as invalid arguments. This
 * avoids accidental event storms. If an immediate event is needed, post it
 * directly instead of starting a zero-delay timer.
 */
#ifndef BEVH_TIMER_ALLOW_ZERO_DELAY
#define BEVH_TIMER_ALLOW_ZERO_DELAY 0u
#endif

/**
 * @def BEVH_TIMER_MAX_CATCHUP_EVENTS
 * @brief Maximum expiration events posted per periodic timer per tick call.
 *
 * When a periodic timer is advanced by a large elapsed tick value, more than one
 * period may have expired. This limit bounds how many events one timer may post
 * during a single bevh_timer_tick() call. Extra expirations are coalesced and
 * tracked by the timer implementation.
 *
 * The value must be greater than zero. A value of 1 keeps queue pressure low by
 * posting at most one event per timer per tick call. Larger values reduce
 * catch-up latency but can fill the event queue faster.
 */
#ifndef BEVH_TIMER_MAX_CATCHUP_EVENTS
#define BEVH_TIMER_MAX_CATCHUP_EVENTS 4u
#endif

/**
 * @def BEVH_RUN_MAX_EVENTS_PER_CALL
 * @brief Maximum number of events processed by bevh_run() per call.
 *
 * A value of 0 means bevh_run() drains the queue and returns when it is empty.
 * A nonzero value bounds work per call, which can help keep a cooperative main
 * loop responsive.
 */
#ifndef BEVH_RUN_MAX_EVENTS_PER_CALL
#define BEVH_RUN_MAX_EVENTS_PER_CALL 0u
#endif

#ifdef __cplusplus
}
#endif

#endif /* BEVH_CONFIG_H */
