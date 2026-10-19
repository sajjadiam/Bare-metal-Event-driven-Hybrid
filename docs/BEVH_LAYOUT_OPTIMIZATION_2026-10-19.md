# BEVH ARMv6-M layout optimization (2026-10-19)

BEVH version **0.2.0** changes the *order* of members of five public structs, preserving each named member, member type, and functional behavior. Users of positional aggregate initializers or precompiled applications **must update/recompile**. Array lengths and callback APIs are unchanged; no packed/unaligned access is introduced.

| Type | Before (32-bit ABI) | After |
|---|---:|---:|
| bevh_event_t | 16 | 16 |
| bevh_handler_entry_t | 16 | 12 |
| bevh_dispatcher_t | 12 | 12 |
| bevh_event_queue_t | 36 | 36 |
| bevh_timer_t | 32 | 28 |
| bevh_timer_config_t | 24 | 20 |
| bevh_timer_mgr_t | 16 | 12 |
| bevh_critical_t | 12 | 12 |
| bevh_t | 68 | 64 |
| bevh_config_runtime_t | 36 | 32 |

The five explicitly changed structs are `bevh_handler_entry_t`, `bevh_timer_t`, `bevh_timer_config_t`, `bevh_timer_mgr_t`, `bevh_config_runtime_t`. The containing `bevh_t` becomes smaller automatically.

For application-allocated storage with defaults of 32 events, 16 handlers, 16 timers: `sizeof(bevh_t) + 32*sizeof(bevh_event_t) + 16*sizeof(bevh_handler_entry_t) + 16*sizeof(bevh_timer_t)` drops **1348 to 1216 bytes (132 bytes, about 9.8%)** on ARMv6-M. Excludes temporary config object and application stack. The host benchmark's displayed total sums nested objects twice and should **not** be mistaken for this actual application-allocated storage model.

The original `tests/bevh_host_test.c` positional subscription initializer is converted to a **designated initializer**, preserving intended handler/event mapping. Regression `tests/bevh_layout_arm.c` statically checks `sizeof` and `offsetof` of all affected fields, compiled by GitHub CI using Clang `-target armv6m-none-eabi -mthumb -std=c11`. The library implementation remains C99 compatible; C11 is used only in this ABI regression test.

**Compatibility:** Other code may contain positional `bevh_handler_entry_t` tables; update them before integrating this version. Validate the same assumptions with Arm Compiler 6 / Keil when STM32G0B1 target is built. These changes do not integrate or enable the library in Motor Driver firmware.