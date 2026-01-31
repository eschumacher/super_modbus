# Building and Testing with Modbus Poll

This guide explains how to build the library and test it with Modbus Poll or similar Modbus testing software.

## Building the Library

### Prerequisites

- CMake 3.15 or higher
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Make or Ninja

### Build Steps

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build library and examples
make -j$(nproc)

# Or build specific targets
make super-modbus-lib          # Just the library
make example_master            # Master example
make example_slave             # Slave example
make example_loopback          # Loopback example
make run_tests                 # Test suite
```

### Build Options

```bash
# Build with examples
cmake .. -DBUILD_EXAMPLES=ON

# Build without tests
cmake .. -DBUILD_TESTS=OFF

# Build in Debug mode (for tests)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build in Release mode (for production)
cmake .. -DCMAKE_BUILD_TYPE=Release
```

## Testing with Modbus Poll

Modbus Poll is a Windows application that acts as a Modbus master. To test your slave implementation:

### Option 1: Using Virtual Serial Ports (Recommended for Development)

#### On Linux (using `socat`):

```bash
# Install socat
sudo apt-get install socat  # Debian/Ubuntu
# or
sudo yum install socat      # RHEL/CentOS

# Create virtual serial port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0

# This will output something like:
# 2024/01/01 12:00:00 socat[12345] N PTY is /dev/pts/2
# 2024/01/01 12:00:00 socat[12346] N PTY is /dev/pts/3

# Terminal 1: Run your slave application using /dev/pts/2
./example_slave /dev/pts/2

# Terminal 2: Connect Modbus Poll (via WSL or Windows) to /dev/pts/3
# Or use a serial port bridge tool
```

#### On Windows (using `com0com`):

1. Download and install [com0com](https://sourceforge.net/projects/com0com/)
2. Create a virtual COM port pair (e.g., COM3 <-> COM4)
3. Run your slave application on one port
4. Connect Modbus Poll to the other port

### Option 2: Using Real Serial Hardware

If you have two serial ports or a USB-to-serial adapter:

```bash
# Terminal 1: Run slave on /dev/ttyUSB0
./example_slave /dev/ttyUSB0

# Terminal 2: Connect Modbus Poll to /dev/ttyUSB1 (or via Windows)
```

### Option 3: Using Modbus TCP Bridge

You can create a bridge application that:
1. Listens for Modbus TCP connections (from Modbus Poll)
2. Converts to Modbus RTU and forwards to your slave via serial

## Creating a Testable Slave Application

Create a simple slave application that you can test with Modbus Poll:

```cpp
// examples/testable_slave.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "super_modbus/rtu/rtu_slave.hpp"
// Include your serial transport implementation here

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <serial_port> [baud_rate]\n";
    std::cerr << "Example: " << argv[0] << " /dev/ttyUSB0 9600\n";
    return 1;
  }

  const char* port = argv[1];
  int baud_rate = (argc >= 3) ? std::atoi(argv[2]) : 9600;

  // Create your serial transport
  // MySerialTransport transport(port, baud_rate);

  // For now, using MemoryTransport as placeholder
  // Replace with your serial transport implementation
  supermb::MemoryTransport transport;

  // Create slave with ID 1
  supermb::RtuSlave slave(1);

  // Configure address space
  slave.AddHoldingRegisters({0, 100});    // Addresses 0-99
  slave.AddInputRegisters({0, 50});       // Addresses 0-49
  slave.AddCoils({0, 100});               // Addresses 0-99
  slave.AddDiscreteInputs({0, 50});       // Addresses 0-49

  std::cout << "Modbus RTU Slave started\n";
  std::cout << "Slave ID: 1\n";
  std::cout << "Holding Registers: 0-99\n";
  std::cout << "Input Registers: 0-49\n";
  std::cout << "Coils: 0-99\n";
  std::cout << "Discrete Inputs: 0-49\n";
  std::cout << "Listening on " << port << " at " << baud_rate << " baud\n";
  std::cout << "Press Ctrl+C to stop\n\n";

  // Poll loop
  bool running = true;
  while (running) {
    if (slave.Poll(transport)) {
      std::cout << "Request processed\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return 0;
}
```

## Modbus Poll Configuration

When connecting Modbus Poll to your slave:

### Connection Settings:
- **Connection Type**: RTU over Serial Port
- **Serial Port**: COM3 (or your virtual/real port)
- **Baud Rate**: 9600 (or match your application)
- **Data Bits**: 8
- **Parity**: Even (or None, depending on your setup)
- **Stop Bits**: 1
- **Slave ID**: 1 (match your slave configuration)

### Testing Operations:

1. **Read Holding Registers (FC 3)**:
   - Address: 0
   - Quantity: 10
   - Should read 10 registers starting at address 0

2. **Write Single Register (FC 6)**:
   - Address: 0
   - Value: 1234
   - Should write value 1234 to register 0

3. **Read Coils (FC 1)**:
   - Address: 0
   - Quantity: 8
   - Should read 8 coils starting at address 0

4. **Write Single Coil (FC 5)**:
   - Address: 0
   - Value: ON
   - Should turn coil 0 ON

## Quick Test Script

Create a test script to verify your build:

```bash
#!/bin/bash
# test_build.sh

set -e

echo "Building Super Modbus library..."

cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
make -j$(nproc)

echo ""
echo "Build successful!"
echo ""
echo "Executables created:"
ls -lh example_* 2>/dev/null || echo "  (Examples not built)"
echo ""
echo "Library created:"
ls -lh libsuper-modbus-lib.a 2>/dev/null || echo "  (Library not found)"
echo ""
echo "To test with Modbus Poll:"
echo "  1. Implement serial transport (see examples/example_serial_transport.cpp)"
echo "  2. Build testable_slave application"
echo "  3. Connect Modbus Poll to the serial port"
echo "  4. Configure Modbus Poll: Slave ID=1, RTU, 9600 baud"
```

## Troubleshooting

### Build Issues

```bash
# Clean build
rm -rf build
mkdir build
cd build
cmake ..
make

# Verbose build
make VERBOSE=1
```

### Serial Port Issues

```bash
# Check available serial ports (Linux)
ls -l /dev/ttyUSB* /dev/ttyACM* /dev/ttyS*

# Check permissions
ls -l /dev/ttyUSB0
# If permission denied, add user to dialout group:
sudo usermod -a -G dialout $USER
# Then logout and login again

# Test serial port (Linux)
stty -F /dev/ttyUSB0 9600 cs8 -cstopb -parenb
cat /dev/ttyUSB0  # Should see data if connected
```

### Modbus Poll Connection Issues

- **No Response**: Check baud rate, parity, stop bits match
- **Timeout**: Increase timeout in Modbus Poll settings
- **CRC Errors**: Verify serial port settings match exactly
- **Wrong Slave ID**: Ensure Modbus Poll slave ID matches your slave (default: 1)

## Example: Complete Test Setup

```bash
# 1. Build everything
cd /workspaces/super_modbus
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
make -j$(nproc)

# 2. Create virtual serial ports (Linux)
socat -d -d pty,raw,echo=0 pty,raw,echo=0 &
# Note the two /dev/pts/X paths

# 3. Run slave on one port (replace with your serial transport)
# ./example_slave /dev/pts/2 9600

# 4. Connect Modbus Poll to the other port (/dev/pts/3)
# Or use a serial port bridge to connect to Windows COM port
```

## Next Steps

1. **Implement Serial Transport**: See `examples/example_serial_transport.cpp` for template
2. **Create Testable Slave**: Build a slave application with your serial transport
3. **Configure Modbus Poll**: Set up connection parameters
4. **Test Function Codes**: Try reading/writing registers and coils
5. **Verify Responses**: Check that values are correctly read/written
