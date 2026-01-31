#!/bin/bash
# test_master_comprehensive.sh
# Comprehensive end-to-end test of Modbus RTU Master
# Tests master against library's slave implementation using virtual serial ports
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
MASTER_EXEC=""
BAUD_RATE=9600
SLAVE_ID=1
CLEANUP_DONE=false

# Cleanup function
cleanup() {
    # Prevent multiple calls to cleanup
    if [ "$CLEANUP_DONE" = true ]; then
        return
    fi
    CLEANUP_DONE=true

    # Remove trap to prevent re-entrancy
    trap - EXIT INT TERM

    echo -e "\n${YELLOW}Cleaning up...${NC}"

    # Kill master process if running
    if [ -n "${MASTER_PID:-}" ] && kill -0 "$MASTER_PID" 2>/dev/null; then
        echo "  Stopping master (PID: $MASTER_PID)"
        kill "$MASTER_PID" 2>/dev/null || true
        wait "$MASTER_PID" 2>/dev/null || true
    fi

    # Kill socat process
    if [ -n "$SOCAT_PID" ] && kill -0 "$SOCAT_PID" 2>/dev/null; then
        echo "  Stopping socat (PID: $SOCAT_PID)"
        kill "$SOCAT_PID" 2>/dev/null || true
        wait "$SOCAT_PID" 2>/dev/null || true
    fi

    # Clean up any remaining processes (but be more careful)
    pkill -f "^.*test_master_comprehensive.*" 2>/dev/null || true
    pkill -f "socat.*pty.*pty" 2>/dev/null || true

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

    # Check if master test executable exists
    if [ -f "build/test_master_comprehensive" ]; then
        MASTER_EXEC="build/test_master_comprehensive"
    else
        echo -e "${RED}Error: test_master_comprehensive not found${NC}"
        echo "  Build with: cd build && cmake .. -DBUILD_EXAMPLES=ON && make test_master_comprehensive"
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

# Main execution
main() {
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Modbus RTU Master Comprehensive Test${NC}"
    echo -e "${YELLOW}Testing master against library's slave${NC}"
    echo -e "${YELLOW}========================================${NC}\n"

    # Check prerequisites
    check_prerequisites

    # Create virtual ports
    create_virtual_ports

    # Wait a moment for ports to be ready
    sleep 0.5

    # Run master test (it will start the slave internally)
    echo -e "${BLUE}Running master comprehensive test...${NC}\n"

    if "$MASTER_EXEC" "$MASTER_PORT" "$SLAVE_PORT" "$BAUD_RATE" "$SLAVE_ID"; then
        echo -e "\n${GREEN}========================================${NC}"
        echo -e "${GREEN}All master tests passed! ✓${NC}"
        echo -e "${GREEN}========================================${NC}\n"
        exit 0
    else
        echo -e "\n${RED}========================================${NC}"
        echo -e "${RED}Some master tests failed ✗${NC}"
        echo -e "${RED}========================================${NC}\n"
        exit 1
    fi
}

# Run main function
main "$@"
