#ifndef __DYNAREC_STATS_H__
#define __DYNAREC_STATS_H__

#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include "dynablock_private.h"
#ifndef _WIN32
#include <pthread.h>  // For Linux/Unix builds
#endif
#include "mypthread.h"  // Provides pthread wrappers (WOW64) or passthrough (Linux)

// Forward declaration
typedef struct box64context_s box64context_t;

// Global statistics structure
typedef struct dynablock_stats_s {
    // Context reference
    box64context_t* context;                     // Box64 context (for mutex access)

    // List of all living dynamic blocks
    dynablock_t* head;                           // List head (protected by mutex_dyndump)

    // Counters
    _Atomic uint64_t total_blocks_created;      // Total blocks ever created
    _Atomic uint64_t total_blocks_freed;        // Total blocks freed
    _Atomic uint64_t total_executions;          // Total block executions

    // Configuration
    uint64_t report_interval;                   // Report every N executions (500)
    uint64_t last_report_count;                 // Last reported at this count

    // Stats thread control
    pthread_t stats_thread;                     // Stats thread handle
    _Atomic int stats_thread_running;           // 1 = running, 0 = stopped
    _Atomic int stats_requested;                // 1 = stats report requested

    // File output
    FILE* stats_file;                           // File to write stats to

} dynablock_stats_t;

// API Functions
void init_block_stats(box64context_t* context, uint64_t report_interval);
void cleanup_block_stats(box64context_t* context);
void add_block_to_stats(dynablock_t* block);
void remove_block_from_stats(dynablock_t* block);
void print_block_stats(void);
void request_block_stats(void);  // Async stats request

#endif // __DYNAREC_STATS_H__