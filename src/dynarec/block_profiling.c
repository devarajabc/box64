#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include "block_profiling.h"
#include "debug.h"
#include "box64context.h"
#include "custommem.h"

static _Atomic uint64_t global_block_count = 0;
static _Atomic uint64_t global_execution_count = 0;
static _Atomic uint64_t single_execution_blocks = 0;
static _Atomic uint64_t blocks_freed = 0;
static int profiling_enabled = 0;
static int stats_printed = 0;
static uint64_t report_interval = 500;  // Report every 500 blocks created

// Lock-free MPMC queue implementation for block tracking
#define MPMC_PAGE_SIZE 4096
#define MPMC_CACHE_LINE_SIZE 64
#define MPMC_N (1 << 12)  // 4096 cells per node
#define MPMC_N_BITS (MPMC_N - 1)

#define __MPMC_CACHE_ALIGNED __attribute__((aligned(MPMC_CACHE_LINE_SIZE)))
#define __MPMC_DOUBLE_CACHE_ALIGNED __attribute__((aligned(2 * MPMC_CACHE_LINE_SIZE)))

typedef struct mpmc_node {
    struct mpmc_node *volatile next __MPMC_DOUBLE_CACHE_ALIGNED;
    long id __MPMC_DOUBLE_CACHE_ALIGNED;
    void *cells[MPMC_N] __MPMC_DOUBLE_CACHE_ALIGNED;
} mpmc_node_t;

typedef struct {
    mpmc_node_t *spare;
    mpmc_node_t *volatile push __MPMC_CACHE_ALIGNED;
    mpmc_node_t *volatile pop __MPMC_CACHE_ALIGNED;
} mpmc_handle_t;

typedef struct {
    mpmc_node_t *init_node;
    volatile long init_id __MPMC_DOUBLE_CACHE_ALIGNED;
    volatile long put_index __MPMC_DOUBLE_CACHE_ALIGNED;
    volatile long pop_index __MPMC_DOUBLE_CACHE_ALIGNED;
} mpmc_queue_t;

static mpmc_queue_t block_queue;
static __thread mpmc_handle_t *thread_handle = NULL;

// Allocate a new MPMC node
static inline mpmc_node_t *mpmc_new_node(void)
{
    mpmc_node_t *n;
    if (posix_memalign((void **)&n, MPMC_PAGE_SIZE, sizeof(mpmc_node_t)) != 0)
        return NULL;
    memset(n, 0, sizeof(mpmc_node_t));
    return n;
}

// Get or create thread-local handle
static mpmc_handle_t *mpmc_get_handle(void)
{
    if (!thread_handle) {
        // Safety check: ensure queue is initialized
        if (!block_queue.init_node) {
            return NULL;
        }

        thread_handle = calloc(1, sizeof(mpmc_handle_t));
        if (!thread_handle) {
            return NULL;
        }

        thread_handle->spare = mpmc_new_node();
        // Note: spare can be NULL, it will be allocated on-demand
        thread_handle->push = thread_handle->pop = block_queue.init_node;
    }
    return thread_handle;
}

// Find cell in MPMC queue
static void *mpmc_find_cell(mpmc_node_t *volatile *ptr, long i, mpmc_handle_t *th)
{
    mpmc_node_t *curr = *ptr;

    // Safety check: ensure curr is not NULL
    if (!curr) {
        return NULL;
    }

    for (long j = curr->id; j < i / MPMC_N; ++j) {
        mpmc_node_t *next = curr->next;
        if (!next) {
            mpmc_node_t *tmp = th->spare;
            if (!tmp) {
                tmp = mpmc_new_node();
                th->spare = tmp;
            }
            if (tmp) {
                tmp->id = j + 1;
                if (__atomic_compare_exchange_n(&curr->next, &next, tmp, 0,
                                               __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
                    next = tmp;
                    th->spare = NULL;
                }
            }
        }
        if (next)
            curr = next;
    }
    *ptr = curr;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return &curr->cells[i & MPMC_N_BITS];
}

// Enqueue a block (multi-producer safe)
static void mpmc_enqueue_block(dynablock_t *block)
{
    mpmc_handle_t *th = mpmc_get_handle();
    if (!th) return;

    void *volatile *c = mpmc_find_cell(&th->push,
                                       __atomic_fetch_add(&block_queue.put_index, 1, __ATOMIC_SEQ_CST),
                                       th);
    if (!c) {
        // Failed to find cell - queue not properly initialized
        __atomic_fetch_sub(&block_queue.put_index, 1, __ATOMIC_SEQ_CST);
        return;
    }
    __atomic_store_n(c, block, __ATOMIC_RELEASE);
}

// Try to dequeue a block (non-blocking, returns NULL if empty)
static dynablock_t *mpmc_try_dequeue_block(void)
{
    mpmc_handle_t *th = mpmc_get_handle();
    if (!th) return NULL;

    // Check if queue is empty
    long put_idx = __atomic_load_n(&block_queue.put_index, __ATOMIC_ACQUIRE);
    long pop_idx = __atomic_load_n(&block_queue.pop_index, __ATOMIC_ACQUIRE);
    if (pop_idx >= put_idx)
        return NULL;

    // Try to claim a slot
    long index = __atomic_fetch_add(&block_queue.pop_index, 1, __ATOMIC_SEQ_CST);
    if (index >= put_idx) {
        __atomic_fetch_sub(&block_queue.pop_index, 1, __ATOMIC_SEQ_CST);
        return NULL;
    }

    void *volatile *c = mpmc_find_cell(&th->pop, index, th);
    if (!c) {
        // Failed to find cell - queue not properly initialized
        __atomic_fetch_sub(&block_queue.pop_index, 1, __ATOMIC_SEQ_CST);
        return NULL;
    }

    // Spin a bit waiting for value
    void *cv = NULL;
    for (int tries = 0; tries < (1 << 20); tries++) {
        cv = __atomic_load_n(c, __ATOMIC_ACQUIRE);
        if (cv) break;
#if defined(__x86_64__) || defined(__i386__)
        __asm__ __volatile__("pause");
#elif defined(__aarch64__) || defined(__arm__)
        __asm__ __volatile__("isb\n");
#endif
    }

    // If we timed out waiting, return NULL (not the uninitialized cv)
    return (dynablock_t *)cv;
}

// Array to store all tracked blocks (grows dynamically)
static dynablock_t** all_blocks = NULL;
static size_t all_blocks_count = 0;
static size_t all_blocks_capacity = 0;

// Drain all pending blocks from MPMC queue into array (multi-consumer safe!)
static void drain_queue_to_array(void)
{
    dynablock_t* block;
    while ((block = mpmc_try_dequeue_block()) != NULL) {
        // Expand array if needed
        if (all_blocks_count >= all_blocks_capacity) {
            size_t new_capacity = all_blocks_capacity == 0 ? 1024 : all_blocks_capacity * 2;
            dynablock_t** new_array = (dynablock_t**)realloc(all_blocks, new_capacity * sizeof(dynablock_t*));
            if (new_array) {
                all_blocks = new_array;
                all_blocks_capacity = new_capacity;
            } else {
                break;  // Out of memory, stop draining
            }
        }

        // Add block to array
        all_blocks[all_blocks_count++] = block;
    }
}

void init_block_profiling(void)
{
    profiling_enabled = BOX64ENV(dynarec_profile);
    if (profiling_enabled) {
        atomic_store(&global_block_count, 0);
        atomic_store(&global_execution_count, 0);
        atomic_store(&single_execution_blocks, 0);
        atomic_store(&blocks_freed, 0);

        // Initialize MPMC queue
        block_queue.init_node = mpmc_new_node();
        if (!block_queue.init_node) {
            printf("[BOX64] ERROR: Failed to initialize block profiling queue\n");
            profiling_enabled = 0;
            return;
        }
        block_queue.put_index = block_queue.pop_index = block_queue.init_id = 0;

        // Register atexit handler to ensure statistics are printed even if shutdown is incomplete
        atexit(report_block_stats);
        printf("[BOX64] Block profiling initialized (reporting every %lu blocks created)\n", report_interval);
    }
}

void shutdown_block_profiling(void)
{
    if (profiling_enabled) {
        report_block_stats();
        printf("[BOX64] Block profiling shutdown\n");
    }
}

// Print periodic statistics (can be called multiple times)
static void print_periodic_stats(void)
{
    // Scan all live blocks for accurate statistics
    scan_live_blocks_stats();
}

void report_block_stats(void)
{
    if (!profiling_enabled) return;

    // Prevent double printing (in case both atexit and shutdown are called)
    if (stats_printed) return;
    stats_printed = 1;

    uint64_t total_blocks = atomic_load(&global_block_count);
    uint64_t total_executions = atomic_load(&global_execution_count);
    uint64_t single_exec = atomic_load(&single_execution_blocks);
    uint64_t freed = atomic_load(&blocks_freed);

    printf("\n");
    printf("========================================\n");
    printf("     DYNABLOCK PROFILING STATISTICS\n");
    printf("========================================\n");
    printf("Total blocks created:  %lu\n", total_blocks);
    printf("Total blocks freed:    %lu\n", freed);
    if (total_blocks >= freed) {
        printf("Blocks still alive:    %lu\n", total_blocks - freed);
        if (total_blocks == freed) {
            printf("✓ All blocks tracked correctly!\n");
        }
    } else {
        printf("WARNING: Freed > Created! (%lu > %lu) - Tracking error!\n", freed, total_blocks);
    }
    printf("Total executions:      %lu\n", total_executions);
    if (total_blocks > 0) {
        printf("Single-exec blocks (freed):    %lu (%.2f%%)\n",
               single_exec, (double)single_exec * 100.0 / freed);
        printf("Single-exec blocks:    %lu (%.2f%%)\n",
               single_exec, (double)single_exec * 100.0 / total_blocks);
    }
    printf("========================================\n");
    printf("\n");
}

void log_block_free_stats(dynablock_t* block)
{
    if (!profiling_enabled || !block) return;

    uint64_t usage = atomic_load_explicit(&block->usage_count, memory_order_relaxed);
    uint64_t now = (uint64_t)time(NULL);
    uint64_t lifetime = now - block->created_time;

    // Update global stats
    atomic_fetch_add(&global_execution_count, usage);

    // Track single-execution blocks
    if (usage == 1) {
        atomic_fetch_add(&single_execution_blocks, 1);
    }

    // Increment freed blocks counter
    atomic_fetch_add(&blocks_freed, 1);

    // Log individual block stats if it was used
    if (usage > 0) {
        const char* category = "COLD";
        if (usage >= 1000) category = "HOT";
        else if (usage >= 100) category = "WARM";

        printf("[%s] Block freed: addr=%p size=%d insts=%d usage=%lu lifetime=%lus avg=%.2f/s\n",
               category, block->x64_addr, (int)block->x64_size, block->isize,
               usage, lifetime,
               lifetime > 0 ? (double)usage / lifetime : (double)usage);
    }
}

// Called when a new block is created (multi-producer safe!)
void register_block_creation(dynablock_t* block)
{
    if (!profiling_enabled || !block) return;

    // Increment block count and check if we should print periodic stats
    uint64_t count = atomic_fetch_add(&global_block_count, 1) + 1;

    // Push to MPMC queue (multiple threads can enqueue and dequeue safely)
    mpmc_enqueue_block(block);

    // Print periodic statistics every N blocks created
    if (report_interval > 0 && (count % report_interval) == 0) {
        print_periodic_stats();
    }
}

// Scan all live blocks and collect statistics (multi-consumer safe with MPMC!)
void scan_live_blocks_stats(void)
{
    if (!profiling_enabled) return;

    // Drain any new blocks from MPMC queue into our array (thread-safe)
    drain_queue_to_array();

    uint64_t live_blocks = 0;
    uint64_t live_single_exec = 0;
    uint64_t freed_count = atomic_load(&blocks_freed);

    // Scan all blocks in array (safe - blocks are never removed from array)
    for (size_t i = 0; i < all_blocks_count; i++) {
        dynablock_t* block = all_blocks[i];
        if (block && !block->gone) {  // Only count blocks that are still alive
            live_blocks++;
            uint64_t usage = atomic_load_explicit(&block->usage_count, memory_order_relaxed);
            if (usage == 1) {
                live_single_exec++;
            }
        }
    }

    printf("\n");
    printf("======== LIVE BLOCK STATISTICS ========\n");
    printf("Total blocks created:  %lu\n", atomic_load(&global_block_count));
    printf("Blocks still alive:    %lu\n", live_blocks);
    printf("Blocks freed:          %lu\n", freed_count);
    if (live_blocks > 0) {
        printf("Live single-exec:      %lu (%.2f%%)\n",
               live_single_exec, (double)live_single_exec * 100.0 / live_blocks);
    }
    printf("=======================================\n");
    printf("\n");
}
