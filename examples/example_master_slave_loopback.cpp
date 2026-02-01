/**
 * @file example_master_slave_loopback.cpp
 * @brief Complete example showing master-slave communication
 *
 * This example demonstrates a complete Modbus RTU communication scenario
 * using MemoryTransport to simulate communication between a master and slave.
 * This is useful for testing and understanding the library behavior.
 */

#include <iostream>
#include <vector>
#include "super_modbus/rtu/rtu_frame.hpp"
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

int main() {
  using supermb::AddressSpan;
  using supermb::MemoryTransport;
  using supermb::RtuFrame;
  using supermb::RtuMaster;
  using supermb::RtuRequest;
  using supermb::RtuSlave;

  std::cout << "=== Modbus RTU Master-Slave Loopback Example ===\n\n";

  // Create shared transport (in real use, this would be a serial port)
  MemoryTransport transport;

  // Create slave with ID 1
  RtuSlave slave(1);
  slave.AddHoldingRegisters({0, 100});
  slave.AddInputRegisters({0, 50});
  slave.AddCoils({0, 100});
  slave.AddDiscreteInputs({0, 50});

  // Create master
  RtuMaster master(transport);

  // Helper function to simulate master-slave communication
  auto simulate_communication = [&]() {
    // Get master's request from transport
    auto master_request = transport.GetWrittenData();
    if (master_request.empty()) {
      return false;
    }

    // Put master request in transport for slave to read
    transport.ClearWriteBuffer();
    transport.SetReadData(master_request);
    transport.ResetReadPosition();

    // Slave processes request and writes response
    slave.ProcessIncomingFrame(transport, 100);

    // Get slave's response
    auto slave_response = transport.GetWrittenData();

    // Put slave response in transport for master to read
    transport.ClearWriteBuffer();
    transport.SetReadData(slave_response);
    transport.ResetReadPosition();

    return true;
  };

  // Example 1: Master writes, then reads
  std::cout << "Example 1: Write then read holding register\n";
  std::cout << "  Master writing register 0 = 0x1234...\n";
  bool success = master.WriteSingleRegister(1, 0, 0x1234);
  simulate_communication();
  if (success) {
    std::cout << "  Write successful!\n";
  }

  std::cout << "  Master reading register 0...\n";
  auto registers = master.ReadHoldingRegisters(1, 0, 1);
  simulate_communication();
  if (registers.has_value() && !registers->empty()) {
    std::cout << "  Read value: 0x" << std::hex << (*registers)[0] << std::dec << "\n";
    if ((*registers)[0] == 0x1234) {
      std::cout << "  ✓ Value matches!\n";
    }
  }

  // Example 2: Master writes multiple registers
  std::cout << "\nExample 2: Write multiple registers\n";
  std::vector<int16_t> values{100, 200, 300, 400, 500};
  std::cout << "  Master writing " << values.size() << " registers...\n";
  success = master.WriteMultipleRegisters(1, 0, values);
  simulate_communication();
  if (success) {
    std::cout << "  Write successful!\n";
  }

  // Example 3: Master reads multiple registers
  std::cout << "\nExample 3: Read multiple registers\n";
  std::cout << "  Master reading 5 registers...\n";
  registers = master.ReadHoldingRegisters(1, 0, 5);
  simulate_communication();
  if (registers.has_value()) {
    std::cout << "  Read " << registers->size() << " registers:\n";
    for (size_t i = 0; i < registers->size(); ++i) {
      std::cout << "    Register[" << i << "] = " << (*registers)[i] << "\n";
    }
  }

  // Example 4: Master reads coils
  std::cout << "\nExample 4: Read coils\n";
  std::cout << "  Master reading 8 coils...\n";
  auto coils = master.ReadCoils(1, 0, 8);
  simulate_communication();
  if (coils.has_value()) {
    std::cout << "  Read " << coils->size() << " coils:\n";
    for (size_t i = 0; i < coils->size(); ++i) {
      std::cout << "    Coil[" << i << "] = " << ((*coils)[i] ? "ON" : "OFF") << "\n";
    }
  }

  // Example 5: Master writes coils
  std::cout << "\nExample 5: Write multiple coils\n";
  std::vector<bool> coil_values{true, false, true, false, true, false, true, false};
  std::cout << "  Master writing " << coil_values.size() << " coils...\n";
  // Convert vector<bool> to regular bool array for span
  bool bool_array[8] = {coil_values[0], coil_values[1], coil_values[2], coil_values[3],
                        coil_values[4], coil_values[5], coil_values[6], coil_values[7]};
  success = master.WriteMultipleCoils(1, 0, std::span<bool const>(bool_array));
  simulate_communication();
  if (success) {
    std::cout << "  Write successful!\n";
  }

  // Example 6: Master reads coils back
  std::cout << "\nExample 6: Read coils back\n";
  coils = master.ReadCoils(1, 0, 8);
  simulate_communication();
  if (coils.has_value()) {
    std::cout << "  Read " << coils->size() << " coils:\n";
    for (size_t i = 0; i < coils->size(); ++i) {
      bool expected = (i % 2 == 0);
      std::cout << "    Coil[" << i << "] = " << ((*coils)[i] ? "ON" : "OFF");
      if ((*coils)[i] == expected) {
        std::cout << " ✓";
      } else {
        std::cout << " ✗ (expected " << (expected ? "ON" : "OFF") << ")";
      }
      std::cout << "\n";
    }
  }

  // Example 7: Broadcast write
  std::cout << "\nExample 7: Broadcast write (slave ID 0)\n";
  std::cout << "  Master broadcasting write to all slaves...\n";
  success = master.WriteSingleRegister(0, 0, 0xABCD);  // Broadcast
  // Note: Broadcast doesn't get a response, so no need to simulate
  if (success) {
    std::cout << "  Broadcast successful (no response expected)\n";
  }

  // Example 8: Read input registers
  std::cout << "\nExample 8: Read input registers\n";
  std::cout << "  Master reading 5 input registers...\n";
  auto input_regs = master.ReadInputRegisters(1, 0, 5);
  simulate_communication();
  if (input_regs.has_value()) {
    std::cout << "  Read " << input_regs->size() << " input registers\n";
  }

  // Example 9: Read exception status
  std::cout << "\nExample 9: Read exception status\n";
  auto exception_status = master.ReadExceptionStatus(1);
  simulate_communication();
  if (exception_status.has_value()) {
    std::cout << "  Exception status: 0x" << std::hex << static_cast<int>(*exception_status) << std::dec << "\n";
  }

  // Example 10: Read FIFO queue
  std::cout << "\nExample 10: Read FIFO queue\n";
  // Set up FIFO queue in slave
  std::vector<int16_t> fifo_data{0x1111, 0x2222, 0x3333, 0x4444};
  slave.SetFIFOQueue(0, fifo_data);
  std::cout << "  Slave FIFO queue configured with " << fifo_data.size() << " entries\n";

  std::cout << "  Master reading FIFO queue...\n";
  auto fifo_result = master.ReadFIFOQueue(1, 0);
  simulate_communication();
  if (fifo_result.has_value()) {
    std::cout << "  Read " << fifo_result->size() << " values from FIFO:\n";
    for (size_t i = 0; i < fifo_result->size(); ++i) {
      std::cout << "    FIFO[" << i << "] = 0x" << std::hex << (*fifo_result)[i] << std::dec << "\n";
    }
  }

  std::cout << "\n=== All examples completed successfully! ===\n";
  return 0;
}
