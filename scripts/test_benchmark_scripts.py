#!/usr/bin/env python3
"""
Unit tests for benchmark comparison scripts.

Tests cover:
- benchmark_utils.py: parse_benchmark_file(), format_ns()
- compare_benchmarks.py: compare_benchmarks(), tiered thresholds
- select_minimum_results.py: select_minimum()

Run with: python -m pytest scripts/test_benchmark_scripts.py -v
Or:       python scripts/test_benchmark_scripts.py
"""

import sys
import tempfile
import unittest
from pathlib import Path

# Add scripts directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from benchmark_utils import parse_benchmark_file, format_ns
from compare_benchmarks import compare_benchmarks
from select_minimum_results import select_minimum


class TestFormatNs(unittest.TestCase):
    """Test format_ns() function."""

    def test_nanoseconds(self):
        """Values under 1000 display as ns."""
        self.assertEqual(format_ns(1.5), "1.50 ns")
        self.assertEqual(format_ns(999.99), "999.99 ns")
        self.assertEqual(format_ns(0.42), "0.42 ns")

    def test_microseconds(self):
        """Values 1000-999999 display as µs."""
        self.assertEqual(format_ns(1000), "1.00 µs")
        self.assertEqual(format_ns(1500), "1.50 µs")
        self.assertEqual(format_ns(999999), "1000.00 µs")

    def test_milliseconds(self):
        """Values >= 1000000 display as ms."""
        self.assertEqual(format_ns(1000000), "1.00 ms")
        self.assertEqual(format_ns(1500000), "1.50 ms")
        self.assertEqual(format_ns(10000000), "10.00 ms")

    def test_zero_value(self):
        """Zero value displays correctly."""
        self.assertEqual(format_ns(0.0), "0.00 ns")


class TestParseBenchmarkFile(unittest.TestCase):
    """Test parse_benchmark_file() function."""

    def test_parse_valid_file(self):
        """Parse a valid benchmark file."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("# Comment line\n")
            f.write("\n")
            f.write("benchmark_a: 100.5\n")
            f.write("benchmark_b: 200.0\n")
            filepath = Path(f.name)
        try:
            result = parse_benchmark_file(filepath, exit_on_error=False, quiet=True)
            self.assertEqual(result, {"benchmark_a": 100.5, "benchmark_b": 200.0})
        finally:
            filepath.unlink()

    def test_skip_comments_and_empty_lines(self):
        """Comments and empty lines are ignored."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("# Header comment\n")
            f.write("# Another comment\n")
            f.write("\n")
            f.write("  \n")
            f.write("test: 42.0\n")
            filepath = Path(f.name)
        try:
            result = parse_benchmark_file(filepath, exit_on_error=False, quiet=True)
            self.assertEqual(result, {"test": 42.0})
        finally:
            filepath.unlink()

    def test_skip_malformed_lines(self):
        """Malformed lines are skipped with warning."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("valid: 100.0\n")
            f.write("invalid_no_colon\n")
            f.write("invalid_value: not_a_number\n")
            f.write(": 50.0\n")  # Empty name
            f.write("also_valid: 200.0\n")
            filepath = Path(f.name)
        try:
            result = parse_benchmark_file(filepath, exit_on_error=False, quiet=True)
            self.assertEqual(result, {"valid": 100.0, "also_valid": 200.0})
        finally:
            filepath.unlink()

    def test_nonexistent_file(self):
        """Non-existent file returns empty dict when exit_on_error=False."""
        result = parse_benchmark_file(Path("/nonexistent/file.txt"),
                                       exit_on_error=False, quiet=True)
        self.assertEqual(result, {})

    def test_empty_file(self):
        """Empty file returns empty dict."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("")
            filepath = Path(f.name)
        try:
            result = parse_benchmark_file(filepath, exit_on_error=False, quiet=True)
            self.assertEqual(result, {})
        finally:
            filepath.unlink()

    def test_negative_values_allowed(self):
        """Negative values are parsed (with warning)."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("negative_test: -50.0\n")
            filepath = Path(f.name)
        try:
            result = parse_benchmark_file(filepath, exit_on_error=False, quiet=True)
            self.assertEqual(result, {"negative_test": -50.0})
        finally:
            filepath.unlink()

    def test_duplicate_names_last_wins(self):
        """Duplicate benchmark names use the last value."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("test: 100.0\n")
            f.write("test: 200.0\n")
            f.write("test: 150.0\n")
            filepath = Path(f.name)
        try:
            result = parse_benchmark_file(filepath, exit_on_error=False, quiet=True)
            self.assertEqual(result, {"test": 150.0})
        finally:
            filepath.unlink()


class TestCompareBenchmarks(unittest.TestCase):
    """Test compare_benchmarks() function."""

    def test_detect_regression(self):
        """Regression detected when current > baseline * (1 + threshold/100)."""
        baseline = {"test": 100.0}
        current = {"test": 130.0}  # 30% slower

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        self.assertEqual(len(regressions), 1)
        self.assertEqual(regressions[0][0], "test")
        self.assertAlmostEqual(regressions[0][3], 30.0)  # 30% delta
        self.assertEqual(len(improvements), 0)
        self.assertEqual(len(unchanged), 0)

    def test_detect_improvement(self):
        """Improvement detected when current < baseline * (1 - threshold/100)."""
        baseline = {"test": 100.0}
        current = {"test": 70.0}  # 30% faster

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        self.assertEqual(len(improvements), 1)
        self.assertEqual(improvements[0][0], "test")
        self.assertAlmostEqual(improvements[0][3], -30.0)  # -30% delta
        self.assertEqual(len(regressions), 0)
        self.assertEqual(len(unchanged), 0)

    def test_unchanged_within_threshold(self):
        """Values within threshold are unchanged."""
        baseline = {"test": 100.0}
        current = {"test": 110.0}  # 10% slower, within 20% threshold

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        self.assertEqual(len(unchanged), 1)
        self.assertEqual(len(regressions), 0)
        self.assertEqual(len(improvements), 0)

    def test_boundary_at_threshold(self):
        """Value exactly at threshold boundary is unchanged."""
        baseline = {"test": 100.0}
        current = {"test": 120.0}  # Exactly 20%

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        # At boundary (delta == threshold), should be unchanged
        self.assertEqual(len(unchanged), 1)
        self.assertEqual(len(regressions), 0)

    def test_zero_delta_is_unchanged(self):
        """Exactly 0% delta is unchanged."""
        baseline = {"test": 100.0}
        current = {"test": 100.0}  # Same value

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        self.assertEqual(len(unchanged), 1)
        self.assertAlmostEqual(unchanged[0][3], 0.0)  # 0% delta

    def test_detect_new_benchmarks(self):
        """New benchmarks (in current but not baseline) are detected."""
        baseline = {"old": 100.0}
        current = {"old": 100.0, "new": 50.0}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        self.assertEqual(new_bench, ["new"])
        self.assertEqual(len(missing), 0)

    def test_detect_missing_benchmarks(self):
        """Missing benchmarks (in baseline but not current) are detected."""
        baseline = {"old": 100.0, "removed": 50.0}
        current = {"old": 100.0}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        self.assertEqual(missing, ["removed"])
        self.assertEqual(len(new_bench), 0)

    def test_skip_zero_baseline(self):
        """Zero baseline value is skipped."""
        baseline = {"zero_baseline": 0.0, "normal": 100.0}
        current = {"zero_baseline": 50.0, "normal": 100.0}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        # Only "normal" should be compared
        self.assertEqual(len(unchanged), 1)
        self.assertEqual(unchanged[0][0], "normal")

    def test_skip_zero_current(self):
        """Zero current value is skipped."""
        baseline = {"zero_current": 100.0, "normal": 100.0}
        current = {"zero_current": 0.0, "normal": 100.0}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        # Only "normal" should be compared
        self.assertEqual(len(unchanged), 1)
        self.assertEqual(unchanged[0][0], "normal")

    def test_skip_negative_values(self):
        """Negative baseline or current values are skipped."""
        baseline = {"neg_baseline": -50.0, "neg_current": 100.0, "normal": 100.0}
        current = {"neg_baseline": 100.0, "neg_current": -50.0, "normal": 100.0}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        # Only "normal" should be compared
        self.assertEqual(len(unchanged), 1)
        self.assertEqual(unchanged[0][0], "normal")

    def test_tiered_threshold_relaxed_benchmark(self):
        """Relaxed threshold applies to specified benchmarks."""
        baseline = {"strict_test": 100.0, "relaxed_test": 100.0}
        current = {"strict_test": 150.0, "relaxed_test": 150.0}  # Both 50% slower

        relaxed_set = {"relaxed_test"}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0,
            relaxed_threshold=100.0, relaxed_benchmarks=relaxed_set
        )

        # strict_test should regress (50% > 20%)
        # relaxed_test should be unchanged (50% < 100%)
        self.assertEqual(len(regressions), 1)
        self.assertEqual(regressions[0][0], "strict_test")

        self.assertEqual(len(unchanged), 1)
        self.assertEqual(unchanged[0][0], "relaxed_test")

    def test_tiered_threshold_reports_effective_threshold(self):
        """The effective threshold used is reported in results."""
        baseline = {"relaxed_test": 100.0}
        current = {"relaxed_test": 250.0}  # 150% slower, beyond relaxed threshold

        relaxed_set = {"relaxed_test"}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0,
            relaxed_threshold=100.0, relaxed_benchmarks=relaxed_set
        )

        self.assertEqual(len(regressions), 1)
        # Check that the effective threshold (100.0) is in the result tuple
        self.assertEqual(regressions[0][4], 100.0)

    def test_relaxed_threshold_without_benchmarks_list(self):
        """Relaxed threshold with None/empty benchmarks uses default for all."""
        baseline = {"test": 100.0}
        current = {"test": 130.0}  # 30% slower

        # relaxed_threshold provided but relaxed_benchmarks is None
        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0,
            relaxed_threshold=100.0, relaxed_benchmarks=None
        )

        # Should regress using default threshold (30% > 20%)
        self.assertEqual(len(regressions), 1)
        # Effective threshold should be default (20.0)
        self.assertEqual(regressions[0][4], 20.0)

    def test_empty_baseline(self):
        """Empty baseline results in all new benchmarks."""
        baseline = {}
        current = {"test": 100.0}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        self.assertEqual(new_bench, ["test"])
        self.assertEqual(len(regressions), 0)
        self.assertEqual(len(improvements), 0)
        self.assertEqual(len(unchanged), 0)

    def test_empty_current(self):
        """Empty current results in all missing benchmarks."""
        baseline = {"test": 100.0}
        current = {}

        regressions, improvements, unchanged, new_bench, missing = compare_benchmarks(
            baseline, current, threshold=20.0
        )

        self.assertEqual(missing, ["test"])
        self.assertEqual(len(regressions), 0)
        self.assertEqual(len(new_bench), 0)


class TestSelectMinimum(unittest.TestCase):
    """Test select_minimum() function."""

    def test_select_minimum_across_runs(self):
        """Minimum value is selected for each benchmark."""
        runs = [
            {"a": 100.0, "b": 200.0},
            {"a": 90.0, "b": 250.0},
            {"a": 110.0, "b": 180.0},
        ]

        result = select_minimum(runs)

        self.assertEqual(result["a"], 90.0)
        self.assertEqual(result["b"], 180.0)

    def test_missing_benchmark_in_some_runs(self):
        """Benchmark missing from some runs still gets minimum of available values."""
        runs = [
            {"a": 100.0, "b": 200.0},
            {"a": 90.0},  # b missing
            {"a": 110.0, "b": 180.0},
        ]

        result = select_minimum(runs)

        self.assertEqual(result["a"], 90.0)
        self.assertEqual(result["b"], 180.0)  # min of 200 and 180

    def test_empty_runs_list(self):
        """Empty runs list returns empty result."""
        result = select_minimum([])
        self.assertEqual(result, {})

    def test_single_run(self):
        """Single run returns same values."""
        runs = [{"a": 100.0, "b": 200.0}]

        result = select_minimum(runs)

        self.assertEqual(result, {"a": 100.0, "b": 200.0})

    def test_benchmark_in_only_one_run(self):
        """Benchmark appearing in only one run is included."""
        runs = [
            {"common": 100.0, "unique1": 50.0},
            {"common": 90.0, "unique2": 60.0},
        ]

        result = select_minimum(runs)

        self.assertEqual(result["common"], 90.0)
        self.assertEqual(result["unique1"], 50.0)
        self.assertEqual(result["unique2"], 60.0)

    def test_all_empty_runs(self):
        """List of empty dicts returns empty result."""
        runs = [{}, {}, {}]

        result = select_minimum(runs)

        self.assertEqual(result, {})


if __name__ == "__main__":
    unittest.main(verbosity=2)
