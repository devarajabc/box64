#!/usr/bin/env python3
"""
3D Locality Analysis for Box64 Dynarec Blocks

Analyzes three dimensions:
- Spatial (x64_addr): WHERE in x86-64 address space
- Temporal (tick/age): WHEN last executed
- Frequency (hot): HOW MANY times executed

This helps understand:
- Are we purging high-frequency (hot) blocks? (LRU vs LFU tradeoff)
- Do hot blocks cluster spatially? (code locality)
- What's the correlation between recency and frequency?
"""

import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from typing import List, Dict, Tuple, Optional
import statistics

@dataclass
class BlockEvent:
    """Represents a single block event from the log."""
    event_type: str      # PURGE_SUCCESS, PURGE_SKIP, PURGE_BLOCKED
    x64_addr: int        # x86-64 address
    hot: int             # execution count
    tick: int            # last used tick
    age: int             # current_tick - tick
    threshold: int       # purge threshold
    in_used: int = 0     # in_used flag (for BLOCKED/SKIP)


def parse_log_line(line: str) -> Optional[BlockEvent]:
    """Parse a single log line and extract block data."""

    # Common patterns
    addr_match = re.search(r'x64_addr=(0x[0-9a-fA-F]+)', line)
    hot_match = re.search(r'hot=(\d+)', line)
    tick_match = re.search(r'last_used_tick=(\d+)', line)
    age_match = re.search(r'current_age=(\d+)', line)
    threshold_match = re.search(r'min_age_required=(\d+)', line)
    in_used_match = re.search(r'in_used=(\d+)', line)

    if not addr_match or not hot_match:
        return None

    x64_addr = int(addr_match.group(1), 16)
    hot = int(hot_match.group(1))
    tick = int(tick_match.group(1)) if tick_match else 0
    age = int(age_match.group(1)) if age_match else 0
    threshold = int(threshold_match.group(1)) if threshold_match else 0
    in_used = int(in_used_match.group(1)) if in_used_match else 0

    if '[PURGE SUCCESS]' in line:
        return BlockEvent('PURGE_SUCCESS', x64_addr, hot, tick, age, threshold)
    elif '[PURGE SKIP]' in line:
        return BlockEvent('PURGE_SKIP', x64_addr, hot, tick, age, threshold, in_used)
    elif '[PURGE BLOCKED]' in line:
        return BlockEvent('PURGE_BLOCKED', x64_addr, hot, tick, age, threshold, in_used)

    return None


def analyze_log(filename: str):
    """Main analysis function."""

    print(f"Analyzing {filename}...\n")

    # Collect all events
    events: List[BlockEvent] = []
    success_events: List[BlockEvent] = []
    skip_events: List[BlockEvent] = []
    blocked_events: List[BlockEvent] = []

    line_count = 0

    print("Parsing log file...")
    with open(filename, 'r') as f:
        for line in f:
            line_count += 1
            if line_count % 500000 == 0:
                print(f"  {line_count // 1000}K lines...")

            event = parse_log_line(line)
            if event:
                events.append(event)
                if event.event_type == 'PURGE_SUCCESS':
                    success_events.append(event)
                elif event.event_type == 'PURGE_SKIP':
                    skip_events.append(event)
                elif event.event_type == 'PURGE_BLOCKED':
                    blocked_events.append(event)

    print(f"Parsed {line_count:,} lines, {len(events):,} block events\n")

    if not events:
        print("No block events found!")
        return

    # ========== REPORT ==========

    print("=" * 80)
    print("3D LOCALITY ANALYSIS REPORT")
    print("=" * 80)

    # 1. Event Summary
    print("\n1. EVENT SUMMARY")
    print("-" * 80)
    print(f"Total events:    {len(events):,}")
    print(f"  PURGE_SUCCESS: {len(success_events):,} ({len(success_events)/len(events)*100:.1f}%)")
    print(f"  PURGE_SKIP:    {len(skip_events):,} ({len(skip_events)/len(events)*100:.1f}%)")
    print(f"  PURGE_BLOCKED: {len(blocked_events):,} ({len(blocked_events)/len(events)*100:.1f}%)")

    # 2. HOT (Frequency) Analysis
    print("\n2. FREQUENCY ANALYSIS (hot counter)")
    print("-" * 80)

    all_hot = [e.hot for e in events]
    success_hot = [e.hot for e in success_events]
    skip_hot = [e.hot for e in skip_events]
    blocked_hot = [e.hot for e in blocked_events]

    print(f"\nAll blocks:")
    print(f"  Mean hot:   {statistics.mean(all_hot):,.1f}")
    print(f"  Median hot: {statistics.median(all_hot):,.0f}")
    print(f"  Max hot:    {max(all_hot):,}")
    print(f"  Min hot:    {min(all_hot):,}")

    if success_hot:
        print(f"\nPURGED blocks (SUCCESS):")
        print(f"  Mean hot:   {statistics.mean(success_hot):,.1f}")
        print(f"  Median hot: {statistics.median(success_hot):,.0f}")
        print(f"  Max hot:    {max(success_hot):,}")

    if skip_hot:
        print(f"\nSKIPPED blocks (too young):")
        print(f"  Mean hot:   {statistics.mean(skip_hot):,.1f}")
        print(f"  Median hot: {statistics.median(skip_hot):,.0f}")
        print(f"  Max hot:    {max(skip_hot):,}")

    if blocked_hot:
        print(f"\nBLOCKED blocks (in_used):")
        print(f"  Mean hot:   {statistics.mean(blocked_hot):,.1f}")
        print(f"  Median hot: {statistics.median(blocked_hot):,.0f}")
        print(f"  Max hot:    {max(blocked_hot):,}")

    # 3. Hot Distribution (buckets)
    print("\n3. HOT DISTRIBUTION (execution frequency buckets)")
    print("-" * 80)

    def bucket_distribution(hot_values, label):
        if not hot_values:
            return
        buckets = {
            '0 (never run after creation)': 0,
            '1-10': 0,
            '11-100': 0,
            '101-1000': 0,
            '1001-10000': 0,
            '10001-100000': 0,
            '100001+': 0
        }
        for h in hot_values:
            if h == 0:
                buckets['0 (never run after creation)'] += 1
            elif h <= 10:
                buckets['1-10'] += 1
            elif h <= 100:
                buckets['11-100'] += 1
            elif h <= 1000:
                buckets['101-1000'] += 1
            elif h <= 10000:
                buckets['1001-10000'] += 1
            elif h <= 100000:
                buckets['10001-100000'] += 1
            else:
                buckets['100001+'] += 1

        print(f"\n{label}:")
        total = len(hot_values)
        for bucket, count in buckets.items():
            pct = count / total * 100
            bar = '#' * int(pct / 2)
            print(f"  {bucket:30s}: {count:8,} ({pct:5.1f}%) {bar}")

    bucket_distribution(all_hot, "All blocks")
    bucket_distribution(success_hot, "PURGED blocks")
    bucket_distribution(skip_hot, "SKIPPED blocks")

    # 4. LRU vs LFU Analysis
    print("\n4. LRU vs LFU TRADEOFF ANALYSIS")
    print("-" * 80)

    if success_hot:
        # Are we purging hot blocks?
        high_hot_purged = sum(1 for h in success_hot if h > 1000)
        very_hot_purged = sum(1 for h in success_hot if h > 10000)

        print(f"\nBlocks purged with hot > 1000:  {high_hot_purged:,} ({high_hot_purged/len(success_hot)*100:.1f}%)")
        print(f"Blocks purged with hot > 10000: {very_hot_purged:,} ({very_hot_purged/len(success_hot)*100:.1f}%)")

        if high_hot_purged > 0:
            print(f"\n  WARNING: LRU is evicting frequently-used blocks!")
            print(f"  These blocks ran many times but weren't used recently.")
            print(f"  LFU would have kept these blocks.")
        else:
            print(f"\n  GOOD: LRU is mostly evicting low-frequency blocks.")

    # 5. Spatial Locality Analysis
    print("\n5. SPATIAL LOCALITY ANALYSIS (x64_addr)")
    print("-" * 80)

    all_addrs = [e.x64_addr for e in events]
    addr_min = min(all_addrs)
    addr_max = max(all_addrs)
    addr_range = addr_max - addr_min

    print(f"\nAddress range:")
    print(f"  Min:   0x{addr_min:x}")
    print(f"  Max:   0x{addr_max:x}")
    print(f"  Range: 0x{addr_range:x} ({addr_range / (1024*1024):.1f} MB)")

    # Bin addresses into 4KB pages
    PAGE_SIZE = 4096
    page_hot_sum = defaultdict(int)
    page_block_count = defaultdict(int)
    page_events = defaultdict(list)

    for e in events:
        page = e.x64_addr // PAGE_SIZE
        page_hot_sum[page] += e.hot
        page_block_count[page] += 1
        page_events[page].append(e)

    print(f"\nPage-level analysis (4KB pages):")
    print(f"  Total unique pages: {len(page_hot_sum):,}")

    # Find hottest pages
    pages_by_hot = sorted(page_hot_sum.items(), key=lambda x: x[1], reverse=True)

    print(f"\n  Top 10 hottest pages (by cumulative hot):")
    for i, (page, total_hot) in enumerate(pages_by_hot[:10], 1):
        count = page_block_count[page]
        avg_hot = total_hot / count
        print(f"    {i:2d}. Page 0x{page*PAGE_SIZE:x}: {count:3d} blocks, total_hot={total_hot:,}, avg_hot={avg_hot:,.0f}")

    # Spatial clustering: are hot blocks close together?
    if success_events:
        success_addrs = sorted([e.x64_addr for e in success_events])
        success_addr_diffs = [success_addrs[i+1] - success_addrs[i] for i in range(len(success_addrs)-1)]
        if success_addr_diffs:
            print(f"\n  Purged block address gaps:")
            print(f"    Mean gap:   {statistics.mean(success_addr_diffs):,.0f} bytes")
            print(f"    Median gap: {statistics.median(success_addr_diffs):,.0f} bytes")
            same_page = sum(1 for d in success_addr_diffs if d < PAGE_SIZE)
            print(f"    Same page:  {same_page:,} ({same_page/len(success_addr_diffs)*100:.1f}%)")

    # 6. Temporal-Frequency Correlation
    print("\n6. TEMPORAL-FREQUENCY CORRELATION (hot vs age)")
    print("-" * 80)

    # Categorize blocks
    categories = {
        'hot_recent': [],    # high hot, low age (active hot - shouldn't purge)
        'hot_stale': [],     # high hot, high age (flash hot - init code?)
        'cold_recent': [],   # low hot, low age (new blocks)
        'cold_stale': []     # low hot, high age (truly cold - good purge targets)
    }

    HOT_THRESHOLD = 100
    AGE_THRESHOLD = 500  # relative to purge threshold

    for e in events:
        is_hot = e.hot > HOT_THRESHOLD
        is_stale = e.age > AGE_THRESHOLD

        if is_hot and not is_stale:
            categories['hot_recent'].append(e)
        elif is_hot and is_stale:
            categories['hot_stale'].append(e)
        elif not is_hot and not is_stale:
            categories['cold_recent'].append(e)
        else:
            categories['cold_stale'].append(e)

    print(f"\nBlock categories (hot>{HOT_THRESHOLD}, age>{AGE_THRESHOLD}):")
    for cat, blocks in categories.items():
        pct = len(blocks) / len(events) * 100
        print(f"  {cat:15s}: {len(blocks):8,} ({pct:5.1f}%)")

    # What categories are being purged?
    if success_events:
        print(f"\nCategories of PURGED blocks:")
        for cat_name, threshold_check in [
            ('hot_stale (flash-hot)', lambda e: e.hot > HOT_THRESHOLD),
            ('cold_stale (ideal)', lambda e: e.hot <= HOT_THRESHOLD)
        ]:
            count = sum(1 for e in success_events if threshold_check(e))
            pct = count / len(success_events) * 100
            print(f"  {cat_name:20s}: {count:8,} ({pct:5.1f}%)")

    # 7. Summary and Insights
    print("\n" + "=" * 80)
    print("SUMMARY AND INSIGHTS")
    print("=" * 80)

    # Calculate key metrics
    avg_purged_hot = statistics.mean(success_hot) if success_hot else 0
    avg_skip_hot = statistics.mean(skip_hot) if skip_hot else 0

    print(f"\n Key Findings:")
    print(f"\n 1. FREQUENCY (hot):")
    print(f"    - Average hot of PURGED blocks: {avg_purged_hot:,.1f}")
    print(f"    - Average hot of SKIPPED blocks: {avg_skip_hot:,.1f}")

    if avg_purged_hot < avg_skip_hot:
        print(f"    GOOD: LRU purges less-frequently-used blocks")
    else:
        print(f"    CONCERN: LRU purges more-frequently-used blocks than it skips")

    print(f"\n 2. SPATIAL LOCALITY:")
    print(f"    - {len(page_hot_sum):,} unique 4KB pages contain blocks")
    top_10_hot = sum(h for _, h in pages_by_hot[:10])
    total_hot = sum(all_hot)
    print(f"    - Top 10 pages account for {top_10_hot/total_hot*100:.1f}% of total hot")

    if top_10_hot / total_hot > 0.5:
        print(f"    GOOD: Hot blocks are spatially clustered")
    else:
        print(f"    Note: Hot blocks are distributed across many pages")

    print(f"\n 3. LRU EFFECTIVENESS:")
    cold_purged = sum(1 for e in success_events if e.hot <= HOT_THRESHOLD)
    if success_events:
        print(f"    - {cold_purged/len(success_events)*100:.1f}% of purged blocks had low frequency (hot<={HOT_THRESHOLD})")

    # 8. Export data for visualization
    print(f"\n" + "=" * 80)
    print("DATA EXPORT")
    print("=" * 80)

    csv_filename = filename.rsplit('.', 1)[0] + '_locality.csv'
    print(f"\nExporting to {csv_filename}...")

    with open(csv_filename, 'w') as f:
        f.write("event_type,x64_addr,hot,tick,age,threshold\n")
        for e in events:
            f.write(f"{e.event_type},{e.x64_addr},{e.hot},{e.tick},{e.age},{e.threshold}\n")

    print(f"Exported {len(events):,} events")
    print(f"\nYou can visualize this with:")
    print(f"  - 3D scatter: x64_addr vs age vs hot")
    print(f"  - Heatmap: x64_addr bins vs hot bins")
    print(f"  - Time series: hot distribution over tick")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python analyze_locality_3d.py <purge_log_file>")
        print("")
        print("To generate log:")
        print("  BOX64_DYNAREC_PURGE=1 BOX64_DYNAREC_PURGE_FILE=1 BOX64_DYNAREC_PURGE_AGE=256 box64 ./program")
        print("")
        print("This creates box64_purge_<pid>.log in current directory")
        print("")
        print("Example:")
        print("  python analyze_locality_3d.py box64_purge_12345.log")
        sys.exit(1)

    analyze_log(sys.argv[1])
