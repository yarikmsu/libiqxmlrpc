#!/usr/bin/env python3
"""
Shared utilities for benchmark scripts.

This module provides common functionality for parsing and handling
benchmark result files.

Requires Python 3.6+
"""

import sys
from pathlib import Path
from typing import Dict

# Maximum lines to read from a file (prevents memory exhaustion)
MAX_LINES = 10000


def parse_benchmark_file(filepath: Path,
                         exit_on_error: bool = True,
                         quiet: bool = False,
                         github_actions: bool = False) -> Dict[str, float]:
    """
    Parse a benchmark file into a dict of benchmark_name -> ns_per_op.

    Args:
        filepath: Path to the benchmark results file
        exit_on_error: If True, exit with code 2 on errors; if False, return empty dict
        quiet: If True, suppress warning messages
        github_actions: If True, print GitHub Actions annotations

    Returns:
        Dictionary mapping benchmark names to nanoseconds per operation
    """
    results = {}
    try:
        with open(filepath) as f:
            for i, line in enumerate(f):
                if i >= MAX_LINES:
                    if not quiet:
                        print(f"Warning: Truncated {filepath} after {MAX_LINES} lines")
                        if github_actions:
                            print(f"::warning::Truncated {filepath} after {MAX_LINES} lines")
                    break
                line = line.strip()
                # Skip comments and empty lines
                if not line or line.startswith('#'):
                    continue
                # Parse "benchmark_name: value" format
                if ':' in line:
                    name, value = line.split(':', 1)
                    name = name.strip()
                    if not name:
                        if not quiet:
                            print(f"Warning: Skipping line {i+1} with empty name in {filepath}: {line}")
                        continue
                    try:
                        parsed_value = float(value.strip())
                        if parsed_value < 0:
                            if not quiet:
                                print(f"Warning: Negative value at line {i+1} in {filepath}: {line}")
                                if github_actions:
                                    print(f"::warning::Negative benchmark value in {filepath} line {i+1}")
                        results[name] = parsed_value
                    except ValueError:
                        if not quiet:
                            print(f"Warning: Skipping malformed line {i+1} in {filepath}: {line}")
                        continue
    except FileNotFoundError:
        if not quiet:
            if github_actions:
                print(f"::error::File not found: {filepath}")
            print(f"Error: File not found: {filepath}", file=sys.stderr)
        if exit_on_error:
            sys.exit(2)
    except (IOError, OSError) as e:
        if not quiet:
            if github_actions:
                print(f"::error::Failed to read {filepath}: {e}")
            print(f"Error: Failed to read {filepath}: {e}", file=sys.stderr)
        if exit_on_error:
            sys.exit(2)
    return results


def format_ns(ns: float) -> str:
    """Format nanoseconds in human-readable form."""
    if ns >= 1_000_000:
        return f"{ns/1_000_000:.2f} ms"
    elif ns >= 1_000:
        return f"{ns/1_000:.2f} µs"
    else:
        return f"{ns:.2f} ns"
