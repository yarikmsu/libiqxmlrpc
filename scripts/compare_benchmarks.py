#!/usr/bin/env python3
"""
Compare benchmark results against baseline and detect regressions.

Usage:
    python compare_benchmarks.py <baseline_file> <current_file> [--threshold=10]

Exit codes:
    0 - No regressions detected
    1 - Regressions exceed threshold
    2 - Error (missing files, parse errors)

Requires Python 3.6+
"""

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Tuple

from benchmark_utils import parse_benchmark_file, format_ns


def error_exit(msg: str, github_actions: bool = False) -> None:
    """Print error message and exit with code 2."""
    if github_actions:
        print(f"::error::{msg}")
    print(f"Error: {msg}", file=sys.stderr)
    sys.exit(2)


def compare_benchmarks(baseline: Dict[str, float],
                       current: Dict[str, float],
                       threshold: float) -> Tuple[List, List, List, List, List]:
    """
    Compare current results against baseline.

    Returns:
        (regressions, improvements, unchanged, new_benchmarks, missing_benchmarks)
        regressions/improvements/unchanged: list of (name, baseline_ns, current_ns, delta_percent)
        new_benchmarks: list of benchmark names in current but not baseline
        missing_benchmarks: list of benchmark names in baseline but not current
    """
    regressions = []
    improvements = []
    unchanged = []

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

        entry = (name, baseline_ns, current_ns, delta_percent)

        if delta_percent > threshold:
            regressions.append(entry)
        elif delta_percent < -threshold:
            improvements.append(entry)
        else:
            unchanged.append(entry)

    return regressions, improvements, unchanged, new_benchmarks, missing_benchmarks


def print_report(regressions: List, improvements: List, unchanged: List,
                 new_benchmarks: List, missing_benchmarks: List,
                 threshold: float, github_actions: bool = False):
    """Print a formatted comparison report."""

    # Summary
    total = len(regressions) + len(improvements) + len(unchanged)
    print(f"\n{'='*70}")
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
        for name, baseline_ns, current_ns, delta in sorted(regressions, key=lambda x: -x[3]):
            print(f"{name:<40} {format_ns(baseline_ns):>12} {format_ns(current_ns):>12} {delta:>+9.1f}%")
            if github_actions:
                print(f"::error::{name} regressed by {delta:.1f}%")
        print()

    # Improvements (show if any)
    if improvements:
        print(f"{'-'*70}")
        print("IMPROVEMENTS (performance got better)")
        print(f"{'-'*70}")
        print(f"{'Benchmark':<40} {'Baseline':>12} {'Current':>12} {'Delta':>10}")
        print("-" * 76)
        for name, baseline_ns, current_ns, delta in sorted(improvements, key=lambda x: x[3]):
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
        description="Compare benchmark results against baseline"
    )
    parser.add_argument("baseline", type=Path, help="Baseline results file")
    parser.add_argument("current", type=Path, help="Current results file")
    parser.add_argument("--threshold", type=float, default=10.0,
                        help="Regression threshold percentage (default: 10)")
    parser.add_argument("--github-actions", action="store_true",
                        help="Output GitHub Actions annotations")

    args = parser.parse_args()

    # Validate threshold
    if args.threshold <= 0:
        error_exit("Threshold must be positive", args.github_actions)

    # Parse both files using shared utility
    baseline = parse_benchmark_file(args.baseline, github_actions=args.github_actions)
    current = parse_benchmark_file(args.current, github_actions=args.github_actions)

    if not baseline:
        error_exit("Baseline file is empty or contains no valid data", args.github_actions)

    if not current:
        error_exit("Current results file is empty or contains no valid data", args.github_actions)

    # Compare
    regressions, improvements, unchanged, new_benchmarks, missing_benchmarks = compare_benchmarks(
        baseline, current, args.threshold
    )

    # Report
    print_report(regressions, improvements, unchanged, new_benchmarks, missing_benchmarks,
                 args.threshold, github_actions=args.github_actions)

    # Exit with error if regressions found
    if regressions:
        print(f"\nFAILED: {len(regressions)} regression(s) exceed {args.threshold}% threshold")
        sys.exit(1)
    else:
        print(f"\nPASSED: No regressions beyond {args.threshold}% threshold")
        sys.exit(0)


if __name__ == "__main__":
    main()
