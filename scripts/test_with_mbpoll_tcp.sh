#!/bin/bash
# test_with_mbpoll_tcp.sh
# Comprehensive Modbus TCP test script using mbpoll and testable_tcp_slave
# Tests various function codes with round-trip verification
# Cleans up all processes on exit

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BIND_ADDR="${1:-127.0.0.1}"
PORT="${2:-5502}"
UNIT_ID="${3:-1}"
SLAVE_PID=""
TEST_RESULTS=0
TOTAL_TESTS=0

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"

    if [ -n "$SLAVE_PID" ] && kill -0 "$SLAVE_PID" 2>/dev/null; then
        echo "  Stopping TCP slave (PID: $SLAVE_PID)"
        kill "$SLAVE_PID" 2>/dev/null || true
        wait "$SLAVE_PID" 2>/dev/null || true
    fi

    pkill -f "testable_tcp_slave" 2>/dev/null || true

    echo -e "${GREEN}Cleanup complete${NC}\n"
}

trap cleanup EXIT INT TERM

check_prerequisites() {
    local missing=0

    if ! command -v mbpoll &> /dev/null; then
        echo -e "${RED}Error: mbpoll is not installed${NC}"
        echo "  Install with: sudo apt-get install mbpoll"
        missing=1
    fi

    if [ -f "build/testable_tcp_slave" ]; then
        SLAVE_EXEC="build/testable_tcp_slave"
    elif [ -f "build/bin/testable_tcp_slave" ]; then
        SLAVE_EXEC="build/bin/testable_tcp_slave"
    else
        echo -e "${RED}Error: testable_tcp_slave executable not found${NC}"
        echo "  Build with: cd build && cmake .. -DBUILD_EXAMPLES=ON && make testable_tcp_slave"
        missing=1
    fi

    if [ $missing -eq 1 ]; then
        exit 1
    fi
}

start_slave() {
    echo -e "${BLUE}Starting Modbus TCP slave on $BIND_ADDR:$PORT (unit ID: $UNIT_ID)...${NC}"

    "$SLAVE_EXEC" "$BIND_ADDR" "$PORT" "$UNIT_ID" > /tmp/tcp_slave.log 2>&1 &
    SLAVE_PID=$!

    sleep 2

    if ! kill -0 "$SLAVE_PID" 2>/dev/null; then
        echo -e "${RED}Error: TCP slave failed to start${NC}"
        cat /tmp/tcp_slave.log
        exit 1
    fi

    echo -e "${GREEN}  Slave started (PID: $SLAVE_PID)${NC}"
    echo ""
}

run_test() {
    local test_name="$1"
    local mbpoll_cmd="$2"
    local expected_pattern="${3:-}"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -e "${BLUE}Test $TOTAL_TESTS: $test_name${NC}"

    if eval "$mbpoll_cmd" > /tmp/mbpoll_output.txt 2>&1; then
        if [ -n "$expected_pattern" ]; then
            if grep -q "$expected_pattern" /tmp/mbpoll_output.txt; then
                echo -e "${GREEN}  ✓ PASSED${NC}"
            else
                echo -e "${RED}  ✗ FAILED (unexpected output)${NC}"
                TEST_RESULTS=$((TEST_RESULTS + 1))
                echo "  Expected pattern: $expected_pattern"
                echo "  Output:"
                tail -5 /tmp/mbpoll_output.txt | sed 's/^/    /'
            fi
        else
            echo -e "${GREEN}  ✓ PASSED${NC}"
        fi
    else
        echo -e "${RED}  ✗ FAILED${NC}"
        TEST_RESULTS=$((TEST_RESULTS + 1))
        echo "  Error output:"
        tail -5 /tmp/mbpoll_output.txt | sed 's/^/    /'
    fi

    if echo "$mbpoll_cmd" | grep -q "Written\|write"; then
        sleep 0.3
    else
        sleep 0.1
    fi
    echo ""
}

test_function_codes() {
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Modbus TCP Function Code Tests${NC}"
    echo -e "${YELLOW}========================================${NC}\n"

    local MBPOLL_BASE="mbpoll -m tcp -a $UNIT_ID -0 -p $PORT -1 $BIND_ADDR"

    run_test "FC 3: Read Holding Registers (0-9)" \
        "$MBPOLL_BASE -r 0 -c 10" \
        "\[0\]:"

    run_test "FC 4: Read Input Registers (0-4)" \
        "$MBPOLL_BASE -r 0 -c 5 -t 4" \
        "\[0\]:"

    run_test "FC 1: Read Coils (0-7)" \
        "$MBPOLL_BASE -r 0 -c 8 -t 1" \
        "\[0\]:"

    echo -e "${BLUE}Round-trip tests:${NC}\n"

    run_test "FC 6: Write Single Register (address 0 = 1234)" \
        "$MBPOLL_BASE -r 0 1234" \
        "Written"

    run_test "FC 3: Read back register 0 (should be 1234)" \
        "$MBPOLL_BASE -r 0 -c 1" \
        "1234"

    run_test "FC 6: Write Single Register (address 1 = 5678)" \
        "$MBPOLL_BASE -r 1 5678" \
        "Written"

    run_test "FC 3: Read back register 1 (should be 5678)" \
        "$MBPOLL_BASE -r 1 -c 1" \
        "5678"

    run_test "FC 16: Write Multiple Registers (addresses 2-4)" \
        "$MBPOLL_BASE -r 2 -o 2 100 200 300" \
        "Written"

    run_test "FC 3: Read back registers 2-4" \
        "$MBPOLL_BASE -r 2 -c 3" \
        "100"

    run_test "FC 5: Write Single Coil (address 0 = ON)" \
        "$MBPOLL_BASE -r 0 -t 0 0xFF00" \
        "Written"

    run_test "FC 1: Read back coil 0" \
        "$MBPOLL_BASE -r 0 -c 1 -t 1" \
        "\[0\]:"

    run_test "FC 15: Write Multiple Coils (addresses 1-3)" \
        "$MBPOLL_BASE -r 1 -t 0 -o 2 1 0 1" \
        "Written"

    run_test "FC 17: Report Slave ID" \
        "$MBPOLL_BASE -u" \
        "Slave"

    run_test "FC 3: Read registers 0-5 (verify all writes)" \
        "$MBPOLL_BASE -r 0 -c 6" \
        "\[0\]:"
}

main() {
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Modbus TCP Comprehensive Test Suite${NC}"
    echo -e "${YELLOW}Using mbpoll and testable_tcp_slave${NC}"
    echo -e "${YELLOW}========================================${NC}\n"

    check_prerequisites
    start_slave

    sleep 0.5

    test_function_codes

    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Test Summary${NC}"
    echo -e "${YELLOW}========================================${NC}"
    echo -e "Total tests: $TOTAL_TESTS"
    if [ $TEST_RESULTS -eq 0 ]; then
        echo -e "${GREEN}Passed: $TOTAL_TESTS${NC}"
        echo -e "${GREEN}Failed: 0${NC}"
        echo -e "\n${GREEN}All tests passed! ✓${NC}"
    else
        echo -e "${GREEN}Passed: $((TOTAL_TESTS - TEST_RESULTS))${NC}"
        echo -e "${RED}Failed: $TEST_RESULTS${NC}"
        echo -e "\n${RED}Some tests failed ✗${NC}"
    fi
    echo ""

    exit $TEST_RESULTS
}

main "$@"
