/* ARMv6-M / Cortex-M0+ layout regression; compile-only C11 test. */
#include <stddef.h>
#include <stdint.h>
#include "bevh.h"
#if UINTPTR_MAX != UINT32_MAX
#error "Must compile with ARMv6-M 32-bit-pointer target"
#endif
_Static_assert(sizeof(bevh_event_handler_t) == 4u, "handler function pointer size");
_Static_assert(sizeof(bevh_event_t) == 16u, "bevh_event_t size");
_Static_assert(sizeof(bevh_handler_entry_t) == 12u, "bevh_handler_entry_t size");
_Static_assert(offsetof(bevh_handler_entry_t, handler) == 0u, "bevh_handler_entry_t.handler offset");
_Static_assert(offsetof(bevh_handler_entry_t, user) == 4u, "bevh_handler_entry_t.user offset");
_Static_assert(offsetof(bevh_handler_entry_t, event_id) == 8u, "bevh_handler_entry_t.event_id offset");
_Static_assert(offsetof(bevh_handler_entry_t, used) == 10u, "bevh_handler_entry_t.used offset");
_Static_assert(sizeof(bevh_dispatcher_t) == 12u, "bevh_dispatcher_t size");
_Static_assert(sizeof(bevh_event_queue_t) == 36u, "bevh_event_queue_t size");
_Static_assert(sizeof(bevh_timer_t) == 28u, "bevh_timer_t size");
_Static_assert(offsetof(bevh_timer_t, remaining_ticks) == 0u, "bevh_timer_t.remaining_ticks offset");
_Static_assert(offsetof(bevh_timer_t, period_ticks) == 4u, "bevh_timer_t.period_ticks offset");
_Static_assert(offsetof(bevh_timer_t, param) == 8u, "bevh_timer_t.param offset");
_Static_assert(offsetof(bevh_timer_t, data) == 12u, "bevh_timer_t.data offset");
_Static_assert(offsetof(bevh_timer_t, missed_count) == 16u, "bevh_timer_t.missed_count offset");
_Static_assert(offsetof(bevh_timer_t, id) == 20u, "bevh_timer_t.id offset");
_Static_assert(offsetof(bevh_timer_t, event_id) == 22u, "bevh_timer_t.event_id offset");
_Static_assert(offsetof(bevh_timer_t, source) == 24u, "bevh_timer_t.source offset");
_Static_assert(offsetof(bevh_timer_t, periodic) == 26u, "bevh_timer_t.periodic offset");
_Static_assert(offsetof(bevh_timer_t, active) == 27u, "bevh_timer_t.active offset");
_Static_assert(sizeof(bevh_timer_config_t) == 20u, "bevh_timer_config_t size");
_Static_assert(offsetof(bevh_timer_config_t, delay_ticks) == 0u, "bevh_timer_config_t.delay_ticks offset");
_Static_assert(offsetof(bevh_timer_config_t, param) == 4u, "bevh_timer_config_t.param offset");
_Static_assert(offsetof(bevh_timer_config_t, data) == 8u, "bevh_timer_config_t.data offset");
_Static_assert(offsetof(bevh_timer_config_t, timer_id) == 12u, "bevh_timer_config_t.timer_id offset");
_Static_assert(offsetof(bevh_timer_config_t, event_id) == 14u, "bevh_timer_config_t.event_id offset");
_Static_assert(offsetof(bevh_timer_config_t, source) == 16u, "bevh_timer_config_t.source offset");
_Static_assert(offsetof(bevh_timer_config_t, periodic) == 18u, "bevh_timer_config_t.periodic offset");
_Static_assert(sizeof(bevh_timer_mgr_t) == 12u, "bevh_timer_mgr_t size");
_Static_assert(offsetof(bevh_timer_mgr_t, timers) == 0u, "bevh_timer_mgr_t.timers offset");
_Static_assert(offsetof(bevh_timer_mgr_t, queue) == 4u, "bevh_timer_mgr_t.queue offset");
_Static_assert(offsetof(bevh_timer_mgr_t, capacity) == 8u, "bevh_timer_mgr_t.capacity offset");
_Static_assert(offsetof(bevh_timer_mgr_t, initialized) == 10u, "bevh_timer_mgr_t.initialized offset");
_Static_assert(sizeof(bevh_critical_t) == 12u, "bevh_critical_t size");
_Static_assert(sizeof(bevh_t) == 64u, "bevh_t size");
_Static_assert(sizeof(bevh_config_runtime_t) == 32u, "bevh_config_runtime_t size");
_Static_assert(offsetof(bevh_config_runtime_t, event_buffer) == 0u, "bevh_config_runtime_t.event_buffer offset");
_Static_assert(offsetof(bevh_config_runtime_t, handler_buffer) == 4u, "bevh_config_runtime_t.handler_buffer offset");
_Static_assert(offsetof(bevh_config_runtime_t, timer_buffer) == 8u, "bevh_config_runtime_t.timer_buffer offset");
_Static_assert(offsetof(bevh_config_runtime_t, critical) == 12u, "bevh_config_runtime_t.critical offset");
_Static_assert(offsetof(bevh_config_runtime_t, event_capacity) == 24u, "bevh_config_runtime_t.event_capacity offset");
_Static_assert(offsetof(bevh_config_runtime_t, handler_capacity) == 26u, "bevh_config_runtime_t.handler_capacity offset");
_Static_assert(offsetof(bevh_config_runtime_t, timer_capacity) == 28u, "bevh_config_runtime_t.timer_capacity offset");
