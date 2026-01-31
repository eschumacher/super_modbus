#!/bin/bash
# create_virtual_ports.sh
# Creates a virtual serial port pair using socat for testing Modbus applications

set -e

echo "Creating virtual serial port pair with socat..."
echo ""

# Check if socat is installed
if ! command -v socat &> /dev/null; then
    echo "Error: socat is not installed."
    echo "Install it with: sudo apt-get install socat"
    exit 1
fi

# Create virtual port pair
# The -d -d flags provide debug output showing the port names
echo "Starting socat..."
echo "Press Ctrl+C to stop and close the virtual ports"
echo ""

socat -d -d pty,raw,echo=0 pty,raw,echo=0
