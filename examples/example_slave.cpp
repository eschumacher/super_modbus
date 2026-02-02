/**
 * @file example_slave.cpp
 * @brief Example Modbus RTU Slave/Server implementation
 *
 * This example demonstrates how to use the Super Modbus library as a Modbus RTU Slave.
 * The slave processes incoming Modbus requests and responds appropriately.
 *
 * To use this with real hardware, implement a ByteTransport for your serial port.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

// In a real application, you would implement your own transport:
// class SerialTransport : public supermb::ByteTransport { ... };

int main() {
  using supermb::AddressSpan;
  using supermb::MemoryTransport;
  using supermb::RtuSlave;

  // Create a Modbus slave with ID 1
  RtuSlave slave(1);

  // Configure the slave's address space
  std::cout << "Configuring slave address space...\n";

  // Add holding registers (read/write) at addresses 0-99
  slave.AddHoldingRegisters({0, 100});
  std::cout << "  Added holding registers: 0-99\n";

  // Add input registers (read-only) at addresses 0-49
  slave.AddInputRegisters({0, 50});
  std::cout << "  Added input registers: 0-49\n";

  // Add coils (read/write) at addresses 0-99
  slave.AddCoils({0, 100});
  std::cout << "  Added coils: 0-99\n";

  // Add discrete inputs (read-only) at addresses 0-49
  slave.AddDiscreteInputs({0, 50});
  std::cout << "  Added discrete inputs: 0-49\n";

  // Set up FIFO queue at address 0
  std::vector<int16_t> fifo_data{0x1111, 0x2222, 0x3333};
  slave.SetFIFOQueue(0, fifo_data);
  std::cout << "  Set up FIFO queue at address 0 with " << fifo_data.size() << " entries\n";

  // Create transport (in production, use your serial port transport)
  [[maybe_unused]] MemoryTransport transport;  // Used in real app: slave.Poll(transport) in a loop

  std::cout << "\nSlave ready! Waiting for Modbus requests...\n";
  std::cout << "In a real application, this would poll the transport continuously.\n\n";

  // Example: Process incoming frames
  // In a real application, you would do this in a loop:
  //
  // while (running) {
  //   if (slave.Poll(transport)) {
  //     // Frame processed
  //   }
  //   std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // }

  // For demonstration, show how to process a frame manually
  std::cout << "Example: Processing a read holding registers request...\n";

  // In a real application, frames would come from the transport.
  // Here we demonstrate the Process() method directly:

  // Create a sample request (normally decoded from transport)
  // This is just for demonstration - in real use, frames come from transport
  std::cout << "\nNote: In real use, frames are automatically decoded from transport.\n";
  std::cout << "The slave.ProcessIncomingFrame() or slave.Poll() methods handle this.\n";

  std::cout << "\nSlave configuration complete!\n";
  std::cout << "To use with real hardware:\n";
  std::cout << "  1. Implement ByteTransport for your serial port\n";
  std::cout << "  2. Create RtuSlave with your transport\n";
  std::cout << "  3. Poll transport in a loop: slave.Poll(transport)\n";
  std::cout << "  4. Or use: slave.ProcessIncomingFrame(transport, timeout_ms)\n";

  return 0;
}
