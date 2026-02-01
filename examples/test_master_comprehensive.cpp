/**
 * @file test_master_comprehensive.cpp
 * @brief Comprehensive end-to-end test of Modbus RTU Master
 *
 * This program tests the master implementation against the library's slave
 * using virtual serial ports. It performs round-trip tests for all major
 * function codes.
 *
 * Usage:
 *   ./test_master_comprehensive <master_port> <slave_port> <baud_rate>
 * <slave_id>
 *
 * Example:
 *   ./test_master_comprehensive /dev/pts/2 /dev/pts/3 9600 1
 */

#include "serial_transport.hpp"
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

using supermb::AddressSpan;
using supermb::RtuMaster;
using supermb::RtuSlave;
using supermb::SerialTransport;

volatile bool g_running = true;
volatile bool g_slave_running = false;

void signal_handler(int signal) {
  (void)signal;
  g_running = false;
}

void RunSlave(const std::string &port, int baud_rate, uint8_t slave_id) {
  SerialTransport transport(port, baud_rate);
  if (!transport.IsOpen()) {
    std::cerr << "Error: Slave failed to open port: " << port << "\n";
    return;
  }

  RtuSlave slave(slave_id);

  // Configure address space
  slave.AddHoldingRegisters({0, 100});
  slave.AddInputRegisters({0, 50});
  slave.AddCoils({0, 100});
  slave.AddDiscreteInputs({0, 50});

  // Set up FIFO queue
  std::vector<int16_t> fifo_data{0x1111, 0x2222, 0x3333, 0x4444};
  slave.SetFIFOQueue(0, fifo_data);

  // Set up file records by writing them first (they'll be stored when master
  // writes) We'll write file 1, record 0 with test data via master later

  g_slave_running = true;

  // Poll loop
  while (g_running) {
    slave.Poll(transport);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  g_slave_running = false;
}

struct TestResult {
  std::string name;
  bool passed;
  std::string error;
};

int main(int argc, const char *argv[]) {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  if (argc < 5) {
    std::cerr << "Usage: " << argv[0] << " <master_port> <slave_port> <baud_rate> <slave_id>\n";
    std::cerr << "Example: " << argv[0] << " /dev/pts/2 /dev/pts/3 9600 1\n";
    return 1;
  }

  std::string master_port = argv[1];
  std::string slave_port = argv[2];
  int baud_rate = std::atoi(argv[3]);
  uint8_t slave_id = static_cast<uint8_t>(std::atoi(argv[4]));

  std::cout << "========================================\n";
  std::cout << "Modbus RTU Master Comprehensive Test\n";
  std::cout << "========================================\n";
  std::cout << "Master port: " << master_port << "\n";
  std::cout << "Slave port:  " << slave_port << "\n";
  std::cout << "Baud rate:   " << baud_rate << "\n";
  std::cout << "Slave ID:    " << static_cast<int>(slave_id) << "\n";
  std::cout << "\n";

  // Start slave in background thread
  std::thread slave_thread(RunSlave, slave_port, baud_rate, slave_id);

  // Wait for slave to start
  int wait_count = 0;
  while (!g_slave_running && wait_count < 50) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    wait_count++;
  }

  if (!g_slave_running) {
    std::cerr << "Error: Slave failed to start\n";
    g_running = false;
    slave_thread.join();
    return 1;
  }

  // Create master transport
  SerialTransport master_transport(master_port, baud_rate);
  if (!master_transport.IsOpen()) {
    std::cerr << "Error: Master failed to open port: " << master_port << "\n";
    g_running = false;
    slave_thread.join();
    return 1;
  }

  RtuMaster master(master_transport);

  std::vector<TestResult> results;
  int total_tests = 0;
  int passed_tests = 0;

  auto run_test = [&](const std::string &name, std::function<bool()> test_func) {
    total_tests++;
    std::cout << "Test " << total_tests << ": " << name << "... ";
    std::cout.flush();

    bool passed = false;
    std::string error;
    try {
      passed = test_func();
    } catch (const std::exception &e) {
      error = e.what();
      passed = false;
    }

    if (passed) {
      std::cout << "✓ PASSED\n";
      passed_tests++;
    } else {
      std::cout << "✗ FAILED";
      if (!error.empty()) {
        std::cout << " (" << error << ")";
      }
      std::cout << "\n";
    }

    results.push_back({name, passed, error});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  };

  std::cout << "\n=== Basic Read Operations ===\n\n";

  // FC 3: Read Holding Registers
  run_test("FC 3: Read Holding Registers (0-9)", [&]() {
    auto regs = master.ReadHoldingRegisters(slave_id, 0, 10);
    return regs.has_value() && regs->size() == 10;
  });

  // FC 4: Read Input Registers
  run_test("FC 4: Read Input Registers (0-4)", [&]() {
    auto regs = master.ReadInputRegisters(slave_id, 0, 5);
    return regs.has_value() && regs->size() == 5;
  });

  // FC 1: Read Coils
  run_test("FC 1: Read Coils (0-7)", [&]() {
    auto coils = master.ReadCoils(slave_id, 0, 8);
    return coils.has_value() && coils->size() == 8;
  });

  // FC 2: Read Discrete Inputs
  run_test("FC 2: Read Discrete Inputs (0-7)", [&]() {
    auto inputs = master.ReadDiscreteInputs(slave_id, 0, 8);
    return inputs.has_value() && inputs->size() == 8;
  });

  std::cout << "\n=== Write Operations (Round-trip) ===\n\n";

  // FC 6: Write Single Register
  run_test("FC 6: Write Single Register (address 0 = 1234)",
           [&]() { return master.WriteSingleRegister(slave_id, 0, 1234); });

  run_test("FC 3: Read back register 0 (should be 1234)", [&]() {
    auto regs = master.ReadHoldingRegisters(slave_id, 0, 1);
    return regs.has_value() && !regs->empty() && (*regs)[0] == 1234;
  });

  // FC 6: Write another register
  run_test("FC 6: Write Single Register (address 1 = 5678)",
           [&]() { return master.WriteSingleRegister(slave_id, 1, 5678); });

  run_test("FC 3: Read back register 1 (should be 5678)", [&]() {
    auto regs = master.ReadHoldingRegisters(slave_id, 1, 1);
    return regs.has_value() && !regs->empty() && (*regs)[0] == 5678;
  });

  // FC 16: Write Multiple Registers
  run_test("FC 16: Write Multiple Registers (addresses 2-4 = 100, 200, 300)", [&]() {
    std::vector<int16_t> values{100, 200, 300};
    return master.WriteMultipleRegisters(slave_id, 2, values);
  });

  run_test("FC 3: Read back registers 2-4 (should be 100, 200, 300)", [&]() {
    auto regs = master.ReadHoldingRegisters(slave_id, 2, 3);
    if (!regs.has_value() || regs->size() != 3) return false;
    return (*regs)[0] == 100 && (*regs)[1] == 200 && (*regs)[2] == 300;
  });

  // FC 5: Write Single Coil
  run_test("FC 5: Write Single Coil (address 0 = ON)", [&]() { return master.WriteSingleCoil(slave_id, 0, true); });

  run_test("FC 1: Read back coil 0 (should be ON)", [&]() {
    auto coils = master.ReadCoils(slave_id, 0, 1);
    return coils.has_value() && !coils->empty() && (*coils)[0] == true;
  });

  // FC 5: Write Single Coil OFF
  run_test("FC 5: Write Single Coil (address 0 = OFF)", [&]() { return master.WriteSingleCoil(slave_id, 0, false); });

  run_test("FC 1: Read back coil 0 (should be OFF)", [&]() {
    auto coils = master.ReadCoils(slave_id, 0, 1);
    return coils.has_value() && !coils->empty() && (*coils)[0] == false;
  });

  // FC 15: Write Multiple Coils
  run_test("FC 15: Write Multiple Coils (addresses 1-3 = ON, OFF, ON)", [&]() {
    std::array<bool, 3> values{true, false, true};
    return master.WriteMultipleCoils(slave_id, 1, std::span<const bool>(values));
  });

  run_test("FC 1: Read back coils 1-3", [&]() {
    auto coils = master.ReadCoils(slave_id, 1, 3);
    if (!coils.has_value() || coils->size() != 3) return false;
    return (*coils)[0] == true && (*coils)[1] == false && (*coils)[2] == true;
  });

  std::cout << "\n=== Advanced Function Codes ===\n\n";

  // FC 7: Read Exception Status
  run_test("FC 7: Read Exception Status", [&]() {
    auto status = master.ReadExceptionStatus(slave_id);
    return status.has_value();
  });

  // FC 8: Diagnostics
  run_test("FC 8: Diagnostics (sub-function 0x0000)", [&]() {
    std::vector<uint8_t> test_data{0x12, 0x34, 0x56};
    auto result = master.Diagnostics(slave_id, 0x0000, test_data);
    return result.has_value();
  });

  // FC 11: Get Com Event Counter
  run_test("FC 11: Get Com Event Counter", [&]() {
    auto result = master.GetComEventCounter(slave_id);
    return result.has_value();
  });

  // FC 12: Get Com Event Log
  run_test("FC 12: Get Com Event Log", [&]() {
    auto result = master.GetComEventLog(slave_id);
    return result.has_value();
  });

  // FC 17: Report Slave ID
  run_test("FC 17: Report Slave ID", [&]() {
    auto result = master.ReportSlaveID(slave_id);
    return result.has_value() && !result->empty() && (*result)[0] == slave_id;
  });

  // FC 22: Mask Write Register
  run_test("FC 22: Mask Write Register (address 5, AND=0xFF00, OR=0x0056)", [&]() {
    // First set initial value
    if (!master.WriteSingleRegister(slave_id, 5, 0x1234)) return false;
    // Then mask write
    return master.MaskWriteRegister(slave_id, 5, 0xFF00, 0x0056);
  });

  run_test("FC 3: Read back register 5 (verify mask write)", [&]() {
    auto regs = master.ReadHoldingRegisters(slave_id, 5, 1);
    if (!regs.has_value() || regs->empty()) return false;
    // Result: (0x1234 & 0xFF00) | 0x0056 = 0x1256
    return (*regs)[0] == 0x1256;
  });

  // FC 23: Read/Write Multiple Registers
  run_test("FC 23: Read/Write Multiple Registers (read 10-11, write 20-21)", [&]() {
    // Set initial values
    if (!master.WriteSingleRegister(slave_id, 10, 100)) return false;
    if (!master.WriteSingleRegister(slave_id, 11, 200)) return false;
    // Read/Write
    std::vector<int16_t> write_values{300, 400};
    auto read_values = master.ReadWriteMultipleRegisters(slave_id, 10, 2, 20, write_values);
    if (!read_values.has_value() || read_values->size() != 2) return false;
    // Verify reads
    if ((*read_values)[0] != 100 || (*read_values)[1] != 200) return false;
    // Verify writes
    auto verify = master.ReadHoldingRegisters(slave_id, 20, 2);
    return verify.has_value() && verify->size() == 2 && (*verify)[0] == 300 && (*verify)[1] == 400;
  });

  // FC 24: Read FIFO Queue
  run_test("FC 24: Read FIFO Queue (address 0)", [&]() {
    auto fifo = master.ReadFIFOQueue(slave_id, 0);
    if (!fifo.has_value() || fifo->size() != 4) return false;
    // Verify FIFO data
    return (*fifo)[0] == 0x1111 && (*fifo)[1] == 0x2222 && (*fifo)[2] == 0x3333 && (*fifo)[3] == 0x4444;
  });

  std::cout << "\n=== File Record Operations ===\n\n";

  // FC 21: Write File Record
  run_test("FC 21: Write File Record (file 1, record 0)", [&]() {
    std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records;
    std::vector<int16_t> record_data{static_cast<int16_t>(0xABCD), static_cast<int16_t>(0x1234),
                                     static_cast<int16_t>(0x5678)};
    file_records.push_back({1, 0, record_data});
    return master.WriteFileRecord(slave_id, file_records);
  });

  // FC 20: Read File Record
  run_test("FC 20: Read File Record (file 1, record 0)", [&]() {
    std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records;
    file_records.push_back({1, 0, 3});  // File 1, Record 0, Length 3
    auto result = master.ReadFileRecord(slave_id, file_records);
    if (!result.has_value()) return false;
    auto it = result->find({1, 0});
    if (it == result->end() || it->second.size() != 3) return false;
    // Verify data
    return it->second[0] == static_cast<int16_t>(0xABCD) && it->second[1] == static_cast<int16_t>(0x1234) &&
           it->second[2] == static_cast<int16_t>(0x5678);
  });

  std::cout << "\n=== Edge Cases and Error Handling ===\n\n";

  // Test reading from invalid address (should handle gracefully)
  run_test("FC 3: Read from invalid address (should fail gracefully)", [&]() {
    auto regs = master.ReadHoldingRegisters(slave_id, 1000, 5);
    // Should return empty (invalid address)
    return !regs.has_value();
  });

  // Test writing to invalid address
  run_test("FC 6: Write to invalid address (should fail gracefully)", [&]() {
    bool result = master.WriteSingleRegister(slave_id, 1000, 1234);
    // Should return false (invalid address)
    return !result;
  });

  // Test reading large number of registers
  run_test("FC 3: Read large number of registers (20 registers)", [&]() {
    auto regs = master.ReadHoldingRegisters(slave_id, 0, 20);
    return regs.has_value() && regs->size() == 20;
  });

  // Test writing multiple registers (larger set)
  run_test("FC 16: Write Multiple Registers (10 registers)", [&]() {
    std::vector<int16_t> values;
    for (int i = 0; i < 10; ++i) {
      values.push_back(static_cast<int16_t>(i * 100));
    }
    return master.WriteMultipleRegisters(slave_id, 30, values);
  });

  run_test("FC 3: Read back 10 registers (verify large write)", [&]() {
    auto regs = master.ReadHoldingRegisters(slave_id, 30, 10);
    if (!regs.has_value() || regs->size() != 10) return false;
    for (int i = 0; i < 10; ++i) {
      if ((*regs)[i] != i * 100) return false;
    }
    return true;
  });

  // Test reading multiple coils (larger set)
  run_test("FC 1: Read multiple coils (16 coils)", [&]() {
    auto coils = master.ReadCoils(slave_id, 0, 16);
    return coils.has_value() && coils->size() == 16;
  });

  // Test writing multiple coils (larger set)
  run_test("FC 15: Write Multiple Coils (8 coils)", [&]() {
    std::array<bool, 8> values{true, false, true, false, true, true, false, true};
    return master.WriteMultipleCoils(slave_id, 10, std::span<const bool>(values));
  });

  run_test("FC 1: Read back 8 coils (verify large write)", [&]() {
    auto coils = master.ReadCoils(slave_id, 10, 8);
    if (!coils.has_value() || coils->size() != 8) return false;
    std::vector<bool> expected{true, false, true, false, true, true, false, true};
    for (size_t i = 0; i < 8; ++i) {
      if ((*coils)[i] != expected[i]) return false;
    }
    return true;
  });

  // Test GetComEventCounter returns valid data
  run_test("FC 11: Get Com Event Counter (verify structure)", [&]() {
    auto result = master.GetComEventCounter(slave_id);
    if (!result.has_value()) return false;
    // Should return pair of (status, event_count)
    return true;
  });

  // Test GetComEventLog returns valid data
  run_test("FC 12: Get Com Event Log (verify structure)", [&]() {
    auto result = master.GetComEventLog(slave_id);
    if (!result.has_value()) return false;
    // Should return at least status, event_count, message_count
    return result->size() >= 5;
  });

  // Test ReportSlaveID returns correct slave ID
  run_test("FC 17: Report Slave ID (verify slave ID matches)", [&]() {
    auto result = master.ReportSlaveID(slave_id);
    if (!result.has_value() || result->empty()) return false;
    // First byte should be slave ID
    return (*result)[0] == slave_id;
  });

  std::cout << "\n=== Test Summary ===\n";
  std::cout << "Total tests: " << total_tests << "\n";
  std::cout << "Passed: " << passed_tests << "\n";
  std::cout << "Failed: " << (total_tests - passed_tests) << "\n";
  std::cout << "\n";

  // Stop slave
  g_running = false;
  slave_thread.join();

  return (passed_tests == total_tests) ? 0 : 1;
}
