/**
 * @file bevh_types.h
 * @brief Common public types and status codes for the BEVH framework.
 *
 * BEVH is a small bare-metal event-driven framework. This header defines only
 * framework-wide primitive types, version macros, helper macros, and generic
 * status codes.
 *
 * This file is intentionally hardware-independent. It must not include MCU,
 * HAL, LL, CMSIS, RTOS, board, or application headers.
 */

#ifndef BEVH_TYPES_H
#define BEVH_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @def BEVH_VERSION_MAJOR
 * @brief Major version of the BEVH public API.
 *
 * Increment this when incompatible public API changes are introduced.
 */
#define BEVH_VERSION_MAJOR    0u

/**
 * @def BEVH_VERSION_MINOR
 * @brief Minor version of the BEVH public API.
 *
 * Increment this when backward-compatible features are added.
 */
#define BEVH_VERSION_MINOR    1u

/**
 * @def BEVH_VERSION_PATCH
 * @brief Patch version of the BEVH public API.
 *
 * Increment this for backward-compatible fixes.
 */
#define BEVH_VERSION_PATCH    0u

/**
 * @def BEVH_UNUSED
 * @brief Suppress an unused-parameter or unused-variable warning.
 *
 * @param x Parameter or variable to mark as intentionally unused.
 */
#define BEVH_UNUSED(x)        ((void)(x))

/**
 * @typedef bevh_event_id_t
 * @brief Event identifier type used by the dispatcher and event queue.
 *
 * Framework-reserved event IDs are defined by @c bevh_event.h. Application
 * event IDs must start from @c BEVH_EVENT_USER_BASE.
 */
typedef uint16_t bevh_event_id_t;

/**
 * @typedef bevh_source_id_t
 * @brief Identifier for the producer of an event.
 *
 * Source IDs are owned by the application. BEVH stores and forwards the value
 * but does not interpret it.
 */
typedef uint16_t bevh_source_id_t;

/**
 * @typedef bevh_count_t
 * @brief Small count and capacity type used by fixed-size BEVH containers.
 *
 * This type is intended for queue sizes, handler counts, timer counts, and
 * similar bounded framework collections.
 */
typedef uint16_t bevh_count_t;

/**
 * @typedef bevh_tick_t
 * @brief Generic framework tick type.
 *
 * The unit is defined by the application timebase. BEVH treats this as an
 * abstract tick count and does not assume milliseconds, microseconds, or CPU
 * cycles.
 */
typedef uint32_t bevh_tick_t;

/**
 * @typedef bevh_param_t
 * @brief Lightweight event parameter type.
 *
 * This type is used for small event payloads such as captured values, indexes,
 * flags, or application-defined codes. Larger payloads should be passed through
 * the event data pointer defined in @c bevh_event.h, with ownership documented
 * by the application.
 */
typedef uint32_t bevh_param_t;

/**
 * @enum bevh_status_t
 * @brief Generic return status for public BEVH APIs.
 *
 * Public BEVH functions return these values to make error handling explicit.
 * Project-specific error and fault meanings must not be added here; those
 * belong in application headers.
 */
typedef enum {
    /** Operation completed successfully. */
    BEVH_OK = 0,

    /** A required pointer argument was NULL. */
    BEVH_ERR_NULL,

    /** An argument value was outside the valid range. */
    BEVH_ERR_INVALID_ARG,

    /** A configuration object is incomplete or inconsistent. */
    BEVH_ERR_INVALID_CONFIG,

    /** The object was used before successful initialization. */
    BEVH_ERR_NOT_INITIALIZED,

    /** The object is already initialized and cannot be initialized again. */
    BEVH_ERR_ALREADY_INITIALIZED,

    /** A fixed-size container has no free slot. */
    BEVH_ERR_FULL,

    /** A fixed-size container has no item to read. */
    BEVH_ERR_EMPTY,

    /** The requested item, handler, timer, or entry was not found. */
    BEVH_ERR_NOT_FOUND,

    /** The requested operation cannot run because the object is busy. */
    BEVH_ERR_BUSY,

    /** A timeout occurred while waiting for a bounded operation. */
    BEVH_ERR_TIMEOUT,

    /** A counter, index, or arithmetic operation overflowed. */
    BEVH_ERR_OVERFLOW,

    /** The requested feature or operation is not supported by this build. */
    BEVH_ERR_UNSUPPORTED,

    /** An internal framework invariant failed. */
    BEVH_ERR_INTERNAL
} bevh_status_t;

#ifdef __cplusplus
}
#endif

#endif /* BEVH_TYPES_H */
