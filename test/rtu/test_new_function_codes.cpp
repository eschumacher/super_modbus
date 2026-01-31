#include <array>
#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_frame.hpp"
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

TEST(NewFunctionCodes, ReadCoils) {
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{1};

  // Setup slave
  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 10});

  // Set some coil values
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleCoil}};
  write_req.SetWriteSingleCoilData(0, true);
  slave.Process(write_req);
  write_req.SetWriteSingleCoilData(1, false);
  slave.Process(write_req);
  write_req.SetWriteSingleCoilData(2, true);
  slave.Process(write_req);

  // Read coils
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadCoils}};
  read_req.SetAddressSpan({0, 3});
  RtuResponse response = slave.Process(read_req);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_GE(data.size(), 2);  // byte_count + at least 1 byte of data
  EXPECT_EQ(data[0], 1);  // byte_count for 3 coils = 1 byte

  // Unpack coils
  EXPECT_TRUE((data[1] & 0x01) != 0);   // Coil 0
  EXPECT_FALSE((data[1] & 0x02) != 0);  // Coil 1
  EXPECT_TRUE((data[1] & 0x04) != 0);   // Coil 2
}

TEST(NewFunctionCodes, WriteSingleCoil) {
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{2};

  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 10});

  // Write a coil
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleCoil}};
  write_req.SetWriteSingleCoilData(0, true);
  RtuResponse write_response = slave.Process(write_req);
  EXPECT_EQ(write_response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Verify by reading it back
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadCoils}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_response = slave.Process(read_req);
  EXPECT_EQ(read_response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  auto data = read_response.GetData();
  ASSERT_GE(data.size(), 2);
  EXPECT_TRUE((data[1] & 0x01) != 0);  // Coil 0 should be ON
}

TEST(NewFunctionCodes, WriteMultipleRegisters) {
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{3};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Write multiple registers
  std::array<int16_t, 4> values{100, 200, 300, 400};
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteMultRegs}};
  write_req.SetWriteMultipleRegistersData(0, 4, values);
  RtuResponse write_response = slave.Process(write_req);
  EXPECT_EQ(write_response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Verify by reading them back
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 4});
  RtuResponse read_response = slave.Process(read_req);
  EXPECT_EQ(read_response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  auto data = read_response.GetData();
  // Read register responses include byte_count (1 byte) + register data (4 registers * 2 bytes = 8 bytes)
  ASSERT_EQ(data.size(), 9);  // 1 byte_count + 4 registers * 2 bytes
  // Skip byte_count byte, then read register values (low byte first, then high byte)
  using supermb::MakeInt16;
  EXPECT_EQ(MakeInt16(data[1], data[2]), 100);
  EXPECT_EQ(MakeInt16(data[3], data[4]), 200);
  EXPECT_EQ(MakeInt16(data[5], data[6]), 300);
  EXPECT_EQ(MakeInt16(data[7], data[8]), 400);
}

TEST(NewFunctionCodes, WriteMultipleCoils) {
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{4};

  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 10});

  // Write multiple coils
  std::array<bool, 5> values_array{true, false, true, true, false};
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteMultCoils}};
  write_req.SetWriteMultipleCoilsData(0, 5, values_array);
  RtuResponse write_response = slave.Process(write_req);
  EXPECT_EQ(write_response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Verify by reading them back
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadCoils}};
  read_req.SetAddressSpan({0, 5});
  RtuResponse read_response = slave.Process(read_req);
  EXPECT_EQ(read_response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  auto data = read_response.GetData();
  ASSERT_GE(data.size(), 2);
  EXPECT_TRUE((data[1] & 0x01) != 0);   // Coil 0
  EXPECT_FALSE((data[1] & 0x02) != 0);   // Coil 1
  EXPECT_TRUE((data[1] & 0x04) != 0);   // Coil 2
  EXPECT_TRUE((data[1] & 0x08) != 0);    // Coil 3
  EXPECT_FALSE((data[1] & 0x10) != 0);   // Coil 4
}

TEST(NewFunctionCodes, ReadDiscreteInputs) {
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{5};

  RtuSlave slave{kSlaveId};
  slave.AddDiscreteInputs({0, 10});

  // Read discrete inputs
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadDI}};
  read_req.SetAddressSpan({0, 5});
  RtuResponse response = slave.Process(read_req);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_GE(data.size(), 2);  // byte_count + at least 1 byte of data
  EXPECT_EQ(data[0], 1);  // byte_count for 5 discrete inputs = 1 byte
}
