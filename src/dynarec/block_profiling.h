#ifndef __BLOCK_PROFILING_H__
#define __BLOCK_PROFILING_H__

#include <stdint.h>
#include <stdatomic.h>
#include "dynablock_private.h"

// Block profiling statistics structure
typedef struct block_stats_s {
    uint64_t total_blocks;
    uint64_t total_executions;
    uint64_t hot_blocks;      // usage > 10000
    uint64_t warm_blocks;     // usage 1000-10000
    uint64_t cold_blocks;     // usage < 1000
    uint64_t avg_usage;
    uint64_t max_usage;
    void*    max_usage_addr;
} block_stats_t;

// Initialize block profiling
void init_block_profiling(void);

// Shutdown and report statistics
void shutdown_block_profiling(void);

// Report current statistics
void report_block_stats(void);

// Atomic increment of block usage counter
static inline void increment_block_counter(dynablock_t* block) {
    if (block) {
        atomic_fetch_add_explicit(&block->usage_count, 1, memory_order_relaxed);
    }
}

// Get block usage count
static inline uint64_t get_block_usage(dynablock_t* block) {
    return block ? atomic_load_explicit(&block->usage_count, memory_order_relaxed) : 0;
}

// Log block statistics on free
void log_block_free_stats(dynablock_t* block);

// Register block creation (updates global counter)
void register_block_creation(dynablock_t* block);

// Scan all live blocks and collect statistics (for periodic reporting)
void scan_live_blocks_stats(void);

#endif // __BLOCK_PROFILING_H__
