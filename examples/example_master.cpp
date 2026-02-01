/**
 * @file example_master.cpp
 * @brief Example Modbus RTU Master/Client implementation
 *
 * This example demonstrates how to use the Super Modbus library as a Modbus RTU Master.
 * The master can read and write to Modbus slave devices.
 *
 * To use this with real hardware, implement a ByteTransport for your serial port.
 */

#include <iostream>
#include <vector>
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/transport/memory_transport.hpp"

// In a real application, you would implement your own transport:
// class SerialTransport : public supermb::ByteTransport { ... };

int main() {
  using supermb::MemoryTransport;
  using supermb::RtuMaster;

  // Create transport (in production, use your serial port transport)
  MemoryTransport transport;
  RtuMaster master(transport);

  // Example 1: Read holding registers from slave ID 1
  std::cout << "Example 1: Reading holding registers from slave 1...\n";
  auto registers = master.ReadHoldingRegisters(1, 0, 10);
  if (registers.has_value()) {
    std::cout << "  Read " << registers->size() << " registers:\n";
    for (size_t i = 0; i < registers->size(); ++i) {
      std::cout << "    Register[" << i << "] = " << (*registers)[i] << "\n";
    }
  } else {
    std::cout << "  Failed to read registers (no slave connected in this example)\n";
  }

  // Example 2: Read input registers
  std::cout << "\nExample 2: Reading input registers from slave 1...\n";
  auto input_regs = master.ReadInputRegisters(1, 0, 5);
  if (input_regs.has_value()) {
    std::cout << "  Read " << input_regs->size() << " input registers\n";
  }

  // Example 3: Write a single register
  std::cout << "\nExample 3: Writing single register to slave 1...\n";
  bool success = master.WriteSingleRegister(1, 0, 0x1234);
  if (success) {
    std::cout << "  Successfully wrote register 0 = 0x1234\n";
  } else {
    std::cout << "  Failed to write register\n";
  }

  // Example 4: Write multiple registers
  std::cout << "\nExample 4: Writing multiple registers to slave 1...\n";
  std::vector<int16_t> values{100, 200, 300, 400};
  success = master.WriteMultipleRegisters(1, 0, values);
  if (success) {
    std::cout << "  Successfully wrote " << values.size() << " registers\n";
  }

  // Example 5: Read coils
  std::cout << "\nExample 5: Reading coils from slave 1...\n";
  auto coils = master.ReadCoils(1, 0, 8);
  if (coils.has_value()) {
    std::cout << "  Read " << coils->size() << " coils:\n";
    for (size_t i = 0; i < coils->size(); ++i) {
      std::cout << "    Coil[" << i << "] = " << ((*coils)[i] ? "ON" : "OFF") << "\n";
    }
  }

  // Example 6: Write single coil
  std::cout << "\nExample 6: Writing single coil to slave 1...\n";
  success = master.WriteSingleCoil(1, 0, true);
  if (success) {
    std::cout << "  Successfully wrote coil 0 = ON\n";
  }

  // Example 7: Write multiple coils
  std::cout << "\nExample 7: Writing multiple coils to slave 1...\n";
  std::vector<bool> coil_values{true, false, true, false, true};
  // Convert vector<bool> to regular bool array for span
  bool bool_array[5] = {coil_values[0], coil_values[1], coil_values[2], coil_values[3], coil_values[4]};
  success = master.WriteMultipleCoils(1, 0, std::span<bool const>(bool_array));
  if (success) {
    std::cout << "  Successfully wrote " << coil_values.size() << " coils\n";
  }

  // Example 8: Broadcast write (slave ID 0)
  std::cout << "\nExample 8: Broadcast write to all slaves (slave ID 0)...\n";
  success = master.WriteSingleRegister(0, 0, 0xABCD);  // Broadcast
  if (success) {
    std::cout << "  Broadcast write successful (no response expected)\n";
  }

  // Example 9: Read exception status
  std::cout << "\nExample 9: Reading exception status from slave 1...\n";
  auto exception_status = master.ReadExceptionStatus(1);
  if (exception_status.has_value()) {
    std::cout << "  Exception status: 0x" << std::hex << static_cast<int>(*exception_status) << std::dec << "\n";
  }

  // Example 10: Read FIFO queue
  std::cout << "\nExample 10: Reading FIFO queue from slave 1...\n";
  auto fifo_data = master.ReadFIFOQueue(1, 0);
  if (fifo_data.has_value()) {
    std::cout << "  Read " << fifo_data->size() << " values from FIFO queue\n";
  }

  // Example 11: Mask write register
  std::cout << "\nExample 11: Mask write register on slave 1...\n";
  success = master.MaskWriteRegister(1, 0, 0xFF00, 0x0056);
  if (success) {
    std::cout << "  Mask write successful\n";
  }

  // Example 12: Read/Write multiple registers
  std::cout << "\nExample 12: Read/Write multiple registers on slave 1...\n";
  std::vector<int16_t> write_vals{500, 600};
  auto read_vals = master.ReadWriteMultipleRegisters(1, 10, 2, 0, write_vals);
  if (read_vals.has_value()) {
    std::cout << "  Read " << read_vals->size() << " registers while writing\n";
  }

  std::cout << "\nMaster examples completed!\n";
  return 0;
}
