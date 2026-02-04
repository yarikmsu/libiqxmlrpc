#!/bin/bash
#
# Compare RPS performance between two versions of libiqxmlrpc.
#
# Usage:
#   ./scripts/compare_versions.sh [VERSION1] [VERSION2]
#   ./scripts/compare_versions.sh 0.13.8 0.14.1
#   ./scripts/compare_versions.sh  # Defaults to 0.13.8 vs current
#
# This script:
#   1. Builds the benchmark server for the current version
#   2. Runs the Python RPS benchmark
#   3. Checks out the comparison version, builds, and runs benchmark
#   4. Displays a comparison of results
#
# Prerequisites:
#   - Python 3.6+ (standard library only)
#   - CMake and build tools
#   - Git
#   - curl (for server health check)

set -e

# Configuration
VERSION1="${1:-0.13.8}"
VERSION2="${2:-current}"
PORT=18765
THREADS=4
REQUESTS=10000
CLIENTS="1,4,8"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build_benchmark"
RESULTS_DIR="$PROJECT_DIR/benchmark_results"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_success() { echo -e "${GREEN}[OK]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

validate_git_ref() {
    local ref=$1
    if [ "$ref" = "current" ]; then
        return 0
    fi
    # Check if ref exists as tag, branch, or commit
    if ! git -C "$PROJECT_DIR" rev-parse --verify "$ref^{commit}" >/dev/null 2>&1; then
        log_error "Invalid git reference: $ref"
        log_error "Use a valid tag (e.g., 0.13.8), branch, or commit hash"
        return 1
    fi
    return 0
}

cleanup() {
    # Kill any running benchmark server
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        log_info "Stopping benchmark server (PID $SERVER_PID)..."
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

wait_for_server() {
    local port=$1
    local max_wait=10
    local waited=0

    while ! curl -s "http://localhost:$port/RPC" >/dev/null 2>&1; do
        sleep 0.5
        waited=$((waited + 1))
        if [ $waited -ge $((max_wait * 2)) ]; then
            log_error "Server did not start within ${max_wait}s"
            return 1
        fi
    done
    return 0
}

build_version() {
    local version=$1
    local build_path=$2

    log_info "Building version: $version"

    mkdir -p "$build_path"
    cd "$build_path"

    # Configure and build
    cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
    make -j4 benchmark-server >/dev/null 2>&1

    if [ ! -f "$build_path/tests/benchmark-server" ]; then
        log_error "Build failed: benchmark-server not found"
        return 1
    fi

    log_success "Build complete"
}

run_benchmark() {
    local version=$1
    local build_path=$2
    local output_file=$3

    log_info "Running benchmark for version: $version"

    # Start server in background
    "$build_path/tests/benchmark-server" --port "$PORT" --numthreads "$THREADS" --quiet &
    SERVER_PID=$!

    # Wait for server to be ready
    if ! wait_for_server "$PORT"; then
        return 1
    fi

    log_info "Server started (PID $SERVER_PID), running benchmark..."

    # Run Python benchmark
    python3 "$SCRIPT_DIR/rps_benchmark.py" \
        --port "$PORT" \
        --requests "$REQUESTS" \
        --clients "$CLIENTS" \
        --output json \
        --quiet > "$output_file"

    # Stop server
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    SERVER_PID=""

    log_success "Benchmark complete for $version"
}

compare_results() {
    local file1=$1
    local file2=$2
    local v1=$3
    local v2=$4

    echo ""
    echo "=============================================="
    echo "         RPS COMPARISON: $v1 vs $v2"
    echo "=============================================="
    echo ""

    # Parse JSON and compare using Python
    python3 - "$file1" "$file2" "$v1" "$v2" << 'PYTHON_EOF'
import json
import sys

def load_results(filepath):
    with open(filepath) as f:
        return json.load(f)

def format_pct(old, new):
    if old == 0:
        return "N/A"
    pct = ((new - old) / old) * 100
    sign = "+" if pct >= 0 else ""
    color = "\033[32m" if pct >= 0 else "\033[31m"
    reset = "\033[0m"
    return f"{color}{sign}{pct:.1f}%{reset}"

file1, file2, v1, v2 = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]

try:
    r1 = load_results(file1)
    r2 = load_results(file2)
except Exception as e:
    print(f"Error loading results: {e}")
    sys.exit(1)

print(f"{'Scenario':<20} | {'Clients':>7} | {v1+' RPS':>12} | {v2+' RPS':>12} | {'Change':>10}")
print("-" * 75)

for scenario in ['small_payload', 'large_payload']:
    if scenario not in r1.get('scenarios', {}) or scenario not in r2.get('scenarios', {}):
        continue

    s1 = {x['clients']: x for x in r1['scenarios'][scenario]}
    s2 = {x['clients']: x for x in r2['scenarios'][scenario]}

    for clients in sorted(set(s1.keys()) & set(s2.keys())):
        rps1 = s1[clients]['rps']
        rps2 = s2[clients]['rps']
        change = format_pct(rps1, rps2)
        print(f"{scenario:<20} | {clients:>7} | {rps1:>12.1f} | {rps2:>12.1f} | {change:>18}")

print()

# Latency comparison for p99
print(f"\n{'Scenario':<20} | {'Clients':>7} | {v1+' p99':>12} | {v2+' p99':>12} | {'Change':>10}")
print("-" * 75)

for scenario in ['small_payload', 'large_payload']:
    if scenario not in r1.get('scenarios', {}) or scenario not in r2.get('scenarios', {}):
        continue

    s1 = {x['clients']: x for x in r1['scenarios'][scenario]}
    s2 = {x['clients']: x for x in r2['scenarios'][scenario]}

    for clients in sorted(set(s1.keys()) & set(s2.keys())):
        p99_1 = s1[clients]['latency_ms']['p99']
        p99_2 = s2[clients]['latency_ms']['p99']
        # For latency, negative is better
        change_pct = ((p99_2 - p99_1) / p99_1) * 100 if p99_1 != 0 else 0
        color = "\033[32m" if change_pct <= 0 else "\033[31m"
        reset = "\033[0m"
        sign = "+" if change_pct >= 0 else ""
        change = f"{color}{sign}{change_pct:.1f}%{reset}"
        print(f"{scenario:<20} | {clients:>7} | {p99_1:>10.2f}ms | {p99_2:>10.2f}ms | {change:>18}")

print()
PYTHON_EOF
}

main() {
    log_info "libiqxmlrpc RPS Version Comparison"
    log_info "Comparing: $VERSION1 vs $VERSION2"
    log_info "Port: $PORT, Threads: $THREADS, Requests: $REQUESTS"
    echo ""

    # Validate git refs
    validate_git_ref "$VERSION1" || exit 1
    validate_git_ref "$VERSION2" || exit 1

    # Create results directory
    mkdir -p "$RESULTS_DIR"

    # Save current branch
    CURRENT_BRANCH=$(git -C "$PROJECT_DIR" rev-parse --abbrev-ref HEAD)
    CURRENT_COMMIT=$(git -C "$PROJECT_DIR" rev-parse HEAD)

    # Check for uncommitted changes
    if ! git -C "$PROJECT_DIR" diff --quiet || ! git -C "$PROJECT_DIR" diff --cached --quiet; then
        log_warn "You have uncommitted changes. Stashing them..."
        git -C "$PROJECT_DIR" stash push -m "compare_versions.sh temporary stash"
        STASHED=1
    fi

    # ========== VERSION 2 (current or specified) ==========
    if [ "$VERSION2" = "current" ]; then
        log_info "Using current working tree for $VERSION2"
    else
        log_info "Checking out $VERSION2..."
        git -C "$PROJECT_DIR" checkout "$VERSION2" --quiet
    fi

    BUILD_V2="$BUILD_DIR/v2"
    RESULTS_V2="$RESULTS_DIR/results_v2.json"

    build_version "$VERSION2" "$BUILD_V2"
    run_benchmark "$VERSION2" "$BUILD_V2" "$RESULTS_V2"

    # ========== VERSION 1 ==========
    log_info "Checking out $VERSION1..."
    git -C "$PROJECT_DIR" checkout "$VERSION1" --quiet

    BUILD_V1="$BUILD_DIR/v1"
    RESULTS_V1="$RESULTS_DIR/results_v1.json"

    # For old versions, we need to copy the benchmark_server.cc if it doesn't exist
    if [ ! -f "$PROJECT_DIR/tests/benchmark_server.cc" ]; then
        log_warn "benchmark_server.cc not found in $VERSION1, copying from current..."
        git -C "$PROJECT_DIR" show "$CURRENT_COMMIT:tests/benchmark_server.cc" > "$PROJECT_DIR/tests/benchmark_server.cc"

        # Also need to update CMakeLists.txt to include it
        if ! grep -q "benchmark-server" "$PROJECT_DIR/tests/CMakeLists.txt"; then
            echo "" >> "$PROJECT_DIR/tests/CMakeLists.txt"
            echo "# Benchmark server (added for version comparison)" >> "$PROJECT_DIR/tests/CMakeLists.txt"
            echo "add_executable(benchmark-server benchmark_server.cc)" >> "$PROJECT_DIR/tests/CMakeLists.txt"
            echo 'target_link_libraries(benchmark-server iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES})' >> "$PROJECT_DIR/tests/CMakeLists.txt"
        fi
    fi

    build_version "$VERSION1" "$BUILD_V1"
    run_benchmark "$VERSION1" "$BUILD_V1" "$RESULTS_V1"

    # ========== RESTORE ORIGINAL STATE ==========
    log_info "Restoring original branch ($CURRENT_BRANCH)..."
    git -C "$PROJECT_DIR" checkout "$CURRENT_BRANCH" --quiet

    # Clean up any temporary files added during old version build
    git -C "$PROJECT_DIR" checkout -- tests/benchmark_server.cc 2>/dev/null || true
    git -C "$PROJECT_DIR" checkout -- tests/CMakeLists.txt 2>/dev/null || true

    if [ -n "$STASHED" ]; then
        log_info "Restoring stashed changes..."
        git -C "$PROJECT_DIR" stash pop --quiet || true
    fi

    # ========== COMPARE ==========
    compare_results "$RESULTS_V1" "$RESULTS_V2" "$VERSION1" "$VERSION2"

    log_success "Comparison complete!"
    log_info "Results saved to: $RESULTS_DIR/"
}

main "$@"
