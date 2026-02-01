/**
 * @file testable_slave.cpp
 * @brief Testable Modbus RTU Slave for use with Modbus Poll
 *
 * This is a complete, ready-to-use slave application that can be tested
 * with Modbus Poll or similar Modbus testing software.
 *
 * Usage:
 *   ./testable_slave <serial_port> [baud_rate] [slave_id]
 *
 * Example:
 *   ./testable_slave /dev/ttyUSB0 9600 1
 *
 * To use with real hardware, implement a ByteTransport for your serial port.
 * See example_serial_transport.cpp for a template.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

// Global flag for graceful shutdown
bool volatile g_running = true;

void signal_handler(int signal) {
  (void)signal;
  g_running = false;
}

int main(int argc, char *argv[]) {
  using supermb::AddressSpan;
  using supermb::MemoryTransport;
  using supermb::RtuSlave;

  // Parse command line arguments
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <serial_port> [baud_rate] [slave_id]\n";
    std::cerr << "\n";
    std::cerr << "Arguments:\n";
    std::cerr << "  serial_port  Serial port path (e.g., /dev/ttyUSB0 or COM3)\n";
    std::cerr << "  baud_rate    Baud rate (default: 9600)\n";
    std::cerr << "  slave_id     Modbus slave ID (default: 1)\n";
    std::cerr << "\n";
    std::cerr << "Examples:\n";
    std::cerr << "  " << argv[0] << " /dev/ttyUSB0\n";
    std::cerr << "  " << argv[0] << " /dev/ttyUSB0 19200\n";
    std::cerr << "  " << argv[0] << " /dev/ttyUSB0 9600 5\n";
    std::cerr << "\n";
    std::cerr << "Note: This example uses MemoryTransport for demonstration.\n";
    std::cerr << "      For real hardware, implement ByteTransport for your serial port.\n";
    return 1;
  }

  char const *port = argv[1];
  int baud_rate = (argc >= 3) ? std::atoi(argv[2]) : 9600;
  uint8_t slave_id = (argc >= 4) ? static_cast<uint8_t>(std::atoi(argv[3])) : 1;

  // Set up signal handlers for graceful shutdown
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Create transport
  // TODO: Replace with your serial port transport implementation
  // MySerialTransport transport(port, baud_rate);
  MemoryTransport transport;  // Placeholder - replace with real serial transport

  // Create slave
  RtuSlave slave(slave_id);

  // Configure address space
  std::cout << "Configuring Modbus RTU Slave...\n";
  std::cout << "  Slave ID: " << static_cast<int>(slave_id) << "\n";

  slave.AddHoldingRegisters({0, 100});
  std::cout << "  Holding Registers: 0-99 (read/write)\n";

  slave.AddInputRegisters({0, 50});
  std::cout << "  Input Registers: 0-49 (read-only)\n";

  slave.AddCoils({0, 100});
  std::cout << "  Coils: 0-99 (read/write)\n";

  slave.AddDiscreteInputs({0, 50});
  std::cout << "  Discrete Inputs: 0-49 (read-only)\n";

  // Set up FIFO queue at address 0
  std::vector<int16_t> fifo_data{0x1111, 0x2222, 0x3333, 0x4444};
  slave.SetFIFOQueue(0, fifo_data);
  std::cout << "  FIFO Queue at address 0: " << fifo_data.size() << " entries\n";

  std::cout << "\n";
  std::cout << "Connection:\n";
  std::cout << "  Port: " << port << "\n";
  std::cout << "  Baud Rate: " << baud_rate << "\n";
  std::cout << "\n";
  std::cout << "Modbus Poll Configuration:\n";
  std::cout << "  Connection: RTU over Serial Port\n";
  std::cout << "  Serial Port: " << port << "\n";
  std::cout << "  Baud Rate: " << baud_rate << "\n";
  std::cout << "  Data Bits: 8\n";
  std::cout << "  Parity: Even (or None)\n";
  std::cout << "  Stop Bits: 1\n";
  std::cout << "  Slave ID: " << static_cast<int>(slave_id) << "\n";
  std::cout << "\n";
  std::cout << "Slave running... Press Ctrl+C to stop\n";
  std::cout << "\n";

  // Initialize some register values for testing
  // In a real application, these would come from your hardware/application
  // For demonstration, we'll set some initial values
  {
    using supermb::FunctionCode;
    using supermb::RtuRequest;
    RtuRequest init_req{{slave_id, FunctionCode::kWriteSingleReg}};
    init_req.SetWriteSingleRegisterData(0, 0x1234);
    slave.Process(init_req);
    init_req.SetWriteSingleRegisterData(1, 0x5678);
    slave.Process(init_req);
  }

  // Poll loop
  uint32_t request_count = 0;
  while (g_running) {
    if (slave.Poll(transport)) {
      ++request_count;
      if (request_count % 10 == 0) {
        std::cout << "Processed " << request_count << " requests\n";
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::cout << "\nShutting down...\n";
  std::cout << "Total requests processed: " << request_count << "\n";
  return 0;
}
