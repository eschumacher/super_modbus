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

// For real hardware, use a ByteTransport (e.g. SerialTransport from examples).

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

  [[maybe_unused]] MemoryTransport transport;

  std::cout << "\nSlave ready! Waiting for Modbus requests...\n";
  std::cout << "With real hardware, run slave.Poll(transport) in a loop.\n\n";

  std::cout << "Example: Processing a read holding registers request...\n";
  // Normally frames come from transport; here we show Process() directly.
  std::cout << "\nIn production, use slave.Poll(transport) or ProcessIncomingFrame().\n";

  std::cout << "\nSlave configuration complete!\n";
  std::cout << "To use with real hardware:\n";
  std::cout << "  1. Implement ByteTransport for your serial port\n";
  std::cout << "  2. Create RtuSlave with your transport\n";
  std::cout << "  3. Poll transport in a loop: slave.Poll(transport)\n";
  std::cout << "  4. Or use: slave.ProcessIncomingFrame(transport, timeout_ms)\n";

  return 0;
}
