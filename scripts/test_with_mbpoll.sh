#!/bin/bash
# test_with_mbpoll.sh
# Helper script to demonstrate testing with mbpoll

set -e

echo "=========================================="
echo "Modbus Testing with mbpoll"
echo "=========================================="
echo ""

# Check if mbpoll is installed
if ! command -v mbpoll &> /dev/null; then
    echo "Error: mbpoll is not installed."
    echo "Install it with: sudo apt-get install mbpoll"
    exit 1
fi

# Check if socat is installed
if ! command -v socat &> /dev/null; then
    echo "Error: socat is not installed."
    echo "Install it with: sudo apt-get install socat"
    exit 1
fi

# Check if example is built
if [ ! -f "build/example_virtual_port_test" ]; then
    echo "Error: example_virtual_port_test not found."
    echo "Build it with:"
    echo "  cd build"
    echo "  cmake .. -DBUILD_EXAMPLES=ON"
    echo "  make example_virtual_port_test"
    exit 1
fi

echo "This script will help you test your Modbus slave with mbpoll."
echo ""
echo "Setup:"
echo "  1. We'll create virtual serial ports with socat"
echo "  2. You'll run the slave in one terminal"
echo "  3. You'll use mbpoll commands in another terminal"
echo ""
echo "Press Enter to continue or Ctrl+C to cancel..."
read

echo ""
echo "Step 1: Creating virtual serial port pair..."
echo "Run this in a separate terminal (keep it open):"
echo ""
echo "  socat -d -d pty,raw,echo=0 pty,raw,echo=0"
echo ""
echo "Note the two /dev/pts/X paths from the output."
echo ""
echo "Press Enter when you have the virtual ports running..."
read

echo ""
echo "Step 2: Run your slave"
echo "In another terminal, run:"
echo ""
echo "  cd build"
echo "  ./example_virtual_port_test slave /dev/pts/2 9600 1"
echo ""
echo "(Replace /dev/pts/2 with the first port from socat output)"
echo ""
echo "Press Enter when your slave is running..."
read

echo ""
echo "Step 3: Test with mbpoll"
echo "Use these commands in a third terminal:"
echo ""
echo "# Read 10 holding registers:"
echo "mbpoll -m rtu -b 9600 -P even -a 1 -r 0 -c 10 /dev/pts/3"
echo ""
echo "# Write value 1234 to register 0:"
echo "mbpoll -m rtu -b 9600 -P even -a 1 -r 0 -1 1234 /dev/pts/3"
echo ""
echo "# Read it back:"
echo "mbpoll -m rtu -b 9600 -P even -a 1 -r 0 -c 1 /dev/pts/3"
echo ""
echo "# Read 8 coils:"
echo "mbpoll -m rtu -b 9600 -P even -a 1 -r 0 -c 8 -t 1 /dev/pts/3"
echo ""
echo "# Turn on coil 0:"
echo "mbpoll -m rtu -b 9600 -P even -a 1 -r 0 -1 0xFF00 -t 1 /dev/pts/3"
echo ""
echo "(Replace /dev/pts/3 with the second port from socat output)"
echo ""
echo "=========================================="
echo "For more information, see:"
echo "  - MBPOLL_TESTING.md (complete mbpoll reference)"
echo "  - VIRTUAL_SERIAL_PORT_GUIDE.md (full guide)"
echo "=========================================="
