# Super Modbus Examples

This directory contains example code demonstrating how to use the Super Modbus library.

## Examples

### `example_master.cpp`
Demonstrates how to use the library as a Modbus RTU Master/Client. Shows:
- Reading holding registers and input registers
- Reading coils and discrete inputs
- Writing single and multiple registers/coils
- Broadcast operations
- Advanced function codes (exception status, FIFO queue, mask write, etc.)

### `example_slave.cpp`
Demonstrates how to use the library as a Modbus RTU Slave/Server. Shows:
- Configuring slave address space (registers, coils, etc.)
- Setting up FIFO queues
- Processing incoming Modbus requests

### `example_serial_transport.cpp`
Shows how to implement the `ByteTransport` interface for a serial port. This is a template that you need to adapt to your specific serial port library (termios, Windows API, boost::asio, etc.).

### `example_master_slave_loopback.cpp`
Complete example showing master-slave communication using `MemoryTransport`. This demonstrates:
- Full round-trip communication
- Writing and reading back values
- Multiple operations
- Broadcast operations
- FIFO queue operations

## Building the Examples

To build the examples, add them to your CMakeLists.txt:

```cmake
# Example master
add_executable(example_master examples/example_master.cpp)
target_link_libraries(example_master PRIVATE ${PROJECT_NAME}-lib)

# Example slave
add_executable(example_slave examples/example_slave.cpp)
target_link_libraries(example_slave PRIVATE ${PROJECT_NAME}-lib)

# Example loopback
add_executable(example_loopback examples/example_master_slave_loopback.cpp)
target_link_libraries(example_loopback PRIVATE ${PROJECT_NAME}-lib)
```

Or compile manually:

```bash
# Example master
g++ -std=c++20 -I include examples/example_master.cpp -L build -lsuper-modbus-lib -o example_master

# Example slave
g++ -std=c++20 -I include examples/example_slave.cpp -L build -lsuper-modbus-lib -o example_slave

# Example loopback
g++ -std=c++20 -I include examples/example_master_slave_loopback.cpp -L build -lsuper-modbus-lib -o example_loopback
```

## Using with Real Hardware

To use the library with real serial hardware:

1. **Implement ByteTransport** for your serial port (see `example_serial_transport.cpp`)
2. **Create Master or Slave** with your transport
3. **For Master**: Call read/write methods directly
4. **For Slave**: Poll transport in a loop:
   ```cpp
   while (running) {
     if (slave.Poll(transport)) {
       // Request processed
     }
     std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   ```

## Quick Start

### As a Master

```cpp
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/transport/memory_transport.hpp"

// Create transport (implement your own for serial port)
MySerialTransport transport("/dev/ttyUSB0", 9600);
RtuMaster master(transport);

// Read holding registers
auto registers = master.ReadHoldingRegisters(1, 0, 10);
if (registers.has_value()) {
  // Process registers...
}

// Write a register
bool success = master.WriteSingleRegister(1, 0, 0x1234);
```

### As a Slave

```cpp
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

// Create transport (implement your own for serial port)
MySerialTransport transport("/dev/ttyUSB0", 9600);
RtuSlave slave(1);  // Slave ID = 1

// Configure address space
slave.AddHoldingRegisters({0, 100});
slave.AddCoils({0, 100});

// Poll for requests
while (running) {
  if (slave.Poll(transport)) {
    // Request processed
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

## Notes

- `MemoryTransport` is provided for testing only - use your own transport for real hardware
- The library handles all Modbus protocol details (frame encoding, CRC, timeouts, etc.)
- You only need to implement the `ByteTransport` interface for your I/O mechanism
- All function codes (FC 1-24) are supported
- Broadcast support (slave ID 0) is implemented for write operations
