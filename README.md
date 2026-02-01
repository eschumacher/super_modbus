# Super Modbus

[![CI](https://github.com/eschumacher/super_modbus/actions/workflows/ci.yml/badge.svg)](https://github.com/eschumacher/super_modbus/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/eschumacher/super_modbus/branch/main/graph/badge.svg)](https://codecov.io/gh/eschumacher/super_modbus)

A modern C++ Modbus library designed for easy integration into any project. The library provides a clean abstraction layer that separates the Modbus protocol logic from the underlying communication mechanism.

## Features

- **Transport Abstraction**: Works with any byte I/O mechanism (serial, TCP, memory, etc.) through a simple interface
- **Modbus RTU Support**: Full RTU frame encoding/decoding with CRC-16 verification
- **Master/Client Functionality**: Read and write registers from Modbus devices
- **Slave/Server Functionality**: Process Modbus requests and respond
- **Modern C++20**: Uses modern C++ features for type safety and performance
- **Header-Only Components**: Many utilities are header-only for easy integration
- **Comprehensive Test Suite**: 115+ tests covering all function codes and edge cases

## Architecture

The library is designed with a clear separation of concerns:

```
┌─────────────────────────────────────┐
│   Application Layer                  │
│   (RtuMaster, RtuSlave)             │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   Protocol Layer                    │
│   (RtuFrame, CRC-16)                │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   Transport Layer (Abstract)        │
│   (ByteReader, ByteWriter)          │
└─────────────────────────────────────┘
```

## Quick Start

### Using as a Master/Client

```cpp
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/transport/memory_transport.hpp"

// Create a transport (implement your own ByteTransport for serial/TCP/etc.)
supermb::MemoryTransport transport;  // For testing; use your own transport in production
supermb::RtuMaster master(transport);

// Read holding registers from slave ID 1
auto registers = master.ReadHoldingRegisters(1, 0, 10);
if (registers.has_value()) {
    for (int16_t value : *registers) {
        // Process register value
    }
}

// Write a single register
bool success = master.WriteSingleRegister(1, 0, 42);

// Read coils
auto coils = master.ReadCoils(1, 0, 8);
if (coils.has_value()) {
    // Process coil values
}

// Write multiple registers
std::vector<int16_t> values = {100, 200, 300};
bool success = master.WriteMultipleRegisters(1, 0, values);
```

### Using as a Slave/Server

```cpp
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/rtu/rtu_frame.hpp"

// Create a slave device
supermb::RtuSlave slave(1);  // Slave ID = 1

// Add register ranges
slave.AddHoldingRegisters({0, 100});  // Addresses 0-99
slave.AddInputRegisters({0, 50});     // Addresses 0-49

// Process incoming request (from decoded frame)
auto request = supermb::RtuFrame::DecodeRequest(frame_bytes);
if (request.has_value()) {
    auto response = slave.Process(*request);
    auto response_frame = supermb::RtuFrame::EncodeResponse(response);
    // Send response_frame to transport
}
```

### Implementing Custom Transport

The library intentionally does not include platform-specific transport implementations (serial ports, TCP sockets, etc.). Instead, you implement the `ByteTransport` interface for your specific communication mechanism. This keeps the library portable and allows you to use any I/O mechanism you prefer.

```cpp
#include "super_modbus/transport/byte_reader.hpp"
#include "super_modbus/transport/byte_writer.hpp"

// Example: Serial port transport (you provide the serial port implementation)
class MySerialTransport : public supermb::ByteTransport {
public:
    int Read(std::span<uint8_t> buffer) override {
        // Read bytes from your serial port implementation
        return your_serial_read(buffer.data(), buffer.size());
    }

    int Write(std::span<uint8_t const> data) override {
        // Write bytes to your serial port implementation
        return your_serial_write(data.data(), data.size());
    }

    bool Flush() override {
        // Flush serial port buffers
        return your_serial_flush();
    }

    bool HasData() const override {
        // Check if data is available
        return your_serial_has_data();
    }

    size_t AvailableBytes() const override {
        // Return number of bytes available
        return your_serial_available();
    }
};
```

The `MemoryTransport` class is provided for testing purposes only.

## Implementation Status

### Fully Implemented Function Codes

All standard Modbus RTU function codes are implemented in both master and slave:

- **FC 1**: Read Coils
- **FC 2**: Read Discrete Inputs
- **FC 3**: Read Holding Registers
- **FC 4**: Read Input Registers
- **FC 5**: Write Single Coil
- **FC 6**: Write Single Register
- **FC 7**: Read Exception Status
- **FC 8**: Diagnostics
- **FC 11**: Get Com Event Counter
- **FC 12**: Get Com Event Log
- **FC 15**: Write Multiple Coils
- **FC 16**: Write Multiple Registers
- **FC 17**: Report Slave ID
- **FC 20**: Read File Record
- **FC 21**: Write File Record
- **FC 22**: Mask Write Register
- **FC 23**: Read/Write Multiple Registers
- **FC 24**: Read FIFO Queue

All function codes are fully tested with comprehensive test coverage.

## Building

### Prerequisites

- CMake 3.15 or higher
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Make or Ninja

### Quick Build

```bash
# Using build script (recommended)
./scripts/build.sh

# Or manually
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
make -j$(nproc)
```

### Build Options

```bash
# Debug build with tests
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON

# Release build without examples
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF

# Build specific targets
cd build
make super-modbus-lib          # Just the library
make example_master            # Master example
make example_slave             # Slave example
make example_loopback         # Loopback example
make run_tests                # Test suite
```

### Running Tests

```bash
cd build
ctest --output-on-failure --verbose
```

### Building Examples

```bash
cd build
cmake .. -DBUILD_EXAMPLES=ON
make

# Run examples
./example_master
./example_slave
./example_loopback
./testable_slave /dev/ttyUSB0 9600 1
```

See the `examples/` directory for complete example code demonstrating:
- Master/client usage
- Slave/server usage
- Serial port transport implementation template
- Complete master-slave loopback demonstration
- Testable slave for use with Modbus Poll

## Testing

### Unit Tests

The library includes a comprehensive test suite with 115+ tests covering:
- All function codes (FC 1-24)
- Error handling and exception responses
- Edge cases and boundary conditions
- Integration tests for master-slave communication
- Broadcast support
- File records, FIFO queues, and event logs

Run tests with:
```bash
cd build
ctest --output-on-failure
```

### Testing with Virtual Serial Ports

Virtual serial ports allow you to test your Modbus implementation without physical hardware.

#### Setup (Linux/WSL2)

```bash
# Install socat
sudo apt-get install socat

# Create virtual port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Output: /dev/pts/2 and /dev/pts/3
```

**Keep the socat terminal open!** The ports only exist while `socat` is running.

#### Testing Your Slave

```bash
# Terminal 1: Run your slave
cd build
./example_virtual_port_test slave /dev/pts/2 9600 1

# Terminal 2: Test with external master (see below)
```

#### Testing Your Master

```bash
# Terminal 1: Run external slave (e.g., pymodbus)
# Terminal 2: Run your master
cd build
./example_virtual_port_test master /dev/pts/2 9600 1
```

### Testing with mbpoll

`mbpoll` is a command-line Modbus master tool perfect for testing your slave implementation.

#### Installation

```bash
sudo apt-get install mbpoll
```

#### Basic Usage

**IMPORTANT**: Always use the `-0` flag for 0-based addressing (PDU addressing)!

```bash
# Read 10 holding registers starting at address 0
mbpoll -m rtu -b 9600 -P even -a 1 -0 -r 0 -c 10 /dev/pts/3

# Write value 1234 to register 0 (value goes AFTER device)
mbpoll -m rtu -b 9600 -P even -a 1 -0 -r 0 /dev/pts/3 1234

# Read coils
mbpoll -m rtu -b 9600 -P even -a 1 -0 -r 0 -c 8 -t 1 /dev/pts/3

# Turn on coil 0 (0xFF00 = ON, 0x0000 = OFF)
mbpoll -m rtu -b 9600 -P even -a 1 -0 -r 0 -t 1 /dev/pts/3 0xFF00
```

#### Common mbpoll Options

| Option | Description | Example |
|--------|-------------|---------|
| `-m rtu` | Modbus mode | `-m rtu` |
| `-b <rate>` | Baud rate | `-b 9600` |
| `-P <parity>` | Parity: `none`, `even`, `odd` | `-P even` |
| `-a <id>` | Slave/Unit ID | `-a 1` |
| `-0` | **Use 0-based addressing (required!)** | `-0` |
| `-r <addr>` | Start address | `-r 0` |
| `-c <count>` | Number of items | `-c 10` |
| `-t <code>` | Function code: 1=coils, 2=discrete, 3=holding, 4=input | `-t 3` |
| Write values | **Positional arguments after device** | `/dev/pts/3 1234` |

#### Troubleshooting mbpoll

**"start reference out of range (0)"**: Use the `-0` flag for 0-based addressing.

**"CRC error" or "Invalid CRC"**:
- Check parity matches (`-P even` must match slave's parity)
- Verify baud rate matches exactly
- Ensure both sides use the same settings

**"Connection timeout"**:
- Verify socat is still running
- Check slave is running on the other port
- Ensure baud rate and slave ID match

### Testing with Modbus Poll (Windows)

To test with Modbus Poll or similar Windows software:

1. **Build the library and examples** (see Building section)
2. **Implement serial transport** (see `examples/example_serial_transport.cpp`)
3. **Set up virtual ports** (use com0com on Windows or bridge WSL2 ports)
4. **Run testable slave**: `./testable_slave <port> <baud> <slave_id>`
5. **Connect Modbus Poll** to the serial port with matching settings:
   - Connection Type: RTU over Serial Port
   - Baud Rate: 9600 (or match your application)
   - Data Bits: 8
   - Parity: Even (or None, depending on your setup)
   - Stop Bits: 1
   - Slave ID: 1 (match your slave configuration)

### Comprehensive Test Scripts

The repository includes automated test scripts:

```bash
# Test slave with mbpoll
./scripts/test_with_mbpoll_comprehensive.sh

# Test master against library's slave
./scripts/test_master_comprehensive.sh
```

## Troubleshooting

### Build Issues

```bash
# Clean build
rm -rf build
mkdir build && cd build
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
```

### Communication Issues

- **No Response**: Check baud rate, parity, stop bits match exactly
- **Timeout**: Increase timeout in your application or Modbus Poll settings
- **CRC Errors**: Verify serial port settings match exactly on both sides
- **Wrong Slave ID**: Ensure Modbus Poll slave ID matches your slave configuration

## Code Coverage

The project maintains comprehensive test coverage. Coverage reports are generated automatically in CI and can be generated locally:

![Code Coverage Sunburst](https://codecov.io/gh/eschumacher/super_modbus/graphs/sunburst.svg)

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCOVERAGE=ON
make
ctest
# Coverage HTML report will be in build/coverage-*/
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Ensure all tests pass: `cd build && ctest`
5. Run code formatting: `clang-format -i <files>`
6. Submit a pull request

### Git Hooks (Recommended)

The repository includes git hooks to automatically check code formatting before commits and pushes. To install them:

```bash
./scripts/setup-git-hooks.sh
```

This will install:
- **pre-commit hook**: Checks formatting before each commit
- **pre-push hook**: Checks formatting before each push

The hooks will automatically prevent commits/pushes if code is not properly formatted. To format all staged files:

```bash
git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|hpp|h)$' | xargs clang-format -i && git add -u
```

To bypass hooks (not recommended):
```bash
git commit --no-verify
git push --no-verify
```

## License

See LICENSE file for details.
