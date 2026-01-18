#!/bin/bash
# Compare benchmark performance between current branch and a base branch.
# Must be run from the repository root directory.
#
# Thresholds:
#   - Default: 10% for most benchmarks
#   - Tier 1:  40% for perf_lockfree_queue_p90_latency (thread scheduling variance)
#
# Usage:
#   ./scripts/local_benchmark_compare.sh [threshold] [base_branch]
#
# Arguments:
#   threshold   - Default regression threshold percentage (default: 10)
#   base_branch - Branch to compare against (default: master)
#
# Example:
#   ./scripts/local_benchmark_compare.sh           # 10% threshold, compare to master
#   ./scripts/local_benchmark_compare.sh 5         # 5% threshold (stricter)
#   ./scripts/local_benchmark_compare.sh 15 main   # Compare to 'main' branch

set -euo pipefail

# Check required dependencies
if ! command -v bc &>/dev/null; then
    echo "Error: 'bc' is required but not installed."
    echo "       Install with: apt-get install bc (Linux) or brew install bc (macOS)"
    exit 2
fi

THRESHOLD=${1:-10}
BASE_BRANCH=${2:-master}
BRANCH=$(git rev-parse --abbrev-ref HEAD)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TMP_SCRIPTS="/tmp/benchmark_scripts_$$"

# Tier 1: High-variance benchmarks due to thread scheduling
RELAXED_THRESHOLD=40
RELAXED_BENCHMARKS="perf_lockfree_queue_p90_latency"

# Cross-platform nproc
get_nproc() {
    nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4
}
NPROC=$(get_nproc)

# Run benchmarks 5 times and select minimum
# Usage: run_benchmarks <name> <label>
run_benchmarks() {
    local name="$1"
    local label="$2"
    for i in 1 2 3 4 5; do
        echo "--- $label run $i/5 ---"
        ./tests/performance-test
        cp performance_baseline.txt "${name}_run_$i.txt"
    done
    python3 "$TMP_SCRIPTS/select_minimum_results.py" \
        "${name}_run_1.txt" "${name}_run_2.txt" "${name}_run_3.txt" "${name}_run_4.txt" "${name}_run_5.txt" \
        -o "${name}_results.txt"
}

# Ensure we're in the repo root
cd "$REPO_ROOT"

# Validate threshold is a positive number (no leading zeros except for "0.x")
if ! [[ "$THRESHOLD" =~ ^(0|[1-9][0-9]*)(\.[0-9]+)?$ ]]; then
    echo "Error: Threshold must be a positive number, got: $THRESHOLD"
    exit 1
fi
if [ "$(echo "$THRESHOLD <= 0" | bc -l)" -eq 1 ]; then
    echo "Error: Threshold must be positive, got: $THRESHOLD"
    exit 1
fi

# Check for uncommitted changes
STASH_REF=""
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Warning: You have uncommitted changes. They will be stashed."
    git stash push -m "local_benchmark_compare auto-stash"
    STASH_REF=$(git stash list -1 --format="%h")
    echo "Stashed as: $STASH_REF (stash@{0})"
    STASHED=1
else
    STASHED=0
fi

cleanup() {
    echo ""
    echo "=== Cleaning up ==="
    git checkout "$BRANCH" 2>/dev/null || true
    if [ "$STASHED" -eq 1 ]; then
        local stash_error
        if ! stash_error=$(git stash pop 2>&1); then
            echo "WARNING: Could not restore stashed changes automatically."
            echo "         Error: $stash_error"
            echo "         Your changes are in stash ref: $STASH_REF"
            echo "         Run 'git stash list' to find your stash, 'git stash pop' to restore."
        fi
    fi
    rm -rf "$TMP_SCRIPTS" 2>/dev/null || true
}
trap cleanup EXIT

# Ensure build directory exists
if [ ! -d "build" ]; then
    echo "Error: build directory not found. Run cmake first."
    exit 1
fi

# Copy scripts before checkout (they may not exist on master)
echo "=== Preserving benchmark scripts ==="
cp -r scripts "$TMP_SCRIPTS"

echo ""
echo "=== Benchmarking current branch ($BRANCH) ==="
cd build
# Reconfigure to ensure consistent Release build with O3 optimization
rm -rf CMakeCache.txt CMakeFiles
# Note: Unlike CI (which uses -march=x86-64-v3 for consistency across runners),
# local builds omit -march to use native optimizations for your hardware
cmake -DCMAKE_BUILD_TYPE=Release \
    -Dbuild_tests=ON \
    -DCMAKE_CXX_FLAGS="-DBOOST_TIMER_ENABLE_DEPRECATED -O3" \
    ..
make -j"$NPROC" performance-test
run_benchmarks "current" "Current branch"
echo "Current branch results saved to build/current_results.txt"
cd ..

echo ""
echo "=== Benchmarking $BASE_BRANCH branch ==="
git checkout "$BASE_BRANCH"
cd build
rm -rf CMakeCache.txt CMakeFiles
cmake -DCMAKE_BUILD_TYPE=Release \
    -Dbuild_tests=ON \
    -DCMAKE_CXX_FLAGS="-DBOOST_TIMER_ENABLE_DEPRECATED -O3" \
    ..
make -j"$NPROC" performance-test
run_benchmarks "base" "$BASE_BRANCH branch"
echo "$BASE_BRANCH branch results saved to build/base_results.txt"
cd ..

echo ""
echo "=== Comparison ==="
echo "Default threshold: ${THRESHOLD}%"
if [ -n "$RELAXED_BENCHMARKS" ]; then
    echo "Relaxed threshold: ${RELAXED_THRESHOLD}% for: ${RELAXED_BENCHMARKS}"
    python3 "$TMP_SCRIPTS/compare_benchmarks.py" \
        build/base_results.txt \
        build/current_results.txt \
        --threshold="$THRESHOLD" \
        --relaxed-threshold="$RELAXED_THRESHOLD" \
        --relaxed-benchmarks="$RELAXED_BENCHMARKS"
else
    echo "Relaxed threshold: (none - all benchmarks use default)"
    python3 "$TMP_SCRIPTS/compare_benchmarks.py" \
        build/base_results.txt \
        build/current_results.txt \
        --threshold="$THRESHOLD"
fi
