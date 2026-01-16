#!/usr/bin/env python3
"""
Select minimum (best) result for each benchmark across multiple runs.

Usage:
    python select_minimum_results.py run1.txt run2.txt run3.txt -o output.txt

The minimum represents the best achievable performance, robust to transient
slowdowns from system load, CPU throttling, or cache effects.

Requires Python 3.6+
"""

import argparse
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List

from benchmark_utils import parse_benchmark_file


def select_minimum(runs: List[Dict[str, float]]) -> Dict[str, float]:
    """Select minimum value for each benchmark across all runs."""
    if not runs:
        return {}

    # Get all benchmark names from all runs
    all_names = set().union(*(run.keys() for run in runs))

    result = {}
    negative_found = []
    for name in all_names:
        values = [run[name] for run in runs if name in run]
        if values:
            min_val = min(values)
            if min_val < 0:
                negative_found.append(name)
            result[name] = min_val

    if negative_found:
        print(f"Warning: {len(negative_found)} benchmark(s) have negative values: {', '.join(negative_found[:3])}"
              + ("..." if len(negative_found) > 3 else ""))

    return result


def write_results(results: Dict[str, float], filepath: Path, num_runs: int):
    """Write results to file in baseline format."""
    with open(filepath, 'w') as f:
        f.write("# Performance Results - libiqxmlrpc\n")
        f.write(f"# Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"# Method: minimum of {num_runs} runs\n")
        f.write(f"# Total benchmarks: {len(results)}\n")
        f.write("#\n")
        f.write("# Format: benchmark_name: ns_per_op\n")
        f.write("#\n")

        for name in sorted(results.keys()):
            f.write(f"{name}: {results[name]:.2f}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Select minimum benchmark results across multiple runs"
    )
    parser.add_argument("files", nargs="+", type=Path,
                        help="Result files to compare")
    parser.add_argument("-o", "--output", type=Path, required=True,
                        help="Output file for minimum results")
    parser.add_argument("--github-actions", action="store_true",
                        help="Output GitHub Actions annotations")

    args = parser.parse_args()

    # Parse all runs using shared utility
    runs = []
    for filepath in args.files:
        results = parse_benchmark_file(filepath, exit_on_error=False,
                                        github_actions=args.github_actions)
        if results:
            runs.append(results)
            print(f"Loaded {len(results)} benchmarks from {filepath}")
        else:
            print(f"Warning: Could not load benchmarks from {filepath} (file may be empty, missing, or malformed)")
            if args.github_actions:
                print(f"::warning::Could not load benchmarks from {filepath}")

    if not runs:
        if args.github_actions:
            print("::error::No valid benchmark data found in any input file")
        print("Error: No valid benchmark data found", file=sys.stderr)
        sys.exit(2)

    if len(runs) == 1:
        print("Warning: Only 1 run provided - minimum selection has no effect")
        if args.github_actions:
            print("::warning::Only 1 benchmark run provided - recommend 3+ runs for stability")

    # Select minimum
    minimum = select_minimum(runs)
    print(f"Selected minimum across {len(runs)} runs for {len(minimum)} benchmarks")

    # Write output
    write_results(minimum, args.output, len(runs))
    print(f"Results written to {args.output}")


if __name__ == "__main__":
    main()
