#!/usr/bin/env python3
"""
Compare benchmark results against baseline and detect regressions.

Usage:
    python compare_benchmarks.py <baseline_file> <current_file> [--threshold=20]

Tiered thresholds:
    Some benchmarks (e.g., multi-threaded latency) have inherent variance on CI.
    Use --relaxed-threshold and --relaxed-benchmarks to apply a higher threshold
    to specific benchmarks while keeping strict thresholds for others.

    Example:
        --threshold=20 --relaxed-threshold=50 \\
        --relaxed-benchmarks=perf_lockfree_queue_p90_latency,perf_lockfree_queue_p95_latency

Exit codes:
    0 - No regressions detected
    1 - Regressions exceed threshold
    2 - Error (missing files, parse errors)

Requires Python 3.6+
"""

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple

from benchmark_utils import parse_benchmark_file, format_ns


def error_exit(msg: str, github_actions: bool = False) -> None:
    """Print error message and exit with code 2."""
    if github_actions:
        print(f"::error::{msg}")
    print(f"Error: {msg}", file=sys.stderr)
    sys.exit(2)


def compare_benchmarks(baseline: Dict[str, float],
                       current: Dict[str, float],
                       threshold: float,
                       relaxed_threshold: float = None,
                       relaxed_benchmarks: Set[str] = None) -> Tuple[List, List, List, List, List]:
    """
    Compare current results against baseline.

    Args:
        baseline: Dict of benchmark_name -> ns_per_op for baseline
        current: Dict of benchmark_name -> ns_per_op for current
        threshold: Default regression threshold percentage
        relaxed_threshold: Higher threshold for high-variance benchmarks (optional)
        relaxed_benchmarks: Set of benchmark names that use relaxed_threshold (optional)

    Returns:
        (regressions, improvements, unchanged, new_benchmarks, missing_benchmarks)
        regressions/improvements/unchanged: list of (name, baseline_ns, current_ns, delta_percent, threshold_used)
        new_benchmarks: list of benchmark names in current but not baseline
        missing_benchmarks: list of benchmark names in baseline but not current
    """
    regressions = []
    improvements = []
    unchanged = []

    # Default to empty set if not provided
    if relaxed_benchmarks is None:
        relaxed_benchmarks = set()

    # Detect new benchmarks (in current but not in baseline)
    new_benchmarks = [name for name in current if name not in baseline]

    # Detect missing benchmarks (in baseline but not in current)
    missing_benchmarks = [name for name in baseline if name not in current]

    for name, current_ns in current.items():
        if name not in baseline:
            # New benchmark, skip comparison
            continue

        baseline_ns = baseline[name]
        # Skip invalid values (zero or negative would cause division issues or indicate bad data)
        if baseline_ns <= 0 or current_ns <= 0:
            continue

        # Positive delta = regression (slower), negative = improvement (faster)
        delta_percent = ((current_ns - baseline_ns) / baseline_ns) * 100

        # Use relaxed threshold for specified benchmarks
        effective_threshold = threshold
        if relaxed_threshold is not None and name in relaxed_benchmarks:
            effective_threshold = relaxed_threshold

        entry = (name, baseline_ns, current_ns, delta_percent, effective_threshold)

        if delta_percent > effective_threshold:
            regressions.append(entry)
        elif delta_percent < -effective_threshold:
            improvements.append(entry)
        else:
            unchanged.append(entry)

    return regressions, improvements, unchanged, new_benchmarks, missing_benchmarks


def print_report(regressions: List, improvements: List, unchanged: List,
                 new_benchmarks: List, missing_benchmarks: List,
                 threshold: float, relaxed_threshold: float = None,
                 relaxed_benchmarks: Set[str] = None, github_actions: bool = False):
    """Print a formatted comparison report."""

    # Summary
    total = len(regressions) + len(improvements) + len(unchanged)
    print(f"\n{'='*70}")
    if relaxed_threshold and relaxed_benchmarks:
        print(f"BENCHMARK COMPARISON REPORT")
        print(f"  Default threshold: {threshold}%")
        print(f"  Relaxed threshold: {relaxed_threshold}% for {len(relaxed_benchmarks)} benchmark(s)")
    else:
        print(f"BENCHMARK COMPARISON REPORT (threshold: {threshold}%)")
    print(f"{'='*70}")
    print(f"Total benchmarks compared: {total}")
    print(f"  Regressions:  {len(regressions)}")
    print(f"  Improvements: {len(improvements)}")
    print(f"  Unchanged:    {len(unchanged)}")
    if new_benchmarks:
        print(f"  New:          {len(new_benchmarks)}")
    if missing_benchmarks:
        print(f"  Missing:      {len(missing_benchmarks)}")
    print()

    # Regressions (always show)
    if regressions:
        print(f"{'!'*70}")
        print("REGRESSIONS DETECTED (performance got worse)")
        print(f"{'!'*70}")
        print(f"{'Benchmark':<40} {'Baseline':>12} {'Current':>12} {'Delta':>10}")
        print("-" * 76)
        for name, baseline_ns, current_ns, delta, thresh_used in sorted(regressions, key=lambda x: -x[3]):
            thresh_note = f" [>{thresh_used}%]" if thresh_used != threshold else ""
            print(f"{name:<40} {format_ns(baseline_ns):>12} {format_ns(current_ns):>12} {delta:>+9.1f}%{thresh_note}")
            if github_actions:
                print(f"::error::{name} regressed by {delta:.1f}% (threshold: {thresh_used}%)")
        print()

    # Improvements (show if any)
    if improvements:
        print(f"{'-'*70}")
        print("IMPROVEMENTS (performance got better)")
        print(f"{'-'*70}")
        print(f"{'Benchmark':<40} {'Baseline':>12} {'Current':>12} {'Delta':>10}")
        print("-" * 76)
        for name, baseline_ns, current_ns, delta, _ in sorted(improvements, key=lambda x: x[3]):
            print(f"{name:<40} {format_ns(baseline_ns):>12} {format_ns(current_ns):>12} {delta:>+9.1f}%")
        print()

    # New benchmarks
    if new_benchmarks:
        print(f"{'-'*70}")
        print("NEW BENCHMARKS (no baseline for comparison)")
        print(f"{'-'*70}")
        for name in sorted(new_benchmarks):
            print(f"  {name}")
            if github_actions:
                print(f"::notice::New benchmark '{name}' - no baseline for comparison")
        print()

    # Missing benchmarks
    if missing_benchmarks:
        print(f"{'-'*70}")
        print("MISSING BENCHMARKS (in baseline but not in current)")
        print(f"{'-'*70}")
        for name in sorted(missing_benchmarks):
            print(f"  {name}")
            if github_actions:
                print(f"::warning::Benchmark '{name}' missing from current results")
        print()

    # Summary line for GitHub Actions
    if github_actions:
        if regressions:
            print(f"::error::{len(regressions)} benchmark(s) regressed beyond {threshold}% threshold")
        elif improvements:
            print(f"::notice::{len(improvements)} benchmark(s) improved beyond {threshold}%")


def main():
    parser = argparse.ArgumentParser(
        description="Compare benchmark results against baseline",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Simple comparison with 20% threshold
  %(prog)s baseline.txt current.txt --threshold=20

  # Tiered thresholds: 20% default, 50% for high-variance benchmarks
  %(prog)s baseline.txt current.txt --threshold=20 --relaxed-threshold=50 \\
      --relaxed-benchmarks=perf_lockfree_queue_p90_latency,perf_lockfree_queue_p95_latency
        """
    )
    parser.add_argument("baseline", type=Path, help="Baseline results file")
    parser.add_argument("current", type=Path, help="Current results file")
    parser.add_argument("--threshold", type=float, default=20.0,
                        help="Default regression threshold percentage (default: 20)")
    parser.add_argument("--relaxed-threshold", type=float, default=None,
                        help="Higher threshold for high-variance benchmarks")
    parser.add_argument("--relaxed-benchmarks", type=str, default=None,
                        help="Comma-separated list of benchmarks using relaxed threshold")
    parser.add_argument("--github-actions", action="store_true",
                        help="Output GitHub Actions annotations")

    args = parser.parse_args()

    # Validate threshold
    if args.threshold <= 0:
        error_exit("Threshold must be positive", args.github_actions)

    if args.relaxed_threshold is not None and args.relaxed_threshold <= 0:
        error_exit("Relaxed threshold must be positive", args.github_actions)

    # Parse relaxed benchmarks list
    relaxed_benchmarks = None
    if args.relaxed_benchmarks:
        relaxed_benchmarks = set(b.strip() for b in args.relaxed_benchmarks.split(',') if b.strip())

    # Parse both files using shared utility
    baseline = parse_benchmark_file(args.baseline, github_actions=args.github_actions)
    current = parse_benchmark_file(args.current, github_actions=args.github_actions)

    if not baseline:
        error_exit("Baseline file is empty or contains no valid data", args.github_actions)

    if not current:
        error_exit("Current results file is empty or contains no valid data", args.github_actions)

    # Compare
    regressions, improvements, unchanged, new_benchmarks, missing_benchmarks = compare_benchmarks(
        baseline, current, args.threshold,
        relaxed_threshold=args.relaxed_threshold,
        relaxed_benchmarks=relaxed_benchmarks
    )

    # Report
    print_report(regressions, improvements, unchanged, new_benchmarks, missing_benchmarks,
                 args.threshold, relaxed_threshold=args.relaxed_threshold,
                 relaxed_benchmarks=relaxed_benchmarks, github_actions=args.github_actions)

    # Exit with error if regressions found
    if regressions:
        print(f"\nFAILED: {len(regressions)} regression(s) exceed threshold")
        sys.exit(1)
    else:
        print(f"\nPASSED: No regressions beyond threshold")
        sys.exit(0)


if __name__ == "__main__":
    main()
