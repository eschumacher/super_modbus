/**
 * @file example_virtual_port_test.cpp
 * @brief Example demonstrating how to use virtual serial ports for testing
 *
 * This example shows how to:
 * 1. Use the SerialTransport with virtual serial ports created by socat
 * 2. Test your Modbus master or slave with external Modbus software
 *
 * Setup:
 *   1. Install socat: sudo apt-get install socat
 *   2. Create virtual ports: socat -d -d pty,raw,echo=0 pty,raw,echo=0
 *   3. Note the two /dev/pts/X paths from socat output
 *   4. Run this example on one port (e.g., /dev/pts/2)
 *   5. Connect external Modbus software to the other port (e.g., /dev/pts/3)
 *
 * Usage:
 *   ./example_virtual_port_test master /dev/pts/2 9600 1
 *   ./example_virtual_port_test slave /dev/pts/2 9600 1
 */

#include "serial_transport.hpp"
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>

using supermb::AddressSpan;
using supermb::RtuMaster;
using supermb::RtuSlave;
using supermb::SerialTransport;

volatile bool g_running = true;

void signal_handler([[maybe_unused]] int signal) {
  g_running = false;
}

void RunMaster(const std::string &port, int baud_rate, uint8_t slave_id) {
  std::cout << "=== Modbus Master Mode ===\n";
  std::cout << "Port: " << port << "\n";
  std::cout << "Baud Rate: " << baud_rate << "\n";
  std::cout << "Target Slave ID: " << static_cast<int>(slave_id) << "\n";
  std::cout << "\n";

  // Create serial transport
  SerialTransport transport(port, baud_rate);
  if (!transport.IsOpen()) {
    std::cerr << "Error: Failed to open serial port: " << port << "\n";
    std::cerr << "Make sure:\n";
    std::cerr << "  1. Virtual ports are created with: socat -d -d pty,raw,echo=0 pty,raw,echo=0\n";
    std::cerr << "  2. The port path is correct (e.g., /dev/pts/2)\n";
    std::cerr << "  3. You have permission to access the port\n";
    return;
  }

  std::cout << "Serial port opened successfully!\n";
  std::cout << "\n";

  // Create master
  RtuMaster master(transport);

  // Test operations
  std::cout << "Testing Modbus operations...\n";
  std::cout << "\n";

  // Read holding registers
  std::cout << "1. Reading holding registers 0-9 from slave " << static_cast<int>(slave_id) << "...\n";
  auto registers = master.ReadHoldingRegisters(slave_id, 0, 10);
  if (registers.has_value()) {
    std::cout << "   Success! Read " << registers->size() << " registers:\n";
    for (size_t i = 0; i < registers->size() && i < 10; ++i) {
      std::cout << "     Register " << i << ": " << (*registers)[i] << "\n";
    }
  } else {
    std::cout << "   Failed or timeout (no slave responding)\n";
  }
  std::cout << "\n";

  // Write single register
  std::cout << "2. Writing register 0 = 1234...\n";
  bool write_ok = master.WriteSingleRegister(slave_id, 0, 1234);
  if (write_ok) {
    std::cout << "   Success!\n";
  } else {
    std::cout << "   Failed or timeout\n";
  }
  std::cout << "\n";

  // Read it back
  std::cout << "3. Reading register 0 back...\n";
  auto reg = master.ReadHoldingRegisters(slave_id, 0, 1);
  if (reg.has_value() && !reg->empty()) {
    std::cout << "   Register 0 = " << (*reg)[0] << "\n";
  } else {
    std::cout << "   Failed or timeout\n";
  }
  std::cout << "\n";

  std::cout << "Master test complete!\n";
}

void RunSlave(const std::string &port, int baud_rate, uint8_t slave_id) {
  std::cout << "=== Modbus Slave Mode ===\n";
  std::cout << "Port: " << port << "\n";
  std::cout << "Baud Rate: " << baud_rate << "\n";
  std::cout << "Slave ID: " << static_cast<int>(slave_id) << "\n";
  std::cout << "\n";

  // Create serial transport
  SerialTransport transport(port, baud_rate);
  if (!transport.IsOpen()) {
    std::cerr << "Error: Failed to open serial port: " << port << "\n";
    std::cerr << "Make sure:\n";
    std::cerr << "  1. Virtual ports are created with: socat -d -d pty,raw,echo=0 pty,raw,echo=0\n";
    std::cerr << "  2. The port path is correct (e.g., /dev/pts/2)\n";
    std::cerr << "  3. You have permission to access the port\n";
    return;
  }

  std::cout << "Serial port opened successfully!\n";
  std::cout << "\n";

  // Create slave
  RtuSlave slave(slave_id);

  // Configure address space
  slave.AddHoldingRegisters({0, 100});
  slave.AddInputRegisters({0, 50});
  slave.AddCoils({0, 100});
  slave.AddDiscreteInputs({0, 50});

  std::cout << "Slave configured:\n";
  std::cout << "  Holding Registers: 0-99\n";
  std::cout << "  Input Registers: 0-49\n";
  std::cout << "  Coils: 0-99\n";
  std::cout << "  Discrete Inputs: 0-49\n";
  std::cout << "\n";
  std::cout << "Slave running... Press Ctrl+C to stop\n";
  std::cout << "\n";

  // Initialize some test values
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
}

int main(int argc, const char *argv[]) {
  // Set up signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Parse arguments
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " <master|slave> <port> <baud_rate> [slave_id]\n";
    std::cerr << "\n";
    std::cerr << "Arguments:\n";
    std::cerr << "  master|slave  Mode to run in\n";
    std::cerr << "  port          Serial port path (e.g., /dev/pts/2)\n";
    std::cerr << "  baud_rate     Baud rate (e.g., 9600, 19200)\n";
    std::cerr << "  slave_id      Slave ID (default: 1)\n";
    std::cerr << "\n";
    std::cerr << "Examples:\n";
    std::cerr << "  # Run as slave on virtual port\n";
    std::cerr << "  " << argv[0] << " slave /dev/pts/2 9600 1\n";
    std::cerr << "\n";
    std::cerr << "  # Run as master on virtual port\n";
    std::cerr << "  " << argv[0] << " master /dev/pts/2 9600 1\n";
    std::cerr << "\n";
    std::cerr << "Setup:\n";
    std::cerr << "  1. Create virtual ports: socat -d -d pty,raw,echo=0 pty,raw,echo=0\n";
    std::cerr << "  2. Note the two /dev/pts/X paths from output\n";
    std::cerr << "  3. Run this example on one port\n";
    std::cerr << "  4. Connect external Modbus software to the other port\n";
    return 1;
  }

  std::string mode = argv[1];
  std::string port = argv[2];
  int baud_rate = std::atoi(argv[3]);
  uint8_t slave_id = (argc >= 5) ? static_cast<uint8_t>(std::atoi(argv[4])) : 1;

  if (mode == "master") {
    RunMaster(port, baud_rate, slave_id);
  } else if (mode == "slave") {
    RunSlave(port, baud_rate, slave_id);
  } else {
    std::cerr << "Error: Mode must be 'master' or 'slave'\n";
    return 1;
  }

  return 0;
}
