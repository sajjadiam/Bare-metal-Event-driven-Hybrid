# BEVH bounded-dispatch regression fix — 2026-10-19

This repository's original `bevh_run()` implementation could block a cooperative main loop under sustained/reposted events when `BEVH_RUN_MAX_EVENTS_PER_CALL=0` (queue-draining mode). Merely choosing a non-zero dispatch budget previously exposed a second, deterministic infinite loop: after an empty `bevh_event_pop()`, `BEVH_ERR_EMPTY` was converted back to `BEVH_OK` without breaking or incrementing the processed counter.

The following source revisions were reconciled directly with owner-maintained `sajjadiam/induction-furnace-firmware`, tree commit `2d2e8f5d3d3281efb4e0154136ce48841831421b`:

1. `bevh_config.h`: set default `BEVH_RUN_MAX_EVENTS_PER_CALL=4u` (still overrideable at compile time), preventing unbounded background work.
2. `bevh.c`: explicitly break when the queue is empty or an error occurs during bounded dispatch; preserve `BEVH_OK` as the normal empty-queue return. Fix originally made in furnace commit `f982b9b713e166909b38d2ecbb30254dbc70269b`.
3. `bevh_event_queue.c`: saturate overflow counter at `UINT32_MAX` instead of wrapping to zero.
4. `tests/bevh_host_test.c`: include regression coverage for empty bounded dispatch, exact maximum dispatch budget/FIFO, and overflow saturation; existing CI builds with default and timer catch-up modes.

**Safety boundary:** a bounded number of events does not bound the total runtime of potentially blocking or recursive handler callbacks; callers must also ensure subscribers are non-blocking. A build configured with `BEVH_RUN_MAX_EVENTS_PER_CALL=0` still uses unbounded drain semantics by explicit opt-in. This library is **not** a hard real-time scheduler and should not own PWM, safety shutdown, ADC capture or protection deadlines.

The source changes below were taken from the furnace project and have not introduced STM32-specific HAL code. Target-side timing and CPU-starvation tests must be performed by integrating applications separately. CI status must be checked from actual GitHub workflow execution and is not guaranteed by this documentation alone.
