# Super Modbus

A modern C++ Modbus library designed for easy integration into any project. The library provides a clean abstraction layer that separates the Modbus protocol logic from the underlying communication mechanism.

## Features

- **Transport Abstraction**: Works with any byte I/O mechanism (serial, TCP, memory, etc.) through a simple interface
- **Modbus RTU Support**: Full RTU frame encoding/decoding with CRC-16 verification
- **Master/Client Functionality**: Read and write registers from Modbus devices
- **Slave/Server Functionality**: Process Modbus requests and respond
- **Modern C++20**: Uses modern C++ features for type safety and performance
- **Header-Only Components**: Many utilities are header-only for easy integration

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

// Example: TCP socket transport
class MyTcpTransport : public supermb::ByteTransport {
    // Similar implementation using your TCP socket library
};
```

The `MemoryTransport` class is provided for testing purposes only.

## Current Status

### Implemented
- ✅ Transport abstraction layer (`ByteReader`, `ByteWriter`, `ByteTransport`)
- ✅ Memory-based transport for testing (`MemoryTransport`)
- ✅ RTU frame encoding/decoding with CRC-16 verification
- ✅ Modbus RTU Master/Client (`RtuMaster`) - All standard function codes
- ✅ Modbus RTU Slave/Server (`RtuSlave`) - All standard function codes
- ✅ **All Standard Modbus Function Codes:**
  - FC 1: Read Coils
  - FC 2: Read Discrete Inputs
  - FC 3: Read Holding Registers
  - FC 4: Read Input Registers
  - FC 5: Write Single Coil
  - FC 6: Write Single Register
  - FC 7: Read Exception Status
  - FC 8: Diagnostics
  - FC 11: Get Com Event Counter
  - FC 12: Get Com Event Log
  - FC 15: Write Multiple Coils
  - FC 16: Write Multiple Registers
  - FC 17: Report Slave ID
  - FC 22: Mask Write Register
  - FC 23: Read/Write Multiple Registers
  - FC 24: Read FIFO Queue
- ✅ Comprehensive test suite (115+ tests)
- ✅ Error handling and exception responses
- ✅ Frame timeout handling

### Limitations
- ⚠️ FC 20 (Read File Record): Returns `kIllegalFunction` - not implemented
- ⚠️ FC 21 (Write File Record): Returns `kIllegalFunction` - not implemented
- ⚠️ FC 24 (Read FIFO Queue): Simplified implementation (treats FIFO as regular register)

### Future Enhancements
- 🔄 Full File Record support (FC 20/21)
- 🔄 Enhanced FIFO Queue implementation (FC 24)
- 🔄 Additional error handling improvements

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

### Building Examples

```bash
# Using build script (recommended)
./scripts/build.sh

# Or manually
cd build
cmake .. -DBUILD_EXAMPLES=ON
make

# Run examples
./example_master
./example_slave
./example_loopback
./testable_slave /dev/ttyUSB0 9600 1  # For testing with Modbus Poll
```

See the `examples/` directory for complete example code demonstrating:
- Master/client usage
- Slave/server usage
- Serial port transport implementation template
- Complete master-slave loopback demonstration
- Testable slave for use with Modbus Poll

### Testing with Modbus Poll

To test your slave implementation with Modbus Poll or similar software:

1. **Build the library and examples** (see above)
2. **Implement serial transport** (see `examples/example_serial_transport.cpp`)
3. **Run testable slave**: `./testable_slave <port> <baud> <slave_id>`
4. **Connect Modbus Poll** to the serial port with matching settings

See `BUILD_AND_TEST.md` and `scripts/test_with_modbus_poll.md` for detailed instructions.

## License

See LICENSE file for details.
