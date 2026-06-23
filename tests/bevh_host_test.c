#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "bevh.h"

#define TEST_QUEUE_CAPACITY      128u
#define TEST_HANDLER_CAPACITY    16u
#define TEST_TIMER_CAPACITY      16u
#define TEST_BENCH_ITERS         2000000u

enum {
    TEST_EVT_A = BEVH_EVENT_USER_BASE,
    TEST_EVT_B,
    TEST_EVT_TIMER
};

typedef struct {
    uint32_t enter_count;
    uint32_t exit_count;
} test_critical_stats_t;

typedef struct {
    uint32_t calls_a;
    uint32_t calls_b;
    uint32_t last_param;
} test_handler_stats_t;

static volatile uint32_t g_bench_sink;

static bevh_critical_state_t test_critical_enter(void *user) {
    test_critical_stats_t *stats = (test_critical_stats_t *)user;

    if(stats != NULL) {
        stats->enter_count++;
    }

    return 0x55u;
}

static void test_critical_exit(bevh_critical_state_t state, void *user) {
    test_critical_stats_t *stats = (test_critical_stats_t *)user;

    if(stats != NULL) {
        stats->exit_count++;
    }

    (void)state;
}

static void test_expect(int condition, const char *message) {
    if(!condition) {
        printf("FAIL: %s\n", message);
        exit(1);
    }
}

static bevh_event_t test_make_event(bevh_event_id_t id, bevh_param_t param) {
    bevh_event_t event;

    event.id = id;
    event.source = 1u;
    event.timestamp = 123u;
    event.param = param;
    event.data = NULL;

    return event;
}

static void test_handler_a(const bevh_event_t *event, void *user) {
    test_handler_stats_t *stats = (test_handler_stats_t *)user;

    stats->calls_a++;
    stats->last_param = event->param;
}

static void test_handler_b(const bevh_event_t *event, void *user) {
    test_handler_stats_t *stats = (test_handler_stats_t *)user;

    stats->calls_b++;
    stats->last_param = event->param;
}

static bevh_critical_t test_make_critical(test_critical_stats_t *stats) {
    bevh_critical_t critical;

    critical.enter = test_critical_enter;
    critical.exit = test_critical_exit;
    critical.user = stats;

    return critical;
}

static void test_queue(void) {
    bevh_event_queue_t queue = {0};
    bevh_event_t buffer[4];
    bevh_event_t event;
    bevh_event_t popped;
    test_critical_stats_t critical_stats = {0};
    bevh_critical_t critical = test_make_critical(&critical_stats);

    test_expect(bevh_event_queue_init(&queue, buffer, 3u, &critical) == BEVH_ERR_INVALID_ARG,
                "queue rejects non-power-of-two capacity");
    test_expect(bevh_event_queue_init(&queue, buffer, 4u, &critical) == BEVH_OK,
                "queue init succeeds");

    for(bevh_param_t i = 0u; i < 4u; i++) {
        event = test_make_event(TEST_EVT_A, i);
        test_expect(bevh_event_push(&queue, &event) == BEVH_OK, "queue push succeeds");
    }

    event = test_make_event(TEST_EVT_A, 99u);
    test_expect(bevh_event_push(&queue, &event) == BEVH_ERR_FULL, "queue reports full");
    test_expect(bevh_event_queue_overflow_count(&queue) == 1u, "queue overflow counter increments");
    test_expect(bevh_event_queue_max_used(&queue) == 4u, "queue max used reaches capacity");

    for(bevh_param_t i = 0u; i < 4u; i++) {
        test_expect(bevh_event_pop(&queue, &popped) == BEVH_OK, "queue pop succeeds");
        test_expect(popped.param == i, "queue preserves FIFO order");
    }

    test_expect(bevh_event_pop(&queue, &popped) == BEVH_ERR_EMPTY, "queue reports empty");
    test_expect(critical_stats.enter_count == critical_stats.exit_count,
                "queue critical enter/exit count is balanced");

    event = test_make_event(TEST_EVT_A, 1u);
    test_expect(bevh_event_push_isr(&queue, &event) == BEVH_OK, "ISR push succeeds");
    bevh_event_queue_clear(&queue);
    test_expect(bevh_event_queue_count(&queue) == 0u, "queue clear resets count");
    test_expect(bevh_event_queue_overflow_count(&queue) == 1u,
                "queue clear preserves overflow diagnostics");
}

static void test_dispatcher(void) {
    bevh_dispatcher_t dispatcher = {0};
    bevh_handler_entry_t handlers[4];
    test_handler_stats_t stats = {0};
    bevh_event_t event = test_make_event(TEST_EVT_A, 42u);

    test_expect(bevh_dispatcher_init(&dispatcher, handlers, 4u) == BEVH_OK,
                "dispatcher init succeeds");
    test_expect(bevh_dispatcher_subscribe(&dispatcher, TEST_EVT_A, test_handler_a, &stats) == BEVH_OK,
                "dispatcher subscribe A succeeds");
    test_expect(bevh_dispatcher_subscribe(&dispatcher, TEST_EVT_A, test_handler_a, &stats) == BEVH_ERR_BUSY,
                "dispatcher rejects duplicate subscription");
    test_expect(bevh_dispatcher_subscribe(&dispatcher, TEST_EVT_A, test_handler_b, &stats) == BEVH_OK,
                "dispatcher allows second handler for same event");

    test_expect(bevh_dispatcher_dispatch(&dispatcher, &event) == BEVH_OK, "dispatcher dispatch succeeds");
    test_expect((stats.calls_a == 1u) && (stats.calls_b == 1u), "dispatcher calls all matching handlers");

    test_expect(bevh_dispatcher_unsubscribe(&dispatcher, TEST_EVT_A, test_handler_a, &stats) == BEVH_OK,
                "dispatcher unsubscribe succeeds");
    test_expect(bevh_dispatcher_count(&dispatcher) == 1u, "dispatcher count updates after unsubscribe");
}

static void test_timer(void) {
    bevh_event_queue_t queue = {0};
    bevh_timer_mgr_t mgr = {0};
    bevh_event_t event_buffer[TEST_QUEUE_CAPACITY];
    bevh_timer_t timers[TEST_TIMER_CAPACITY];
    bevh_event_t event;
    test_critical_stats_t critical_stats = {0};
    bevh_critical_t critical = test_make_critical(&critical_stats);
    bevh_timer_config_t cfg;

    test_expect(bevh_event_queue_init(&queue, event_buffer, TEST_QUEUE_CAPACITY, &critical) == BEVH_OK,
                "timer test queue init succeeds");
    test_expect(bevh_timer_mgr_init(&mgr, timers, TEST_TIMER_CAPACITY, &queue) == BEVH_OK,
                "timer manager init succeeds");

    cfg.timer_id = 1u;
    cfg.delay_ticks = 3u;
    cfg.periodic = false;
    cfg.event_id = TEST_EVT_TIMER;
    cfg.source = 2u;
    cfg.param = 33u;
    cfg.data = NULL;
    test_expect(bevh_timer_start(&mgr, &cfg) == BEVH_OK, "one-shot timer starts");
    bevh_timer_tick(&mgr, 2u);
    test_expect(bevh_event_queue_count(&queue) == 0u, "one-shot does not expire early");
    bevh_timer_tick(&mgr, 1u);
    test_expect(bevh_event_pop(&queue, &event) == BEVH_OK, "one-shot posts event");
    test_expect((event.id == TEST_EVT_TIMER) && (event.param == 33u), "one-shot event fields match");
    test_expect(!bevh_timer_is_active(&mgr, 1u), "one-shot deactivates after expiration");

    cfg.timer_id = 2u;
    cfg.delay_ticks = 5u;
    cfg.periodic = true;
    cfg.param = 44u;
    test_expect(bevh_timer_start(&mgr, &cfg) == BEVH_OK, "periodic timer starts");
    bevh_timer_tick(&mgr, 16u);

#if BEVH_TIMER_ENABLE_CATCHUP
    test_expect(bevh_event_queue_count(&queue) == 3u, "catch-up posts all elapsed periods within limit");
    test_expect(timers[1].missed_count == 0u, "catch-up does not miss within limit");
#else
    test_expect(bevh_event_queue_count(&queue) == 1u, "no catch-up posts one event per tick call");
    test_expect(timers[1].missed_count == 2u, "no catch-up coalesces extra elapsed periods");
#endif

    test_expect(bevh_timer_remaining(&mgr, 2u) == 4u, "periodic remaining time is preserved");
    test_expect(bevh_timer_stop(&mgr, 2u) == BEVH_OK, "timer stop succeeds");
    test_expect(!bevh_timer_is_active(&mgr, 2u), "timer stop deactivates timer");

    cfg.timer_id = BEVH_TIMER_ID_NONE;
    test_expect(bevh_timer_start(&mgr, &cfg) == BEVH_ERR_INVALID_ARG,
                "timer rejects reserved timer ID");
}

static void test_core(void) {
    bevh_t ctx = {0};
    bevh_event_t event_buffer[TEST_QUEUE_CAPACITY];
    bevh_handler_entry_t handler_buffer[TEST_HANDLER_CAPACITY];
    bevh_timer_t timer_buffer[TEST_TIMER_CAPACITY];
    test_critical_stats_t critical_stats = {0};
    test_handler_stats_t stats = {0};
    bevh_critical_t critical = test_make_critical(&critical_stats);
    bevh_config_runtime_t cfg;
    bevh_count_t processed = 0u;
    bevh_event_t event = test_make_event(TEST_EVT_A, 77u);
    const bevh_handler_entry_t routes[] = {
        {TEST_EVT_A, test_handler_a, &stats, true},
        {TEST_EVT_B, test_handler_b, &stats, true}
    };

    cfg.event_buffer = event_buffer;
    cfg.event_capacity = TEST_QUEUE_CAPACITY;
    cfg.handler_buffer = handler_buffer;
    cfg.handler_capacity = TEST_HANDLER_CAPACITY;
    cfg.timer_buffer = timer_buffer;
    cfg.timer_capacity = TEST_TIMER_CAPACITY;
    cfg.critical = critical;

    test_expect(bevh_init(&ctx, &cfg) == BEVH_OK, "core init succeeds");
    test_expect(bevh_subscribe_table(&ctx, routes, (bevh_count_t)(sizeof(routes) / sizeof(routes[0]))) == BEVH_OK,
                "core subscribe table succeeds");
    test_expect(bevh_post(&ctx, &event) == BEVH_OK, "core post succeeds");
    test_expect(bevh_run(&ctx, &processed) == BEVH_OK, "core run succeeds");
    test_expect(processed == 1u, "core run reports processed count");
    test_expect((stats.calls_a == 1u) && (stats.last_param == 77u), "core dispatches posted event");
    test_expect(bevh_run_once(&ctx) == BEVH_ERR_EMPTY, "core run_once reports empty queue");
}

static double test_seconds_since(clock_t start, clock_t end) {
    return ((double)(end - start)) / (double)CLOCKS_PER_SEC;
}

static double test_time_per_call_ns(clock_t start, clock_t end, uint32_t calls) {
    return (test_seconds_since(start, end) * 1000000000.0) / (double)calls;
}

static void bench_report(const char *name, clock_t start, clock_t end, uint32_t calls) {
    printf("BENCH %-34s %10.2f ns/call (%lu calls)\n",
           name,
           test_time_per_call_ns(start, end, calls),
           (unsigned long)calls);
}

static void bench_functions(void) {
    bevh_event_queue_t queue = {0};
    bevh_dispatcher_t dispatcher = {0};
    bevh_timer_mgr_t timer_mgr = {0};
    bevh_t ctx = {0};
    bevh_event_t event_buffer[TEST_QUEUE_CAPACITY];
    bevh_handler_entry_t handler_buffer[TEST_HANDLER_CAPACITY];
    bevh_timer_t timer_buffer[TEST_TIMER_CAPACITY];
    test_critical_stats_t critical_stats = {0};
    test_handler_stats_t handler_stats = {0};
    bevh_critical_t critical = test_make_critical(&critical_stats);
    bevh_config_runtime_t core_cfg;
    bevh_event_t event = test_make_event(TEST_EVT_A, 1u);
    bevh_timer_config_t timer_cfg;
    test_handler_stats_t dispatch_stats[TEST_HANDLER_CAPACITY];
    uint32_t status_accumulator = 0u;
    clock_t start;
    clock_t end;

    test_expect(bevh_event_queue_init(&queue, event_buffer, TEST_QUEUE_CAPACITY, &critical) == BEVH_OK,
                "bench queue init succeeds");
    test_expect(bevh_dispatcher_init(&dispatcher, handler_buffer, TEST_HANDLER_CAPACITY) == BEVH_OK,
                "bench dispatcher init succeeds");

    for(bevh_count_t i = 0u; i < TEST_HANDLER_CAPACITY; i++) {
        dispatch_stats[i].calls_a = 0u;
        dispatch_stats[i].calls_b = 0u;
        dispatch_stats[i].last_param = 0u;
        test_expect(bevh_dispatcher_subscribe(&dispatcher,
                                              TEST_EVT_A,
                                              test_handler_a,
                                              &dispatch_stats[i]) == BEVH_OK,
                    "bench dispatcher setup");
    }

    start = clock();
    for(uint32_t i = 0u; i < TEST_BENCH_ITERS; i++) {
        status_accumulator += (uint32_t)bevh_event_push(&queue, &event);
        status_accumulator += (uint32_t)bevh_event_pop(&queue, &event);
    }
    end = clock();
    test_expect(status_accumulator == 0u, "bench queue status");
    bench_report("queue push+pop pair", start, end, TEST_BENCH_ITERS);

    status_accumulator = 0u;
    start = clock();
    for(uint32_t i = 0u; i < TEST_BENCH_ITERS; i++) {
        status_accumulator += (uint32_t)bevh_dispatcher_dispatch(&dispatcher, &event);
    }
    end = clock();
    test_expect(status_accumulator == 0u, "bench dispatcher status");
    g_bench_sink += dispatch_stats[0].calls_a;
    bench_report("dispatch 16 matching handlers", start, end, TEST_BENCH_ITERS);

    test_expect(bevh_timer_mgr_init(&timer_mgr, timer_buffer, TEST_TIMER_CAPACITY, &queue) == BEVH_OK,
                "bench timer manager init succeeds");
    timer_cfg.delay_ticks = 1000000u;
    timer_cfg.periodic = true;
    timer_cfg.event_id = TEST_EVT_TIMER;
    timer_cfg.source = 3u;
    timer_cfg.param = 4u;
    timer_cfg.data = NULL;
    for(bevh_count_t i = 0u; i < TEST_TIMER_CAPACITY; i++) {
        timer_cfg.timer_id = (bevh_timer_id_t)(i + 1u);
        test_expect(bevh_timer_start(&timer_mgr, &timer_cfg) == BEVH_OK, "bench timer start");
    }

    start = clock();
    for(uint32_t i = 0u; i < TEST_BENCH_ITERS; i++) {
        bevh_timer_tick(&timer_mgr, 1u);
    }
    end = clock();
    g_bench_sink += (uint32_t)bevh_timer_remaining(&timer_mgr, 1u);
    bench_report("timer_tick scan 16 no expiry", start, end, TEST_BENCH_ITERS);

    for(bevh_count_t i = 0u; i < TEST_TIMER_CAPACITY; i++) {
        timer_cfg.timer_id = (bevh_timer_id_t)(i + 1u);
        timer_cfg.delay_ticks = 1u;
        timer_cfg.periodic = true;
        test_expect(bevh_timer_start(&timer_mgr, &timer_cfg) == BEVH_OK, "bench timer restart short period");
    }

    start = clock();
    for(uint32_t i = 0u; i < TEST_BENCH_ITERS; i++) {
        bevh_timer_tick(&timer_mgr, 1u);
        bevh_event_queue_clear(&queue);
    }
    end = clock();
    g_bench_sink += (uint32_t)bevh_timer_remaining(&timer_mgr, 1u);
    bench_report("timer_tick 16 expiries+clear", start, end, TEST_BENCH_ITERS);

    core_cfg.event_buffer = event_buffer;
    core_cfg.event_capacity = TEST_QUEUE_CAPACITY;
    core_cfg.handler_buffer = handler_buffer;
    core_cfg.handler_capacity = TEST_HANDLER_CAPACITY;
    core_cfg.timer_buffer = timer_buffer;
    core_cfg.timer_capacity = TEST_TIMER_CAPACITY;
    core_cfg.critical = critical;

    ctx.initialized = false;
    test_expect(bevh_init(&ctx, &core_cfg) == BEVH_OK, "bench core init succeeds");
    test_expect(bevh_subscribe(&ctx, TEST_EVT_A, test_handler_a, &handler_stats) == BEVH_OK,
                "bench core subscribe succeeds");

    status_accumulator = 0u;
    start = clock();
    for(uint32_t i = 0u; i < TEST_BENCH_ITERS; i++) {
        status_accumulator += (uint32_t)bevh_post(&ctx, &event);
        status_accumulator += (uint32_t)bevh_run_once(&ctx);
    }
    end = clock();
    test_expect(status_accumulator == 0u, "bench core status");
    g_bench_sink += handler_stats.calls_a;
    bench_report("core post+run_once", start, end, TEST_BENCH_ITERS);
}

static void print_ram_model(void) {
    const size_t queue_ram = sizeof(bevh_event_queue_t) +
                             (sizeof(bevh_event_t) * (size_t)BEVH_EVENT_QUEUE_SIZE);
    const size_t dispatcher_ram = sizeof(bevh_dispatcher_t) +
                                  (sizeof(bevh_handler_entry_t) * (size_t)BEVH_MAX_HANDLERS);
    const size_t timer_ram = sizeof(bevh_timer_mgr_t) +
                             (sizeof(bevh_timer_t) * (size_t)BEVH_MAX_TIMERS);
    const size_t context_ram = sizeof(bevh_t);
    const size_t total_default_ram = queue_ram + dispatcher_ram + timer_ram + context_ram;

    printf("SIZE sizeof(bevh_event_t)          %lu bytes\n", (unsigned long)sizeof(bevh_event_t));
    printf("SIZE sizeof(bevh_event_queue_t)    %lu bytes\n", (unsigned long)sizeof(bevh_event_queue_t));
    printf("SIZE sizeof(bevh_handler_entry_t)  %lu bytes\n", (unsigned long)sizeof(bevh_handler_entry_t));
    printf("SIZE sizeof(bevh_dispatcher_t)     %lu bytes\n", (unsigned long)sizeof(bevh_dispatcher_t));
    printf("SIZE sizeof(bevh_timer_t)          %lu bytes\n", (unsigned long)sizeof(bevh_timer_t));
    printf("SIZE sizeof(bevh_timer_mgr_t)      %lu bytes\n", (unsigned long)sizeof(bevh_timer_mgr_t));
    printf("SIZE sizeof(bevh_t)                %lu bytes\n", (unsigned long)sizeof(bevh_t));
    printf("RAM  default queue                 %lu bytes\n", (unsigned long)queue_ram);
    printf("RAM  default dispatcher            %lu bytes\n", (unsigned long)dispatcher_ram);
    printf("RAM  default timer manager         %lu bytes\n", (unsigned long)timer_ram);
    printf("RAM  default total framework       %lu bytes\n", (unsigned long)total_default_ram);
}

int main(void) {
    printf("BEVH host test: catchup=%u max_catchup=%u run_budget=%u\n",
           (unsigned int)BEVH_TIMER_ENABLE_CATCHUP,
           (unsigned int)BEVH_TIMER_MAX_CATCHUP_EVENTS,
           (unsigned int)BEVH_RUN_MAX_EVENTS_PER_CALL);

    test_queue();
    test_dispatcher();
    test_timer();
    test_core();
    print_ram_model();
    bench_functions();

    printf("PASS\n");
    return 0;
}
