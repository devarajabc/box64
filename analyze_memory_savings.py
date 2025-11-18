#!/usr/bin/env python3
"""
Box64 Memory Savings Analyzer
Analyzes how much memory was saved through dynarec purging
"""

import sys
import re
from collections import defaultdict

def parse_size(value_str):
    """Parse size value from CSV field"""
    if '=' in value_str:
        return int(value_str.split('=')[1])
    return int(value_str)

def analyze_memory_savings(filename):
    print("=" * 70)
    print("BOX64 DYNAREC MEMORY SAVINGS ANALYSIS")
    print("=" * 70)
    print()

    # Statistics to track
    total_allocated = 0
    total_freed = 0
    allocation_count = 0
    purge_count = 0
    successful_purges = 0
    failed_purges = 0

    freed_per_purge = []
    memory_stats = []

    # Parse the CSV file
    print("⏳ Parsing diagnostic file...")
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            parts = line.split(',')
            record_type = parts[0]

            # Track allocations
            if record_type == 'DIAG_ALLOC_REQUEST':
                # DIAG_ALLOC_REQUEST,timestamp,sequence,x64_addr,size
                size = int(parts[4])
                total_allocated += size
                allocation_count += 1

            # Track individual freed blocks
            elif record_type == 'DIAG_PURGED':
                # DIAG_PURGED,timestamp,sequence,block_addr,hot=X,size=Y
                size_field = parts[4]
                size = parse_size(size_field)
                total_freed += size

            # Track purge attempts
            elif record_type == 'DIAG_PURGE_END':
                # DIAG_PURGE_END,timestamp,sequence,success=X
                success = int(parts[3].split('=')[1])
                purge_count += 1
                if success:
                    successful_purges += 1
                else:
                    failed_purges += 1

            # Track memory statistics per purge
            elif record_type == 'DIAG_PURGE_MEMORY':
                # DIAG_PURGE_MEMORY,timestamp,sequence,total=X,pinned=Y,purgeable=Z
                timestamp = int(parts[1])
                total_mem = parse_size(parts[3])
                pinned_mem = parse_size(parts[4])
                purgeable_mem = parse_size(parts[5])
                memory_stats.append({
                    'timestamp': timestamp,
                    'total': total_mem,
                    'pinned': pinned_mem,
                    'purgeable': purgeable_mem
                })

    print("✅ Parsing complete!\n")

    # Calculate statistics
    savings_rate = (total_freed / total_allocated * 100) if total_allocated > 0 else 0
    avg_alloc_size = total_allocated / allocation_count if allocation_count > 0 else 0
    avg_freed_per_purge = total_freed / successful_purges if successful_purges > 0 else 0

    # Memory in human-readable format
    def format_bytes(bytes_val):
        """Format bytes into human-readable size"""
        for unit in ['B', 'KB', 'MB', 'GB']:
            if bytes_val < 1024.0:
                return f"{bytes_val:.2f} {unit}"
            bytes_val /= 1024.0
        return f"{bytes_val:.2f} TB"

    # Print results
    print("📊 ALLOCATION STATISTICS")
    print("-" * 70)
    print(f"Total allocations:        {allocation_count:,}")
    print(f"Total memory allocated:   {format_bytes(total_allocated)} ({total_allocated:,} bytes)")
    print(f"Average allocation size:  {format_bytes(avg_alloc_size)} ({avg_alloc_size:.0f} bytes)")
    print()

    print("🔥 PURGING STATISTICS")
    print("-" * 70)
    print(f"Total purge attempts:     {purge_count:,}")
    print(f"Successful purges:        {successful_purges:,} ({successful_purges/purge_count*100:.1f}%)")
    print(f"Failed purges:            {failed_purges:,} ({failed_purges/purge_count*100:.1f}%)")
    print(f"Total memory freed:       {format_bytes(total_freed)} ({total_freed:,} bytes)")
    print(f"Avg freed per purge:      {format_bytes(avg_freed_per_purge)} ({avg_freed_per_purge:.0f} bytes)")
    print()

    print("💾 MEMORY SAVINGS")
    print("-" * 70)
    print(f"Memory allocated:         {format_bytes(total_allocated)}")
    print(f"Memory freed (purged):    {format_bytes(total_freed)}")
    print(f"Memory savings rate:      {savings_rate:.2f}%")
    print(f"Net memory used:          {format_bytes(total_allocated - total_freed)}")
    print()

    # Efficiency rating
    print("⭐ EFFICIENCY RATING")
    print("-" * 70)
    if savings_rate >= 80:
        rating = "EXCELLENT ⭐⭐⭐⭐⭐"
        comment = "Very high memory recycling rate!"
    elif savings_rate >= 60:
        rating = "GOOD ⭐⭐⭐⭐"
        comment = "Good memory recycling rate"
    elif savings_rate >= 40:
        rating = "MODERATE ⭐⭐⭐"
        comment = "Moderate memory recycling"
    elif savings_rate >= 20:
        rating = "LOW ⭐⭐"
        comment = "Low memory recycling rate"
    else:
        rating = "POOR ⭐"
        comment = "Very low memory recycling"

    print(f"Rating: {rating}")
    print(f"Comment: {comment}")
    print()

    # Memory pressure analysis
    if len(memory_stats) > 0:
        print("📈 MEMORY PRESSURE ANALYSIS")
        print("-" * 70)

        # Get samples (first, middle, last)
        samples = [
            ("Start", memory_stats[0]),
            ("Middle", memory_stats[len(memory_stats)//2]),
            ("End", memory_stats[-1])
        ]

        for label, stat in samples:
            total = stat['total']
            pinned = stat['pinned']
            purgeable = stat['purgeable']
            in_use = total - purgeable

            print(f"\n{label} of execution:")
            print(f"  Total blocks memory:    {format_bytes(total)}")
            print(f"  In-use memory:          {format_bytes(in_use)} ({in_use/total*100 if total > 0 else 0:.1f}%)")
            print(f"  Purgeable memory:       {format_bytes(purgeable)} ({purgeable/total*100 if total > 0 else 0:.1f}%)")
            print(f"  Pinned (phantom) memory: {format_bytes(pinned)}")

        print()

    # Summary and recommendations
    print("=" * 70)
    print("📋 SUMMARY")
    print("=" * 70)

    if total_freed == 0:
        print("⚠️  WARNING: No memory was freed through purging!")
        print("   This could mean:")
        print("   - BOX64_DYNAREC_PURGE is not enabled")
        print("   - The workload doesn't trigger purging")
        print("   - All blocks are always hot (actively used)")
    elif savings_rate > 50:
        print("✅ Excellent memory recycling! Dynarec purging is working well.")
        print(f"   {savings_rate:.1f}% of allocated memory was successfully recycled.")
    elif savings_rate > 20:
        print("✅ Good memory recycling. Dynarec purging is functional.")
        print(f"   {savings_rate:.1f}% of allocated memory was recycled.")
    else:
        print("⚠️  Low memory recycling rate.")
        print(f"   Only {savings_rate:.1f}% of allocated memory was recycled.")
        print("   Consider adjusting BOX64_DYNAREC_* settings for more aggressive purging.")

    print()
    print("=" * 70)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 analyze_memory_savings.py <diagnostic_csv_file>")
        print("Example: python3 analyze_memory_savings.py box64_sleeping_diag_62168.csv")
        sys.exit(1)

    analyze_memory_savings(sys.argv[1])
