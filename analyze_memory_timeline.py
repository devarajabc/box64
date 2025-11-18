#!/usr/bin/env python3
"""
Box64 Memory Timeline Analyzer
Shows memory allocation and purging patterns over time
"""

import sys
from collections import defaultdict

def format_bytes(bytes_val):
    """Format bytes into human-readable size"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if bytes_val < 1024.0:
            return f"{bytes_val:8.2f} {unit}"
        bytes_val /= 1024.0
    return f"{bytes_val:8.2f} TB"

def analyze_timeline(filename):
    print("=" * 80)
    print("BOX64 MEMORY ALLOCATION & PURGING TIMELINE")
    print("=" * 80)
    print()

    # Time buckets (in milliseconds) - group by 10-second intervals
    bucket_size = 10000  # 10 seconds
    buckets = defaultdict(lambda: {
        'allocated': 0,
        'freed': 0,
        'purge_attempts': 0,
        'successful_purges': 0,
        'failed_purges': 0,
        'total_memory': 0,
        'purgeable_memory': 0
    })

    start_time = None
    end_time = None

    print("⏳ Parsing timeline data...")
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            parts = line.split(',')
            record_type = parts[0]

            if len(parts) < 2:
                continue

            timestamp = int(parts[1])
            if start_time is None:
                start_time = timestamp
            end_time = timestamp

            # Calculate bucket (time since start, in 10-second intervals)
            bucket = (timestamp - start_time) // bucket_size

            # Track allocations
            if record_type == 'DIAG_ALLOC_REQUEST':
                size = int(parts[4])
                buckets[bucket]['allocated'] += size

            # Track freed memory
            elif record_type == 'DIAG_PURGED':
                size_field = parts[4]
                size = int(size_field.split('=')[1])
                buckets[bucket]['freed'] += size

            # Track purge attempts
            elif record_type == 'DIAG_PURGE_END':
                success = int(parts[3].split('=')[1])
                buckets[bucket]['purge_attempts'] += 1
                if success:
                    buckets[bucket]['successful_purges'] += 1
                else:
                    buckets[bucket]['failed_purges'] += 1

            # Track memory state
            elif record_type == 'DIAG_PURGE_MEMORY':
                total = int(parts[3].split('=')[1])
                purgeable = int(parts[5].split('=')[1])
                buckets[bucket]['total_memory'] = total
                buckets[bucket]['purgeable_memory'] = purgeable

    print("✅ Parsing complete!\n")

    # Calculate total duration
    duration_ms = end_time - start_time
    duration_sec = duration_ms / 1000.0

    print(f"Execution duration: {duration_sec:.1f} seconds ({duration_ms:,} ms)")
    print(f"Time bucket size: {bucket_size/1000:.1f} seconds")
    print(f"Total buckets: {len(buckets)}")
    print()

    # Display timeline
    print("=" * 80)
    print("TIMELINE (10-second intervals)")
    print("=" * 80)
    print(f"{'Time':>8s} | {'Allocated':>12s} | {'Freed':>12s} | {'Savings':>8s} | {'Purges':>10s}")
    print("-" * 80)

    cumulative_alloc = 0
    cumulative_freed = 0

    for bucket in sorted(buckets.keys()):
        data = buckets[bucket]
        allocated = data['allocated']
        freed = data['freed']
        purge_attempts = data['purge_attempts']
        successful = data['successful_purges']

        cumulative_alloc += allocated
        cumulative_freed += freed

        time_label = f"{bucket * bucket_size / 1000:.0f}s"
        savings_pct = (freed / allocated * 100) if allocated > 0 else 0

        purge_str = f"{successful}/{purge_attempts}" if purge_attempts > 0 else "-"

        print(f"{time_label:>8s} | {format_bytes(allocated)} | {format_bytes(freed)} | "
              f"{savings_pct:7.2f}% | {purge_str:>10s}")

    print("-" * 80)
    total_savings = (cumulative_freed / cumulative_alloc * 100) if cumulative_alloc > 0 else 0
    print(f"{'TOTAL':>8s} | {format_bytes(cumulative_alloc)} | {format_bytes(cumulative_freed)} | "
          f"{total_savings:7.2f}% |")
    print("=" * 80)
    print()

    # Memory pressure over time
    print("=" * 80)
    print("MEMORY PRESSURE OVER TIME")
    print("=" * 80)
    print(f"{'Time':>8s} | {'Total Mem':>12s} | {'Purgeable':>12s} | {'In-Use':>12s} | {'Usage':>7s}")
    print("-" * 80)

    for bucket in sorted(buckets.keys()):
        data = buckets[bucket]
        total = data['total_memory']
        purgeable = data['purgeable_memory']

        if total == 0:
            continue

        in_use = total - purgeable
        usage_pct = (in_use / total * 100) if total > 0 else 0

        time_label = f"{bucket * bucket_size / 1000:.0f}s"

        print(f"{time_label:>8s} | {format_bytes(total)} | {format_bytes(purgeable)} | "
              f"{format_bytes(in_use)} | {usage_pct:6.1f}%")

    print("=" * 80)
    print()

    # Analysis summary
    print("📊 ANALYSIS")
    print("-" * 80)

    # Find peak memory
    peak_bucket = max(buckets.keys(), key=lambda b: buckets[b]['total_memory'])
    peak_mem = buckets[peak_bucket]['total_memory']
    peak_time = peak_bucket * bucket_size / 1000

    # Find highest allocation rate
    highest_alloc_bucket = max(buckets.keys(), key=lambda b: buckets[b]['allocated'])
    highest_alloc = buckets[highest_alloc_bucket]['allocated']
    highest_alloc_time = highest_alloc_bucket * bucket_size / 1000

    # Find highest freeing rate
    highest_freed_bucket = max(buckets.keys(), key=lambda b: buckets[b]['freed'])
    highest_freed = buckets[highest_freed_bucket]['freed']
    highest_freed_time = highest_freed_bucket * bucket_size / 1000

    print(f"Peak memory usage:        {format_bytes(peak_mem)} at {peak_time:.0f}s")
    print(f"Highest allocation rate:  {format_bytes(highest_alloc)} at {highest_alloc_time:.0f}s")
    print(f"Highest freeing rate:     {format_bytes(highest_freed)} at {highest_freed_time:.0f}s")
    print()

    # Interpretation
    print("💡 INTERPRETATION")
    print("-" * 80)
    if total_savings < 1:
        print("• Very low memory recycling rate (<1%)")
        print("• Most allocated dynarec blocks remain HOT (actively used)")
        print("• This is NORMAL for workloads that reuse the same code paths")
        print("• The low recycling is NOT a bug - it means code is being used efficiently")
    elif total_savings < 10:
        print("• Low memory recycling rate (<10%)")
        print("• Most dynarec blocks are actively used")
        print("• Normal for applications with consistent execution patterns")
    elif total_savings < 50:
        print("• Moderate memory recycling")
        print("• Good balance between code reuse and purging")
    else:
        print("• High memory recycling rate")
        print("• Dynarec blocks are frequently replaced")
        print("• May indicate highly dynamic code execution")

    print()
    print("=" * 80)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 analyze_memory_timeline.py <diagnostic_csv_file>")
        print("Example: python3 analyze_memory_timeline.py box64_sleeping_diag_62168.csv")
        sys.exit(1)

    analyze_timeline(sys.argv[1])
