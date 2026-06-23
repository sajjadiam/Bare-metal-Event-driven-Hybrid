# BEVH Framework Specification

## Project Name

`bevh` — Bare-metal Event-driven Hybrid framework

## Goal

Build a small, portable C framework for bare-metal embedded projects. The framework must provide a reusable event-driven execution core that can be used on any MCU and any bare-metal project without depending on STM32, HAL, LL, CMSIS, FreeRTOS, or any project-specific peripheral.

This framework is not an RTOS. It is a cooperative event-driven framework intended to connect ISR-level events, software timers, event queues, dispatchers, and application handlers in a predictable bare-metal architecture.

## Hard Real-Time Boundary

`bevh` must not be used as the hard-real-time execution path for power-electronics timing.

The framework may observe, report, coordinate, and supervise timing-critical modules, but it must not directly perform precision operations such as:

- thyristor gate firing timing
- inverter PWM edge timing
- hardware compare scheduling
- input-capture timestamping
- ADC trigger timing
- fast overcurrent shutdown
- break-input or emergency gate-off handling

Those operations belong in dedicated hardware-driven modules and MCU port layers. They may post events into `bevh` after the critical action is already handled.

Correct pattern:

```text
hardware ISR / timer compare
    -> hard-real-time module handles the immediate action
    -> optional bevh_post_isr() for supervision, diagnostics, or state update
```

Wrong pattern:

```text
hardware ISR
    -> bevh_post_isr()
    -> main-loop dispatcher
    -> gate firing / PWM timing / emergency shutdown
```

For an induction furnace or any high-power converter, `bevh` is the cooperative coordination layer, not the precision timing engine.

## Core Design Rules

1. The framework must be pure C.
2. The framework must not include MCU-specific headers.
3. The framework must not include `main.h`.
4. The framework must not include HAL, LL, CMSIS, FreeRTOS, or any RTOS header.
5. The framework must not use dynamic memory allocation.
6. The framework must not use blocking delays.
7. The framework must not use `printf` internally.
8. The framework must support ISR-safe event posting.
9. The framework must use fixed-size buffers supplied by the user or configured statically.
10. Every public symbol must use the `bevh_` prefix.
11. The framework must not know anything about GPIO, UART, ADC, PWM, display, buttons, motor control, power control, communication protocols, or any project-specific state machine.
12. Project-specific events must start from `BEVH_EVENT_USER_BASE`.
13. The framework must be testable on a host machine where possible.
14. The framework must be usable with multiple independent `bevh_t` contexts.

## What This Framework Does

The framework provides:

- event type definitions
- fixed-size event queue
- ISR-safe event posting
- event dispatcher
- software timer manager
- critical-section abstraction
- framework context object
- optional diagnostics
- optional fault manager
- optional cooperative tasklets

## What This Framework Does Not Do

The framework must not implement:

- GPIO drivers
- UART drivers
- ADC drivers
- PWM drivers
- display drivers
- button/key processing
- encoder processing
- communication protocols
- PID controllers
- project-specific state machines
- MCU register access
- HAL/LL wrappers
- RTOS compatibility layer

External modules may use `bevh_post()` or `bevh_post_isr()` to generate events, but they must remain outside this framework.

---

# Recommended Folder Structure

```text
bevh/
|
├── include/
│   ├── bevh.h
│   ├── bevh_config.h
│   ├── bevh_types.h
│   ├── bevh_event.h
│   ├── bevh_event_queue.h
│   ├── bevh_dispatcher.h
│   ├── bevh_timer.h
│   ├── bevh_critical.h
│   └── bevh_diag.h
│
├── src/
│   ├── bevh.c
│   ├── bevh_event_queue.c
│   ├── bevh_dispatcher.c
│   ├── bevh_timer.c
│   ├── bevh_critical.c
│   └── bevh_diag.c
│
├── tests/
│   ├── test_bevh_event_queue.c
│   ├── test_bevh_dispatcher.c
│   ├── test_bevh_timer.c
│   ├── test_bevh_core.c
│   └── fake_bevh_critical.c
│
├── examples/
│   ├── minimal_main.c
│   ├── periodic_timer_example.c
│   └── isr_post_example.c
│
├── ports/
│   └── stm32/
│       ├── bevh_stm32_critical.c
│       └── bevh_stm32_systick.c
│
└── docs/
    ├── architecture.md
    └── integration_contract.md
```

## Folder Rules

- `include/` and `src/` are the portable framework.
- `tests/` must build on a host machine without MCU headers.
- `examples/` must show usage only; examples are not framework dependencies.
- `ports/` may contain MCU-specific helpers, but portable files must never include files from `ports/`.
- `docs/` must document integration contracts, timing limits, and ownership rules.
- `bevh_diag` is recommended early because event-driven systems are hard to debug without counters, but it must remain generic and optional by configuration.

## Version 2 Optional Files

Add these only after the core framework is implemented and tested:

```text
bevh/
|
├── include/
│   ├── bevh_fault.h
│   └── bevh_tasklet.h
│
└── src/
    ├── bevh_fault.c
    └── bevh_tasklet.c
```

## Version 3 Optional Files

Add these only if there is a clear need:

```text
bevh/
|
├── include/
│   ├── bevh_signal.h
│   ├── bevh_trace.h
│   └── bevh_assert.h
│
└── src/
    ├── bevh_signal.c
    ├── bevh_trace.c
    └── bevh_assert.c
```

---

# Layer 1 — `bevh_types`

## Files

```text
include/bevh_types.h
```

## Responsibility

Define generic framework-wide types and status codes.

## Required Content

Define common integer and boolean usage with standard C headers only:

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
```

Define framework version macros:

```c
#define BEVH_VERSION_MAJOR    0u
#define BEVH_VERSION_MINOR    1u
#define BEVH_VERSION_PATCH    0u
```

Define generic helper macros:

```c
#define BEVH_UNUSED(x)        ((void)(x))
```

Define framework-wide primitive types:

```c
typedef uint16_t bevh_event_id_t;
typedef uint16_t bevh_source_id_t;
typedef uint16_t bevh_timer_id_t;
typedef uint16_t bevh_count_t;
typedef uint32_t bevh_tick_t;
typedef uint32_t bevh_param_t;
```

Define status codes:

```c
typedef enum {
    BEVH_OK = 0,
    BEVH_ERR_NULL,
    BEVH_ERR_INVALID_ARG,
    BEVH_ERR_INVALID_CONFIG,
    BEVH_ERR_NOT_INITIALIZED,
    BEVH_ERR_ALREADY_INITIALIZED,
    BEVH_ERR_FULL,
    BEVH_ERR_EMPTY,
    BEVH_ERR_NOT_FOUND,
    BEVH_ERR_BUSY,
    BEVH_ERR_TIMEOUT,
    BEVH_ERR_OVERFLOW,
    BEVH_ERR_UNSUPPORTED,
    BEVH_ERR_INTERNAL
} bevh_status_t;
```

## Rules

- Do not define project-specific statuses here.
- Do not define GPIO, ADC, PWM, thyristor, inverter, communication, or application-specific types here.
- Do not define event structs here; `bevh_event.h` owns the event object.
- Do not include MCU, HAL, LL, CMSIS, RTOS, or project headers.
- Keep this header stable. Changing these typedefs changes the public ABI/API surface of the framework.

---

# Layer 2 — `bevh_config`

## Files

```text
include/bevh_config.h
```

## Responsibility

Provide compile-time configuration defaults.

## Required Rules

All settings must be override-friendly:

```c
#ifndef BEVH_EVENT_QUEUE_SIZE
#define BEVH_EVENT_QUEUE_SIZE 32u
#endif
```

## Required Configuration Macros

```c
#ifndef BEVH_EVENT_QUEUE_SIZE
#define BEVH_EVENT_QUEUE_SIZE 32u
#endif

#ifndef BEVH_MAX_HANDLERS
#define BEVH_MAX_HANDLERS 16u
#endif

#ifndef BEVH_MAX_TIMERS
#define BEVH_MAX_TIMERS 16u
#endif

#ifndef BEVH_ENABLE_DIAG
#define BEVH_ENABLE_DIAG 1u
#endif

#ifndef BEVH_ENABLE_CRITICAL
#define BEVH_ENABLE_CRITICAL 1u
#endif

#ifndef BEVH_DISPATCH_MISSING_HANDLER_IS_ERROR
#define BEVH_DISPATCH_MISSING_HANDLER_IS_ERROR 0u
#endif

#ifndef BEVH_ALLOW_NULL_CRITICAL
#define BEVH_ALLOW_NULL_CRITICAL 0u
#endif

#ifndef BEVH_TIMER_ALLOW_ZERO_DELAY
#define BEVH_TIMER_ALLOW_ZERO_DELAY 0u
#endif

#ifndef BEVH_TIMER_ENABLE_CATCHUP
#define BEVH_TIMER_ENABLE_CATCHUP 0u
#endif

#ifndef BEVH_TIMER_MAX_CATCHUP_EVENTS
#define BEVH_TIMER_MAX_CATCHUP_EVENTS 4u
#endif

#ifndef BEVH_RUN_MAX_EVENTS_PER_CALL
#define BEVH_RUN_MAX_EVENTS_PER_CALL 0u
#endif
```

No MCU-specific configuration belongs here.

`BEVH_EVENT_QUEUE_SIZE` must be a power of two because the queue implementation
uses mask-based index wrapping. `BEVH_RUN_MAX_EVENTS_PER_CALL == 0u` means
`bevh_run()` drains the queue and returns when it becomes empty; a nonzero value
bounds the amount of event work per call.

---

# Layer 3 — `bevh_event`

## Files

```text
include/bevh_event.h
```

## Responsibility

Define the generic event object and framework-reserved event IDs.

## Required Event IDs

```c
#define BEVH_EVENT_NONE       0u
#define BEVH_EVENT_TICK       1u
#define BEVH_EVENT_TIMER      2u
#define BEVH_EVENT_FAULT      3u
#define BEVH_EVENT_USER_BASE  1000u
```

## Required Event Type

```c
typedef struct {
    bevh_event_id_t id;
    bevh_source_id_t source;
    bevh_tick_t timestamp;
    bevh_param_t param;
    void *data;
} bevh_event_t;
```

## Rules

- `id` identifies the event type.
- `source` identifies the event producer.
- `timestamp` is supplied by the application or framework tick source.
- `param` is a lightweight value field.
- `data` is optional and must not imply ownership transfer unless the user explicitly documents it.
- The framework must not allocate or free `data`.

---

# Layer 4 — `bevh_critical`

## Files

```text
include/bevh_critical.h
src/bevh_critical.c
```

## Responsibility

Provide a portable critical-section abstraction for protecting data shared between ISR and main context.

## Required Design

The framework must not directly call MCU intrinsics like:

```c
__disable_irq();
__enable_irq();
```

Instead, use callbacks or user-overridable macros.

## Preferred Callback Types

```c
typedef uint32_t (*bevh_enter_critical_fn)(void *user);
typedef void (*bevh_exit_critical_fn)(uint32_t state, void *user);

typedef struct {
    bevh_enter_critical_fn enter;
    bevh_exit_critical_fn exit;
    void *user;
} bevh_critical_t;
```

## Rules

- Critical sections must be short.
- Critical sections must only protect queue head/tail/counter or similar shared data.
- Do not call event handlers inside a critical section.
- Do not run dispatcher logic inside a critical section.

---

# Layer 5 — `bevh_event_queue`

## Files

```text
include/bevh_event_queue.h
src/bevh_event_queue.c
```

## Responsibility

Implement a fixed-size FIFO event queue.

## Required Features

- user-supplied buffer or internally configured static buffer
- push from main context
- push from ISR context
- pop from main context
- clear
- check empty/full
- get count
- get capacity
- overflow counter
- maximum usage counter, if diagnostics are enabled

## Required Public Types

```c
typedef struct {
    bevh_event_t *buffer;
    bevh_count_t capacity;
    bevh_count_t mask;
    volatile bevh_count_t head;
    volatile bevh_count_t tail;
    volatile bevh_count_t count;
    volatile uint32_t overflow_count;
    bevh_count_t max_used;
    bool initialized;
    bevh_critical_t critical;
} bevh_event_queue_t;
```

## Required API

```c
bevh_status_t bevh_event_queue_init(bevh_event_queue_t *q,
                                    bevh_event_t *buffer,
                                    bevh_count_t capacity,
                                    const bevh_critical_t *critical);

bevh_status_t bevh_event_push(bevh_event_queue_t *q,
                              const bevh_event_t *event);

bevh_status_t bevh_event_push_isr(bevh_event_queue_t *q,
                                  const bevh_event_t *event);

bevh_status_t bevh_event_pop(bevh_event_queue_t *q,
                             bevh_event_t *event);

void bevh_event_queue_clear(bevh_event_queue_t *q);

bool bevh_event_queue_is_empty(const bevh_event_queue_t *q);

bool bevh_event_queue_is_full(const bevh_event_queue_t *q);

bevh_count_t bevh_event_queue_count(const bevh_event_queue_t *q);

uint32_t bevh_event_queue_overflow_count(const bevh_event_queue_t *q);
```

## Rules

- No dynamic allocation.
- No blocking.
- Queue capacity must be a power of two.
- Queue objects must be zero-initialized before the first init call.
- If queue is full, increment `overflow_count` and return `BEVH_ERR_FULL`.
- `push_isr` must be safe to call from ISR.
- `pop` is intended for main context.
- Do not dispatch events inside the queue module.

---

# Layer 6 — `bevh_dispatcher`

## Files

```text
include/bevh_dispatcher.h
src/bevh_dispatcher.c
```

## Responsibility

Route events to user-registered handlers.

## Required Handler Type

```c
typedef void (*bevh_event_handler_t)(const bevh_event_t *event, void *user);
```

## Required Internal Handler Entry

```c
typedef struct {
    bevh_event_id_t event_id;
    bevh_event_handler_t handler;
    void *user;
    bool used;
} bevh_handler_entry_t;
```

## Required Dispatcher Type

```c
typedef struct {
    bevh_handler_entry_t *handlers;
    bevh_count_t capacity;
    bevh_count_t count;
    bool initialized;
} bevh_dispatcher_t;
```

## Required API

```c
bevh_status_t bevh_dispatcher_init(bevh_dispatcher_t *d,
                                   bevh_handler_entry_t *handlers,
                                   bevh_count_t capacity);

bevh_status_t bevh_dispatcher_subscribe(bevh_dispatcher_t *d,
                                        bevh_event_id_t event_id,
                                        bevh_event_handler_t handler,
                                        void *user);

bevh_status_t bevh_dispatcher_subscribe_table(bevh_dispatcher_t *d,
                                              const bevh_handler_entry_t *table,
                                              bevh_count_t count);

bevh_status_t bevh_dispatcher_unsubscribe(bevh_dispatcher_t *d,
                                          bevh_event_id_t event_id,
                                          bevh_event_handler_t handler,
                                          void *user);

bevh_status_t bevh_dispatcher_dispatch(bevh_dispatcher_t *d,
                                       const bevh_event_t *event);

bevh_count_t bevh_dispatcher_count(const bevh_dispatcher_t *d);

bevh_count_t bevh_dispatcher_capacity(const bevh_dispatcher_t *d);
```

## Required Behavior

- Multiple handlers may subscribe to the same event ID.
- A static application handler table may be loaded into the runtime dispatcher buffer with `bevh_dispatcher_subscribe_table()`.
- Dispatch must call all matching handlers.
- If no handler exists, return `BEVH_ERR_NOT_FOUND` or `BEVH_OK` depending on a config macro. Default should be `BEVH_OK` to avoid unnecessary failures.
- Dispatcher must not contain project logic.
- Dispatcher must not block.
- Dispatcher must not modify the event.
- Dispatcher initialization owns a mutable runtime handler buffer; it must not keep a pointer to an application `const` route table.

---

# Layer 7 — `bevh_timer`

## Files

```text
include/bevh_timer.h
src/bevh_timer.c
```

## Responsibility

Implement cooperative software timers that generate events when they expire.

This is not a hardware timer driver. It must be driven by the application calling `bevh_timer_tick()` or `bevh_tick()` from a periodic interrupt or main loop timebase.

## Required Timer Type

```c
#define BEVH_TIMER_ID_NONE    ((bevh_timer_id_t)0u)
```

```c
typedef struct {
    bevh_timer_id_t id;
    bevh_tick_t remaining_ticks;
    bevh_tick_t period_ticks;
    bevh_event_id_t event_id;
    bevh_source_id_t source;
    bevh_param_t param;
    void *data;
    uint32_t missed_count;
    bool periodic;
    bool active;
} bevh_timer_t;
```

## Required Timer Config Type

```c
typedef struct {
    bevh_timer_id_t timer_id;
    bevh_tick_t delay_ticks;
    bool periodic;
    bevh_event_id_t event_id;
    bevh_source_id_t source;
    bevh_param_t param;
    void *data;
} bevh_timer_config_t;
```

## Required Timer Manager Type

```c
typedef struct {
    bevh_timer_t *timers;
    bevh_count_t capacity;
    bevh_event_queue_t *queue;
    bool initialized;
} bevh_timer_mgr_t;
```

## Required API

```c
bevh_status_t bevh_timer_mgr_init(bevh_timer_mgr_t *mgr,
                                  bevh_timer_t *timers,
                                  bevh_count_t capacity,
                                  bevh_event_queue_t *queue);

bevh_status_t bevh_timer_start(bevh_timer_mgr_t *mgr,
                               const bevh_timer_config_t *cfg);

bevh_status_t bevh_timer_stop(bevh_timer_mgr_t *mgr,
                              bevh_timer_id_t timer_id);

bevh_status_t bevh_timer_restart(bevh_timer_mgr_t *mgr,
                                 bevh_timer_id_t timer_id);

void bevh_timer_tick(bevh_timer_mgr_t *mgr,
                     bevh_tick_t elapsed_ticks);

bool bevh_timer_is_active(const bevh_timer_mgr_t *mgr,
                          bevh_timer_id_t timer_id);

bevh_tick_t bevh_timer_remaining(const bevh_timer_mgr_t *mgr,
                                 bevh_timer_id_t timer_id);

bevh_count_t bevh_timer_capacity(const bevh_timer_mgr_t *mgr);
```

## Required Behavior

- When a timer expires, it posts an event to the event queue.
- One-shot timers deactivate after expiration.
- Periodic timers reload automatically.
- Timer IDs are application-defined values. They are not array indexes.
- `BEVH_TIMER_ID_NONE` is reserved for invalid or unused timer slots.
- If catch-up is disabled, each periodic timer posts at most one event per tick call.
- If catch-up is enabled, each periodic timer posts at most `BEVH_TIMER_MAX_CATCHUP_EVENTS` events per tick call.
- Extra elapsed periods must be accumulated in `missed_count`, not posted without bound.
- If event posting fails because the queue is full, the timer manager must not block.
- Timers must not call event handlers directly.
- Software timers must not be used for hard real-time operations.
- Zero-delay timers are rejected by default. If `BEVH_TIMER_ALLOW_ZERO_DELAY` is enabled, zero-delay one-shot timers may be allowed, but zero-delay periodic timers must still be rejected to avoid an event storm.
- The timer manager has no internal critical section. Calls that modify the same manager must come from one execution context or be protected externally by the application.

## Explicit Warning for Documentation

Document this clearly:

> `bevh_timer` is suitable for UI refresh, communication timeout, retry delay, slow monitoring, and cooperative scheduling. It is not suitable for precision PWM, input capture, gate pulse timing, ADC trigger timing, or safety shutdown.

---

# Layer 8 — `bevh_core`

## Files

```text
include/bevh.h
src/bevh.c
```

## Responsibility

Provide the top-level framework context and simple API.

## Required Context Type

```c
typedef struct {
    bevh_event_queue_t queue;
    bevh_dispatcher_t dispatcher;
    bevh_timer_mgr_t timer_mgr;

    bool initialized;
} bevh_t;
```

## Required Runtime Config Type

```c
typedef struct {
    bevh_event_t *event_buffer;
    bevh_count_t event_capacity;

    bevh_handler_entry_t *handler_buffer;
    bevh_count_t handler_capacity;

    bevh_timer_t *timer_buffer;
    bevh_count_t timer_capacity;

    bevh_critical_t critical;
} bevh_config_runtime_t;
```

## Required API

```c
bevh_status_t bevh_init(bevh_t *ctx,
                        const bevh_config_runtime_t *cfg);

bevh_status_t bevh_post(bevh_t *ctx,
                        const bevh_event_t *event);

bevh_status_t bevh_post_isr(bevh_t *ctx,
                            const bevh_event_t *event);

bevh_status_t bevh_subscribe(bevh_t *ctx,
                             bevh_event_id_t event_id,
                             bevh_event_handler_t handler,
                             void *user);

bevh_status_t bevh_unsubscribe(bevh_t *ctx,
                               bevh_event_id_t event_id,
                               bevh_event_handler_t handler,
                               void *user);

bevh_status_t bevh_subscribe_table(bevh_t *ctx,
                                   const bevh_handler_entry_t *table,
                                   bevh_count_t count);

bevh_status_t bevh_start_timer(bevh_t *ctx,
                               const bevh_timer_config_t *cfg);

bevh_status_t bevh_stop_timer(bevh_t *ctx,
                              bevh_timer_id_t timer_id);

bevh_status_t bevh_restart_timer(bevh_t *ctx,
                                 bevh_timer_id_t timer_id);

void bevh_tick(bevh_t *ctx,
               bevh_tick_t elapsed_ticks);

bevh_status_t bevh_run_once(bevh_t *ctx);

bevh_status_t bevh_run(bevh_t *ctx,
                       bevh_count_t *processed_count);
```

## Required Behavior

`bevh_run_once()` must:

1. Pop at most one event from the queue.
2. Dispatch it if one was available.
3. Return the actual processing status.

`bevh_run()` may:

1. Pop and dispatch until the queue is empty when `BEVH_RUN_MAX_EVENTS_PER_CALL == 0u`.
2. Process at most `BEVH_RUN_MAX_EVENTS_PER_CALL` events when the limit is nonzero.
3. Optionally report the number of successfully dispatched events through `processed_count`.
4. Return `BEVH_OK` when the queue becomes empty or the configured budget is reached.

Do not implement an infinite loop inside `bevh_run()`. The application owns the main loop.

---

# Optional Layer — `bevh_diag`

## Files

```text
include/bevh_diag.h
src/bevh_diag.c
```

## Responsibility

Expose framework diagnostic counters.

## Suggested Diagnostics

```c
typedef struct {
    uint32_t posted_count;
    uint32_t posted_isr_count;
    uint32_t dispatched_count;
    uint32_t dropped_count;
    uint32_t queue_overflow_count;
    bevh_count_t queue_max_used;
    uint32_t timer_expired_count;
    uint32_t handler_not_found_count;
} bevh_diag_t;
```

Diagnostics are essential for debugging event-driven systems. Add this after the base framework works.

---

# Optional Layer — `bevh_fault`

## Files

```text
include/bevh_fault.h
src/bevh_fault.c
```

## Responsibility

Provide generic fault latching and clearing.

## Rules

- The framework must not know the meaning of fault IDs.
- The project defines fault IDs.
- The fault manager only stores active state, counters, timestamp, source, and parameter.
- Raising a fault may optionally post `BEVH_EVENT_FAULT`.

## Suggested API

```c
bevh_status_t bevh_fault_raise(bevh_fault_mgr_t *mgr,
                               uint16_t fault_id,
                               bevh_source_id_t source,
                               bevh_param_t param);

bevh_status_t bevh_fault_clear(bevh_fault_mgr_t *mgr,
                               uint16_t fault_id);

bool bevh_fault_is_active(const bevh_fault_mgr_t *mgr,
                          uint16_t fault_id);
```

---

# Optional Layer — `bevh_tasklet`

## Files

```text
include/bevh_tasklet.h
src/bevh_tasklet.c
```

## Responsibility

Provide cooperative background callbacks.

## Rules

- Tasklets are not RTOS tasks.
- Tasklets must not block.
- Tasklets must be short.
- Tasklets are for background maintenance, diagnostics, and slow polling.

## Suggested API

```c
typedef void (*bevh_tasklet_fn)(void *user);

bevh_status_t bevh_tasklet_register(bevh_tasklet_mgr_t *mgr,
                                    bevh_tasklet_fn fn,
                                    void *user,
                                    bevh_tick_t period_ticks);

void bevh_tasklet_process(bevh_tasklet_mgr_t *mgr);
```

---

# Execution Model

## Normal Event Flow

```text
ISR / callback / polling source
        ↓
bevh_post_isr() or bevh_post()
        ↓
event queue
        ↓
main loop calls bevh_run_once() or bevh_run()
        ↓
dispatcher
        ↓
user handler
```

## Main Loop Pattern

```c
int main(void)
{
    hardware_init();
    app_init();

    while (1) {
        bevh_run(&g_bevh, NULL);
        app_background();
    }
}
```

## Tick Pattern

```c
void SysTick_Handler(void)
{
    bevh_tick(&g_bevh, 1u);
}
```

## ISR Event Posting Pattern

```c
void SOME_IRQ_Handler(void)
{
    bevh_event_t ev = {
        .id = APP_EVT_SOMETHING,
        .source = APP_SOURCE_IRQ,
        .timestamp = app_get_time(),
        .param = captured_value,
        .data = NULL
    };

    bevh_post_isr(&g_bevh, &ev);
}
```

---

# Example Project Usage Contract

Each project using `bevh` must:

1. Create a `bevh_t` context.
2. Provide event buffer, handler buffer, and timer buffer.
3. Initialize the framework using `bevh_init()`.
4. Define project event IDs starting from `BEVH_EVENT_USER_BASE`.
5. Register event handlers using `bevh_subscribe()`.
6. Post events from ISR or main context.
7. Call `bevh_tick()` from a hardware timebase if software timers are used.
8. Call `bevh_run()` or `bevh_run_once()` from the main loop and handle non-OK errors according to project policy.

## Minimal Project Event Definition

```c
typedef enum {
    APP_EVT_BUTTON = BEVH_EVENT_USER_BASE,
    APP_EVT_TIMEOUT,
    APP_EVT_SENSOR_READY,
    APP_EVT_COMMAND_RECEIVED
} app_event_id_t;
```

---

# Generic Project Integration Structure

Any project using `bevh` should keep the framework separate from application logic and hardware ports.

Recommended project-level structure:

```text
project/
|
├── third_party/
│   └── bevh/
│       ├── include/
│       └── src/
│
├── app/
│   ├── app_events.h
│   ├── app_sources.h
│   ├── app_faults.h
│   ├── app_init.c
│   ├── app_state.c
│   └── app_handlers.c
│
├── modules/
│   ├── module_a/
│   └── module_b/
│
├── board/
│   └── target_mcu/
│       ├── board_init.c
│       ├── board_timebase.c
│       └── board_interrupts.c
│
└── tests/
    ├── host/
    └── integration/
```

## Generic Integration Rules

- `bevh/` must not include `app/`, `modules/`, or `board/` headers.
- `app/` owns event IDs, source IDs, fault IDs, and high-level state machines.
- `modules/` may post events, but they must not depend on unrelated modules through `bevh`.
- `board/` owns MCU startup, interrupt vectors, hardware critical sections, and timebase configuration.
- Hardware ISRs may post events, but only after immediate hardware responsibilities are complete.
- Application handlers must be short and non-blocking.
- Long operations must be split into state-machine steps or tasklets.

---

# Induction Furnace Integration Structure

For this induction furnace project, `bevh` should be used as the cooperative coordination layer only.

Recommended firmware structure:

```text
firmware/
|
├── bevh/
│   ├── include/
│   └── src/
│
├── app/
│   ├── app_events.h
│   ├── app_sources.h
│   ├── app_faults.h
│   ├── app_supervisor.c
│   ├── app_command.c
│   └── app_diag.c
│
├── power/
│   ├── rectifier/
│   │   ├── zcd_capture.c
│   │   ├── thy_zcd_sync_adapter.c
│   │   ├── thy_channel_controller.c
│   │   ├── thy_fire_scheduler.c
│   │   ├── thy_gate_pulser.c
│   │   └── thy_3ph_semiconv_controller.c
│   │
│   └── inverter/
│       ├── inv_control.c
│       ├── inv_fault.c
│       └── inv_timing.c
│
├── board/
│   └── stm32g474/
│       ├── board_init.c
│       ├── board_timebase.c
│       ├── board_fault_inputs.c
│       ├── board_rectifier_port.c
│       └── board_inverter_port.c
│
└── tests/
    ├── bevh/
    ├── rectifier/
    └── host_fakes/
```

## Induction Furnace Rules

- Rectifier firing timing must stay in `thy_fire_scheduler` plus hardware timer port code, not in `bevh`.
- Gate pulse generation must stay non-blocking and timer-driven, not dispatcher-driven.
- Inverter PWM and safety shutdown must stay hardware-driven.
- `bevh` may receive events such as command received, soft timeout, fault latched, measurement ready, state changed, or diagnostics requested.
- `bevh` must not be the only path that turns off dangerous outputs. Fast shutdown must be handled by hardware or immediate port-layer code first.
- Fault events posted into `bevh` are for reporting and supervision; they are not a substitute for hardware protection.

---

# Coding Style Requirements

1. Use clear C99-compatible code.
2. Use `static` for internal functions.
3. Validate all public pointer arguments.
4. Return `BEVH_ERR_NULL` for NULL pointer arguments.
5. Return `BEVH_ERR_NOT_INITIALIZED` when an object is used before init.
6. Avoid hidden global state.
7. Prefer user-supplied buffers.
8. Keep ISR-safe functions short.
9. Do not call user handlers from ISR-safe APIs.
10. Do not use recursion.
11. Do not use heap allocation.
12. Do not use blocking waits.
13. Do not silently ignore queue overflow; count it.
14. Document every public function.
15. Keep module responsibilities narrow.

---

# Implementation Order

Implement in this exact order:

1. `bevh_types.h`
2. `bevh_config.h`
3. `bevh_event.h`
4. `bevh_critical.h/.c`
5. `bevh_event_queue.h/.c`
6. `bevh_dispatcher.h/.c`
7. `bevh_timer.h/.c`
8. `bevh.h/.c`
9. `bevh_diag.h/.c` as a generic optional module controlled by configuration
10. `tests/test_bevh_event_queue.c`
11. `tests/test_bevh_dispatcher.c`
12. `tests/test_bevh_timer.c`
13. `tests/test_bevh_core.c`
14. `examples/minimal_main.c`
15. `examples/periodic_timer_example.c`
16. `examples/isr_post_example.c`
17. Optional MCU ports under `ports/`
18. Optional: `bevh_fault.h/.c`
19. Optional: `bevh_tasklet.h/.c`

Do not start with optional modules before the base framework is complete and tested.

---

# Initial Acceptance Tests

Codex should create simple test cases or examples proving:

## Event Queue

- init rejects NULL arguments
- push/pop preserves FIFO order
- push fails when queue is full
- overflow counter increments when full
- pop fails when queue is empty
- clear resets queue state

## Dispatcher

- subscribe rejects NULL handler
- dispatch calls correct handler
- multiple handlers for same event are called
- unrelated handlers are not called

## Timer

- one-shot timer posts event once
- periodic timer posts repeatedly
- timer IDs are matched by ID, not by array index
- stopped timer does not post event
- timer expiration posts to queue, not directly to handler
- catch-up disabled posts at most one event per periodic timer per tick call
- catch-up enabled is bounded by `BEVH_TIMER_MAX_CATCHUP_EVENTS`

## Core

- `bevh_init()` initializes queue, dispatcher, and timer manager
- `bevh_post()` places an event in the queue
- `bevh_run_once()` dispatches one event only and returns status
- `bevh_run()` drains the queue or respects `BEVH_RUN_MAX_EVENTS_PER_CALL`
- `bevh_run()` reports processed event count when requested

---

# Final Architecture Rule

The purpose of `bevh` is only this:

```text
Move events safely and predictably between ISR, main loop, software timers, and application handlers without using an RTOS.
```

It must not become a hidden RTOS, hardware abstraction layer, protocol stack, power-control module, or hard-real-time timing engine.

Keep the framework small. Keep it portable. Keep it strict.
