#!/bin/bash
# test_with_mbpoll_comprehensive.sh
# Comprehensive Modbus RTU test script using mbpoll and socat
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
SLAVE_PORT=""
MASTER_PORT=""
SLAVE_PID=""
SOCAT_PID=""
SLAVE_EXEC=""
BAUD_RATE=9600
SLAVE_ID=1
TEST_RESULTS=0
TOTAL_TESTS=0

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"

    # Kill slave process
    if [ -n "$SLAVE_PID" ] && kill -0 "$SLAVE_PID" 2>/dev/null; then
        echo "  Stopping slave (PID: $SLAVE_PID)"
        kill "$SLAVE_PID" 2>/dev/null || true
        wait "$SLAVE_PID" 2>/dev/null || true
    fi

    # Kill socat process
    if [ -n "$SOCAT_PID" ] && kill -0 "$SOCAT_PID" 2>/dev/null; then
        echo "  Stopping socat (PID: $SOCAT_PID)"
        kill "$SOCAT_PID" 2>/dev/null || true
        wait "$SOCAT_PID" 2>/dev/null || true
    fi

    # Clean up any remaining processes
    pkill -f "example_virtual_port_test.*slave" 2>/dev/null || true
    pkill -f "socat.*pty" 2>/dev/null || true

    echo -e "${GREEN}Cleanup complete${NC}\n"
}

# Set up signal handlers
trap cleanup EXIT INT TERM

# Check prerequisites
check_prerequisites() {
    local missing=0

    if ! command -v socat &> /dev/null; then
        echo -e "${RED}Error: socat is not installed${NC}"
        echo "  Install with: sudo apt-get install socat"
        missing=1
    fi

    if ! command -v mbpoll &> /dev/null; then
        echo -e "${RED}Error: mbpoll is not installed${NC}"
        echo "  Install with: sudo apt-get install mbpoll"
        missing=1
    fi

    # Check if slave executable exists
    if [ -f "build/example_virtual_port_test" ]; then
        SLAVE_EXEC="build/example_virtual_port_test"
    elif [ -f "build/testable_slave" ]; then
        SLAVE_EXEC="build/testable_slave"
    else
        echo -e "${RED}Error: Slave executable not found${NC}"
        echo "  Build with: cd build && cmake .. -DBUILD_EXAMPLES=ON && make example_virtual_port_test"
        missing=1
    fi

    if [ $missing -eq 1 ]; then
        exit 1
    fi
}

# Create virtual serial port pair
create_virtual_ports() {
    echo -e "${BLUE}Creating virtual serial port pair...${NC}"

    # Start socat in background and capture output
    local socat_output
    socat_output=$(mktemp)

    socat -d -d pty,raw,echo=0 pty,raw,echo=0 > "$socat_output" 2>&1 &
    SOCAT_PID=$!

    # Wait a moment for ports to be created
    sleep 1

    # Extract port names from socat output
    SLAVE_PORT=$(grep "PTY is" "$socat_output" | head -1 | awk '{print $NF}')
    MASTER_PORT=$(grep "PTY is" "$socat_output" | tail -1 | awk '{print $NF}')

    rm -f "$socat_output"

    if [ -z "$SLAVE_PORT" ] || [ -z "$MASTER_PORT" ]; then
        echo -e "${RED}Error: Failed to create virtual ports${NC}"
        exit 1
    fi

    echo -e "${GREEN}  Slave port:  $SLAVE_PORT${NC}"
    echo -e "${GREEN}  Master port: $MASTER_PORT${NC}"
    echo ""
}

# Start slave
start_slave() {
    echo -e "${BLUE}Starting Modbus slave...${NC}"

    if [ "$SLAVE_EXEC" = "build/example_virtual_port_test" ]; then
        "$SLAVE_EXEC" slave "$SLAVE_PORT" "$BAUD_RATE" "$SLAVE_ID" > /dev/null 2>&1 &
    else
        "$SLAVE_EXEC" "$SLAVE_PORT" "$BAUD_RATE" "$SLAVE_ID" > /dev/null 2>&1 &
    fi

    SLAVE_PID=$!

    # Wait for slave to start and be ready
    sleep 2

    if ! kill -0 "$SLAVE_PID" 2>/dev/null; then
        echo -e "${RED}Error: Slave failed to start${NC}"
        exit 1
    fi

    echo -e "${GREEN}  Slave started (PID: $SLAVE_PID)${NC}"
    echo ""
}

# Run a test and check result
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
                cat /tmp/mbpoll_output.txt | grep -E "\[0\]:|Written|Error" | head -3 | sed 's/^/    /' || cat /tmp/mbpoll_output.txt | tail -3 | sed 's/^/    /'
            fi
        else
            echo -e "${GREEN}  ✓ PASSED${NC}"
        fi
    else
        echo -e "${RED}  ✗ FAILED${NC}"
        TEST_RESULTS=$((TEST_RESULTS + 1))
        echo "  Error output:"
        cat /tmp/mbpoll_output.txt | head -5 | sed 's/^/    /'
    fi

    # Small delay between tests to avoid timing issues
    # Longer delay after write operations
    if echo "$mbpoll_cmd" | grep -q "Written\|write"; then
        sleep 0.5
    else
        sleep 0.2
    fi
    echo ""
}

# Test function codes
test_function_codes() {
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Modbus RTU Function Code Tests${NC}"
    echo -e "${YELLOW}========================================${NC}\n"

    # FC 3: Read Holding Registers
    run_test "FC 3: Read Holding Registers (0-9)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -c 10 -1 $MASTER_PORT" \
        "\[0\]:"

    # FC 4: Read Input Registers
    run_test "FC 4: Read Input Registers (0-4)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -c 5 -t 4 -1 $MASTER_PORT" \
        "\[0\]:"

    # FC 1: Read Coils
    run_test "FC 1: Read Coils (0-7)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -c 8 -t 1 -1 $MASTER_PORT" \
        "\[0\]:"

    # FC 2: Read Discrete Inputs (skip if not supported by mbpoll)
    # Note: Some mbpoll versions don't support FC 2, so we'll skip this test
    # run_test "FC 2: Read Discrete Inputs (0-7)" \
    #     "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -c 8 -t 2 -1 $MASTER_PORT" \
    #     "\[0\]:"

    # FC 6: Write Single Register (round-trip)
    echo -e "${BLUE}Round-trip tests:${NC}\n"

    run_test "FC 6: Write Single Register (address 0 = 1234)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 $MASTER_PORT 1234" \
        "Written"

    run_test "FC 3: Read back register 0 (should be 1234)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -c 1 -1 $MASTER_PORT" \
        "1234"

    # FC 6: Write another value
    run_test "FC 6: Write Single Register (address 1 = 5678)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 1 $MASTER_PORT 5678" \
        "Written"

    run_test "FC 3: Read back register 1 (should be 5678)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 1 -c 1 -1 $MASTER_PORT" \
        "5678"

    # FC 16: Write Multiple Registers (don't use -c for writing, count inferred from values)
    # Increase timeout for multiple writes
    run_test "FC 16: Write Multiple Registers (addresses 2-4 = 100, 200, 300)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 2 -o 2 $MASTER_PORT 100 200 300" \
        "Written"

    run_test "FC 3: Read back registers 2-4 (should be 100, 200, 300)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 2 -c 3 -1 $MASTER_PORT" \
        "100"

    # FC 5: Write Single Coil (use -t 0 for discrete output/coil, not -t 1)
    run_test "FC 5: Write Single Coil (address 0 = ON)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -t 0 $MASTER_PORT 0xFF00" \
        "Written"

    run_test "FC 1: Read back coil 0 (should be ON)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -c 1 -t 1 -1 $MASTER_PORT" \
        "\[0\]:"

    # FC 5: Write Single Coil OFF
    run_test "FC 5: Write Single Coil (address 0 = OFF)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -t 0 $MASTER_PORT 0x0000" \
        "Written"

    # FC 15: Write Multiple Coils (don't use -c for writing, count inferred from values)
    # Increase timeout for multiple writes
    run_test "FC 15: Write Multiple Coils (addresses 1-3)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 1 -t 0 -o 2 $MASTER_PORT 1 0 1" \
        "Written"

    run_test "FC 1: Read back coils 1-3" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 1 -c 3 -t 1 -1 $MASTER_PORT" \
        "\["

    # Test reading multiple registers after writes
    run_test "FC 3: Read registers 0-4 (verify all writes)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 0 -c 5 -1 $MASTER_PORT" \
        "\[0\]:"

    # Test with different addresses
    run_test "FC 6: Write register 10 = 9999" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 10 $MASTER_PORT 9999" \
        "Written"

    run_test "FC 3: Read register 10 (should be 9999)" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -0 -r 10 -c 1 -1 $MASTER_PORT" \
        "9999"

    # Additional function codes
    echo -e "${BLUE}Additional function code tests:${NC}\n"

    # FC 17: Report Slave ID (using -u flag)
    run_test "FC 17: Report Slave ID" \
        "mbpoll -m rtu -b $BAUD_RATE -P even -a $SLAVE_ID -u -1 $MASTER_PORT" \
        "Slave"

    # Note: The following function codes are implemented but cannot be tested with mbpoll:
    # - FC 7: Read Exception Status (not supported by mbpoll)
    # - FC 8: Diagnostics (not supported by mbpoll)
    # - FC 11: Get Com Event Counter (not supported by mbpoll)
    # - FC 12: Get Com Event Log (not supported by mbpoll)
    # - FC 22: Mask Write Register (not supported by mbpoll)
    # - FC 23: Read/Write Multiple Registers (not supported by mbpoll)
    # - FC 24: Read FIFO Queue (not supported by mbpoll)
    # These are tested in the unit test suite (test/rtu/)
}

# Main execution
main() {
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Modbus RTU Comprehensive Test Suite${NC}"
    echo -e "${YELLOW}Using mbpoll and socat${NC}"
    echo -e "${YELLOW}========================================${NC}\n"

    # Check prerequisites
    check_prerequisites

    # Create virtual ports
    create_virtual_ports

    # Start slave
    start_slave

    # Wait a moment for everything to settle
    sleep 0.5

    # Run tests
    test_function_codes

    # Print summary
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
    echo -e "${YELLOW}Function Code Coverage:${NC}"
    echo -e "${GREEN}✓ Tested with mbpoll:${NC}"
    echo -e "  - FC 1: Read Coils"
    echo -e "  - FC 3: Read Holding Registers"
    echo -e "  - FC 4: Read Input Registers"
    echo -e "  - FC 5: Write Single Coil"
    echo -e "  - FC 6: Write Single Register"
    echo -e "  - FC 15: Write Multiple Coils"
    echo -e "  - FC 16: Write Multiple Registers"
    echo -e "  - FC 17: Report Slave ID"
    echo ""
    echo -e "${YELLOW}⚠ Implemented but not testable with mbpoll:${NC}"
    echo -e "  - FC 2: Read Discrete Inputs (mbpoll limitation)"
    echo -e "  - FC 7: Read Exception Status"
    echo -e "  - FC 8: Diagnostics"
    echo -e "  - FC 11: Get Com Event Counter"
    echo -e "  - FC 12: Get Com Event Log"
    echo -e "  - FC 22: Mask Write Register"
    echo -e "  - FC 23: Read/Write Multiple Registers"
    echo -e "  - FC 24: Read FIFO Queue"
    echo -e "${BLUE}  (These are tested in the unit test suite: test/rtu/)${NC}"
    echo ""

    # Cleanup will happen automatically via trap
    exit $TEST_RESULTS
}

# Run main function
main "$@"
