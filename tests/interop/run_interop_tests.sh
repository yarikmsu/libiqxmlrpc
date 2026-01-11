#!/bin/bash
#
# Interoperability test runner for libiqxmlrpc
#
# This script:
# 1. Starts a Python XML-RPC server
# 2. Runs libiqxmlrpc client tests against it
# 3. Reports results
#
# Usage: ./run_interop_tests.sh [BUILD_DIR]
#
# Requirements:
# - Python 3 with xmlrpc.server module (standard library)
# - Built libiqxmlrpc test binaries
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-${SCRIPT_DIR}/../../build}"
PORT=18765  # Use high port to avoid conflicts
HOST="localhost"
SERVER_PID=""
PASSED=0
FAILED=0
TESTS_RUN=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        echo -e "${YELLOW}Stopping Python XML-RPC server (PID: $SERVER_PID)...${NC}"
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASSED++))
    ((TESTS_RUN++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((FAILED++))
    ((TESTS_RUN++))
}

log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."

    if ! command -v python3 &> /dev/null; then
        echo "ERROR: python3 is required but not found"
        exit 1
    fi

    if [ ! -f "$SCRIPT_DIR/python_xmlrpc_server.py" ]; then
        echo "ERROR: python_xmlrpc_server.py not found in $SCRIPT_DIR"
        exit 1
    fi

    # Check if test binary exists
    if [ ! -f "$BUILD_DIR/tests/test_integration" ]; then
        echo "ERROR: test_integration binary not found in $BUILD_DIR/tests/"
        echo "Please build the project first: cmake --build $BUILD_DIR"
        exit 1
    fi

    log_info "Prerequisites OK"
}

# Start the Python XML-RPC server
start_server() {
    log_info "Starting Python XML-RPC server on $HOST:$PORT..."

    python3 "$SCRIPT_DIR/python_xmlrpc_server.py" --host "$HOST" --port "$PORT" &
    SERVER_PID=$!

    # Wait for server to start
    local retries=10
    while [ $retries -gt 0 ]; do
        if curl -s -o /dev/null -w "%{http_code}" "http://$HOST:$PORT/" 2>/dev/null | grep -q "405\|200"; then
            log_info "Server started successfully (PID: $SERVER_PID)"
            return 0
        fi
        sleep 0.5
        ((retries--))
    done

    echo "ERROR: Server failed to start"
    exit 1
}

# Run a single test using Python client (for quick validation)
run_python_client_test() {
    local method="$1"
    local expected="$2"
    shift 2
    local params="$@"

    python3 -c "
import xmlrpc.client
proxy = xmlrpc.client.ServerProxy('http://$HOST:$PORT/')
result = proxy.$method($params)
expected = $expected
if result == expected:
    exit(0)
else:
    print(f'Expected: {expected}, Got: {result}')
    exit(1)
" 2>/dev/null

    return $?
}

# Test Python server is working correctly first
test_python_server() {
    log_info "Testing Python server functionality..."

    if run_python_client_test "add" "5" "2, 3"; then
        log_pass "Python server: add(2, 3) = 5"
    else
        log_fail "Python server: add(2, 3) = 5"
    fi

    if run_python_client_test "echo_string" "'hello'" "'hello'"; then
        log_pass "Python server: echo_string('hello')"
    else
        log_fail "Python server: echo_string('hello')"
    fi

    if run_python_client_test "echo_int" "42" "42"; then
        log_pass "Python server: echo_int(42)"
    else
        log_fail "Python server: echo_int(42)"
    fi
}

# Run libiqxmlrpc integration tests against Python server
run_libiqxmlrpc_tests() {
    log_info "Running libiqxmlrpc wire compatibility tests..."

    # Run the dedicated wire compatibility test if it exists
    if [ -f "$BUILD_DIR/tests/wire-compatibility-test" ]; then
        log_info "Running wire-compatibility-test..."
        # Set environment variables for server connection
        export XMLRPC_TEST_HOST="$HOST"
        export XMLRPC_TEST_PORT="$PORT"

        if "$BUILD_DIR/tests/wire-compatibility-test" --log_level=test_suite 2>&1; then
            log_pass "Wire compatibility tests passed"
        else
            log_fail "Wire compatibility tests failed"
        fi
    else
        log_info "wire-compatibility-test not found in $BUILD_DIR/tests/, skipping..."
        log_info "Build it with: cmake --build $BUILD_DIR --target wire-compatibility-test"
    fi
}

# Summary
print_summary() {
    echo ""
    echo "=============================================="
    echo "           INTEROP TEST SUMMARY"
    echo "=============================================="
    echo -e "Tests run:  $TESTS_RUN"
    echo -e "Passed:     ${GREEN}$PASSED${NC}"
    echo -e "Failed:     ${RED}$FAILED${NC}"
    echo "=============================================="

    if [ $FAILED -gt 0 ]; then
        echo -e "${RED}SOME TESTS FAILED${NC}"
        return 1
    else
        echo -e "${GREEN}ALL TESTS PASSED${NC}"
        return 0
    fi
}

# Main
main() {
    echo "=============================================="
    echo "  libiqxmlrpc Interoperability Test Suite"
    echo "=============================================="
    echo ""

    check_prerequisites
    start_server
    echo ""

    test_python_server
    echo ""

    run_libiqxmlrpc_tests
    echo ""

    print_summary
}

main "$@"
