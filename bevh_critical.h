/**
 * @file bevh_critical.h
 * @brief Portable critical-section abstraction for BEVH.
 *
 * BEVH uses critical sections to protect short shared-data operations, mainly
 * event queue head, tail, and count updates shared between ISR and main context.
 *
 * This header defines only the portable callback contract. It must not call MCU
 * intrinsics such as __disable_irq() or __enable_irq(), and it must not include
 * MCU, HAL, LL, CMSIS, RTOS, board, or application headers.
 */

#ifndef BEVH_CRITICAL_H
#define BEVH_CRITICAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bevh_types.h"

/**
 * @typedef bevh_critical_state_t
 * @brief Opaque saved critical-section state.
 *
 * The meaning of this value is defined by the port layer. On some MCUs it may
 * store interrupt mask state; on host tests it may be unused.
 */
typedef uint32_t bevh_critical_state_t;

/**
 * @typedef bevh_enter_critical_fn
 * @brief Enter a critical section and return the previous protection state.
 *
 * Implementations must be short, deterministic, and safe to call from the
 * contexts where BEVH queue operations are used.
 *
 * @param user User pointer supplied in @ref bevh_critical_t.
 *
 * @return Saved protection state to pass back to @ref bevh_exit_critical_fn.
 */
typedef bevh_critical_state_t (*bevh_enter_critical_fn)(void *user);

/**
 * @typedef bevh_exit_critical_fn
 * @brief Exit a critical section and restore the previous protection state.
 *
 * @param state State returned by the matching enter callback.
 * @param user User pointer supplied in @ref bevh_critical_t.
 */
typedef void (*bevh_exit_critical_fn)(bevh_critical_state_t state, void *user);

/**
 * @struct bevh_critical_t
 * @brief Critical-section callback table.
 *
 * BEVH copies or stores this object where needed, depending on the module. The
 * pointed-to callbacks must remain valid for as long as the owning BEVH object
 * is used.
 */
typedef struct {
    /**
     * @brief Callback used to enter a critical section.
     *
     * If critical sections are enabled for a module, this callback must not be
     * NULL unless explicitly allowed by configuration.
     */
    bevh_enter_critical_fn enter;

    /**
     * @brief Callback used to exit a critical section.
     *
     * If critical sections are enabled for a module, this callback must not be
     * NULL unless explicitly allowed by configuration.
     */
    bevh_exit_critical_fn exit;

    /**
     * @brief Optional user pointer passed to the enter and exit callbacks.
     */
    void *user;
} bevh_critical_t;

/**
 * @brief Return whether a critical-section callback table is complete.
 *
 * @param critical Pointer to a critical-section callback table.
 *
 * @retval true @p critical is not NULL and both callbacks are present.
 * @retval false @p critical is NULL or at least one callback is missing.
 */
static inline bool bevh_critical_is_valid(const bevh_critical_t *critical) {
    return (critical != NULL) && (critical->enter != NULL) && (critical->exit != NULL);
}

/**
 * @brief Enter a configured critical section.
 *
 * This helper assumes @p critical is valid. Public modules must validate their
 * configuration before calling it.
 *
 * @param critical Valid critical-section callback table.
 *
 * @return Saved protection state returned by the port enter callback.
 */
static inline bevh_critical_state_t bevh_critical_enter(const bevh_critical_t *critical) {
    return critical->enter(critical->user);
}

/**
 * @brief Exit a configured critical section.
 *
 * This helper assumes @p critical is valid. Public modules must validate their
 * configuration before calling it.
 *
 * @param critical Valid critical-section callback table.
 * @param state Saved protection state returned by @ref bevh_critical_enter.
 */
static inline void bevh_critical_exit(const bevh_critical_t *critical, bevh_critical_state_t state) {
    critical->exit(state, critical->user);
}

#ifdef __cplusplus
}
#endif

#endif /* BEVH_CRITICAL_H */
