#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "debug.h"
#include "dynarec_stats.h"
#include "dynablock_private.h"
#include "box64context.h"
#include "box64cpu.h"

// Global statistics instance
static dynablock_stats_t global_stats = {0};

// External reference to Box64 context
extern box64context_t *my_context;

// Forward declaration for stats thread
static void* stats_thread_func(void* arg);

// Helper function to format memory sizes
static const char* format_memory(uint64_t bytes, char* buffer, size_t bufsize) {
    if(bytes < 1024) {
        snprintf(buffer, bufsize, "%luB", bytes);
    } else if(bytes < 1024*1024) {
        snprintf(buffer, bufsize, "%.1fKB", bytes / 1024.0);
    } else {
        snprintf(buffer, bufsize, "%.2fMB", bytes / (1024.0 * 1024.0));
    }
    return buffer;
}

// Calculate total executions by summing all usage_counts
static uint64_t calculate_total_executions(void)
{
    uint64_t total = 0;

    mutex_lock(&my_context->mutex_dyndump);

    dynablock_t* current = (dynablock_t*)atomic_load(&global_stats.head);
    while(current) {
        total += atomic_load(&current->usage_count);
        current = (dynablock_t*)atomic_load(&current->next_in_stats);
    }

    mutex_unlock(&my_context->mutex_dyndump);

    return total;
}

// Initialize statistics system
void init_block_stats(uint64_t report_interval)
{
    dynarec_log(LOG_INFO, "Initializing block statistics (report every %lu executions)\n",
                report_interval);

    atomic_store(&global_stats.head, NULL);
    atomic_store(&global_stats.total_blocks_created, 0);
    atomic_store(&global_stats.total_blocks_freed, 0);
    atomic_store(&global_stats.total_executions, 0);

    global_stats.report_interval = report_interval;
    global_stats.last_report_count = 0;

    atomic_store(&global_stats.stats_thread_running, 1);
    atomic_store(&global_stats.stats_requested, 0);

    // Open stats file for writing
    global_stats.stats_file = fopen("box64_block_stats.txt", "w");
    if(!global_stats.stats_file) {
        dynarec_log(LOG_INFO, "Warning: Failed to open box64_block_stats.txt for writing\n");
    } else {
        fprintf(global_stats.stats_file, "# Box64 Dynamic Block Statistics\n");
        fprintf(global_stats.stats_file, "# Report interval: every %lu block executions\n", report_interval);
        fprintf(global_stats.stats_file, "# Format: [Timestamp] Blocks: total, low_usage_count (percentage%%) | Memory: total, low_usage_memory (percentage%%)\n\n");
        fflush(global_stats.stats_file);
    }

    // Create stats thread
    if(pthread_create(&global_stats.stats_thread, NULL, stats_thread_func, NULL) != 0) {
        dynarec_log(LOG_INFO, "Warning: Failed to create stats thread\n");
        atomic_store(&global_stats.stats_thread_running, 0);
    }
}

// Add newly created block to the global list
void add_block_to_stats(dynablock_t* block)
{
    if(!block) return;

    // IMPORTANT: This function is called from FillBlock64(),
    // which already holds my_context->mutex_dyndump.
    // DO NOT acquire mutex again!

    dynarec_log(LOG_DEBUG, "Adding block %p to stats (x64=%p, size=%lu)\n",
                block, block->x64_addr, block->x64_size);

    // Initialize next pointer
    atomic_store(&block->next_in_stats, (dynablock_t*)NULL);

    // Add to head of list (simple prepend)
    dynablock_t* old_head = (dynablock_t*)atomic_load(&global_stats.head);
    atomic_store(&block->next_in_stats, (dynablock_t*)old_head);
    atomic_store(&global_stats.head, (dynablock_t*)block);

    // Update counter
    atomic_fetch_add(&global_stats.total_blocks_created, 1);

    dynarec_log(LOG_DEBUG, "Block %p added to stats (total created: %lu)\n",
                block, atomic_load(&global_stats.total_blocks_created));
}

// Remove block from the global list when it's being freed
void remove_block_from_stats(dynablock_t* block)
{
    if(!block) return;

    // IMPORTANT: This function is called from FreeDynablock(),
    // which already holds my_context->mutex_dyndump.
    // DO NOT acquire mutex again!

    dynarec_log(LOG_DEBUG, "Removing block %p from stats (x64=%p)\n",
                block, block->x64_addr);

    // Simple linear scan to remove
    // This is O(N) but block deletion is rare

    dynablock_t* current = (dynablock_t*)atomic_load(&global_stats.head);
    dynablock_t* prev = NULL;

    while(current) {
        if(current == block) {
            // Found it! Unlink
            if(prev) {
                // Middle or end of list
                dynablock_t* next = (dynablock_t*)atomic_load(&current->next_in_stats);
                atomic_store(&prev->next_in_stats, (dynablock_t*)next);
            } else {
                // Head of list
                dynablock_t* next = (dynablock_t*)atomic_load(&current->next_in_stats);
                atomic_store(&global_stats.head, (dynablock_t*)next);
            }

            atomic_fetch_add(&global_stats.total_blocks_freed, 1);

            dynarec_log(LOG_DEBUG, "Block %p removed from stats (total freed: %lu)\n",
                        block, atomic_load(&global_stats.total_blocks_freed));
            return;
        }

        prev = current;
        current = (dynablock_t*)atomic_load(&current->next_in_stats);
    }

    dynarec_log(LOG_INFO, "Warning: Block %p not found in stats list during removal\n", block);
}

// Print statistics with memory information
void print_block_stats(void)
{
    // Acquire mutex to ensure stable list during iteration
    mutex_lock(&my_context->mutex_dyndump);

    uint64_t total_blocks = 0;
    uint64_t blocks_never_executed = 0;
    uint64_t blocks_executed_once = 0;
    uint64_t blocks_executed_multiple = 0;

    // Memory statistics (in bytes)
    uint64_t total_memory = 0;
    uint64_t memory_never_executed = 0;
    uint64_t memory_executed_once = 0;
    uint64_t memory_executed_multiple = 0;

    dynarec_log(LOG_DEBUG, "Scanning block stats...\n");

    dynablock_t* current = (dynablock_t*)atomic_load(&global_stats.head);
    while(current) {
        total_blocks++;

        uint64_t usage = atomic_load(&current->usage_count);
        size_t block_size = current->native_size;  // Native code size in bytes

        total_memory += block_size;

        if(usage == 0) {
            blocks_never_executed++;
            memory_never_executed += block_size;
        } else if(usage == 1) {
            blocks_executed_once++;
            memory_executed_once += block_size;
        } else {
            blocks_executed_multiple++;
            memory_executed_multiple += block_size;
        }

        current = (dynablock_t*)atomic_load(&current->next_in_stats);
    }

    mutex_unlock(&my_context->mutex_dyndump);

    // Calculate block statistics
    uint64_t blocks_low_usage = blocks_never_executed + blocks_executed_once;
    double block_percentage = (total_blocks > 0) ?
                              (100.0 * blocks_low_usage / total_blocks) : 0.0;

    // Calculate memory statistics
    uint64_t memory_low_usage = memory_never_executed + memory_executed_once;
    double memory_percentage = (total_memory > 0) ?
                               (100.0 * memory_low_usage / total_memory) : 0.0;

    // Format memory sizes
    char total_mem_str[32], low_usage_mem_str[32];
    char never_mem_str[32], once_mem_str[32], multi_mem_str[32];
    format_memory(total_memory, total_mem_str, sizeof(total_mem_str));
    format_memory(memory_low_usage, low_usage_mem_str, sizeof(low_usage_mem_str));

    // Write to stats file (if open)
    if(global_stats.stats_file) {
        fprintf(global_stats.stats_file, "[BOX64 STATS] Blocks: %lu total, %lu (%.1f%%) executed <=1 times\n",
                total_blocks, blocks_low_usage, block_percentage);
        fprintf(global_stats.stats_file, "[BOX64 STATS] Memory: %s total, %s (%.1f%%) in blocks executed <=1 times\n",
                total_mem_str, low_usage_mem_str, memory_percentage);
        fflush(global_stats.stats_file);
    }

    // Also print to console for debugging
    dynarec_log(LOG_INFO, "[BOX64 STATS] Blocks: %lu total, %lu (%.1f%%) executed <=1 times\n",
                total_blocks, blocks_low_usage, block_percentage);
    dynarec_log(LOG_INFO, "[BOX64 STATS] Memory: %s total, %s (%.1f%%) in blocks executed <=1 times\n",
                total_mem_str, low_usage_mem_str, memory_percentage);

    // Detailed debug output
    dynarec_log(LOG_DEBUG, "  Block breakdown:\n");
    dynarec_log(LOG_DEBUG, "    Never executed: %lu blocks (%.1f%%), %s\n",
                blocks_never_executed,
                total_blocks > 0 ? 100.0 * blocks_never_executed / total_blocks : 0.0,
                format_memory(memory_never_executed, never_mem_str, sizeof(never_mem_str)));
    dynarec_log(LOG_DEBUG, "    Executed once:  %lu blocks (%.1f%%), %s\n",
                blocks_executed_once,
                total_blocks > 0 ? 100.0 * blocks_executed_once / total_blocks : 0.0,
                format_memory(memory_executed_once, once_mem_str, sizeof(once_mem_str)));
    dynarec_log(LOG_DEBUG, "    Executed >1:    %lu blocks (%.1f%%), %s\n",
                blocks_executed_multiple,
                total_blocks > 0 ? 100.0 * blocks_executed_multiple / total_blocks : 0.0,
                format_memory(memory_executed_multiple, multi_mem_str, sizeof(multi_mem_str)));
}

// Request stats report asynchronously
void request_block_stats(void)
{
    atomic_store(&global_stats.stats_requested, 1);
}

// Cleanup statistics system
void cleanup_block_stats(void)
{
    dynarec_log(LOG_INFO, "Cleaning up block statistics\n");

    // Signal stats thread to stop
    atomic_store(&global_stats.stats_thread_running, 0);

    // Wait for stats thread to exit
    if(global_stats.stats_thread) {
        pthread_join(global_stats.stats_thread, NULL);
    }

    // Print final statistics
    print_block_stats();

    // List will be cleaned up when blocks are freed
    dynarec_log(LOG_INFO, "Block statistics cleanup complete\n");
}

// Stats thread function (Phase 3)
static void* stats_thread_func(void* arg)
{
    (void)arg;

    dynarec_log(LOG_INFO, "Block statistics thread started (TID %d)\n", GetTID());

    uint64_t last_total_executions = 0;

    while(atomic_load(&global_stats.stats_thread_running)) {
        // Sleep for 100ms (reduce CPU usage)
        usleep(100000);

        // Check if stats report requested
        if(atomic_load(&global_stats.stats_requested)) {
            print_block_stats();
            atomic_store(&global_stats.stats_requested, 0);
            continue;
        }

        // Calculate current total executions
        uint64_t current_executions = calculate_total_executions();

        // Check if we've crossed the report threshold
        if(current_executions >= global_stats.last_report_count + global_stats.report_interval) {
            print_block_stats();
            global_stats.last_report_count = current_executions;
        }

        last_total_executions = current_executions;
    }

    dynarec_log(LOG_INFO, "Block statistics thread exiting\n");
    return NULL;
}