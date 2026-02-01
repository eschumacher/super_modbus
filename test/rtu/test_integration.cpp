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

using supermb::AddressSpan;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::MakeInt16;
using supermb::MemoryTransport;
using supermb::RtuFrame;
using supermb::RtuMaster;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

TEST(Integration, MasterSlaveReadHoldingRegisters) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  MemoryTransport transport;

  // Master creates request
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto request_frame = RtuFrame::EncodeRequest(request);

  // Simulate sending to slave
  transport.SetReadData(request_frame);
  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_TRUE(processed);

  // Get response and verify
  auto response_frame = transport.GetWrittenData();
  auto decoded_response = RtuFrame::DecodeResponse(response_frame);
  ASSERT_TRUE(decoded_response.has_value());
  EXPECT_EQ(decoded_response->GetExceptionCode(), ExceptionCode::kAcknowledge);
  // Read register responses include byte_count (1 byte) + register data (5 registers * 2 bytes = 10 bytes)
  EXPECT_EQ(decoded_response->GetData().size(), 11);  // 1 byte_count + 5 registers * 2 bytes
}

TEST(Integration, MasterSlaveWriteThenRead) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Direct write using slave
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_req.SetWriteSingleRegisterData(0, 0x1234);
  RtuResponse write_resp = slave.Process(write_req);
  EXPECT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Now read it back
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = read_resp.GetData();
  // Read register responses include byte_count (1 byte) + register data (1 register * 2 bytes = 2 bytes)
  ASSERT_EQ(data.size(), 3);
  // Skip byte_count byte, then read register value
  // ProcessReadRegisters writes: high byte first, then low byte (big-endian)
  // MakeInt16 takes: (low_byte, high_byte), so we need data[2] (low) then data[1] (high)
  int16_t value = MakeInt16(data[2], data[1]);
  EXPECT_EQ(value, 0x1234);
}

TEST(Integration, MultipleWriteOperations) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Write to address 0 (this works in existing tests)
  RtuRequest write1{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write1.SetWriteSingleRegisterData(0, 100);
  RtuResponse resp1 = slave.Process(write1);
  EXPECT_EQ(resp1.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Verify it was written
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = read_resp.GetData();
  // Read register responses include byte_count (1 byte) + register data (1 register * 2 bytes = 2 bytes)
  ASSERT_EQ(data.size(), 3);
  // Skip byte_count byte, then read register value
  // ProcessReadRegisters writes: high byte first, then low byte (big-endian)
  // MakeInt16 takes: (low_byte, high_byte), so we need data[2] (low) then data[1] (high)
  EXPECT_EQ(MakeInt16(data[2], data[1]), 100);
}

TEST(Integration, CoilAndRegisterMixed) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 10});
  slave.AddHoldingRegisters({0, 10});

  // Write coil
  RtuRequest coil_write{{kSlaveId, FunctionCode::kWriteSingleCoil}};
  coil_write.SetWriteSingleCoilData(0, true);
  RtuResponse coil_resp = slave.Process(coil_write);
  EXPECT_EQ(coil_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Write register
  RtuRequest reg_write{{kSlaveId, FunctionCode::kWriteSingleReg}};
  reg_write.SetWriteSingleRegisterData(0, 0x5678);
  RtuResponse reg_resp = slave.Process(reg_write);
  EXPECT_EQ(reg_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read coil
  RtuRequest coil_read{{kSlaveId, FunctionCode::kReadCoils}};
  coil_read.SetAddressSpan({0, 1});
  RtuResponse coil_read_resp = slave.Process(coil_read);
  EXPECT_EQ(coil_read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read register
  RtuRequest reg_read{{kSlaveId, FunctionCode::kReadHR}};
  reg_read.SetAddressSpan({0, 1});
  RtuResponse reg_read_resp = slave.Process(reg_read);
  EXPECT_EQ(reg_read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto reg_data = reg_read_resp.GetData();
  // Skip byte_count byte, then read register value
  // ProcessReadRegisters writes: high byte first, then low byte (big-endian)
  // MakeInt16 takes: (low_byte, high_byte), so we need reg_data[2] (low) then reg_data[1] (high)
  int16_t reg_value = MakeInt16(reg_data[2], reg_data[1]);
  EXPECT_EQ(reg_value, 0x5678);
}

TEST(Integration, ExceptionResponseHandling) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Request invalid address
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({100, 5});
  RtuResponse response = slave.Process(request);

  // Should return exception
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);

  // Encode and decode exception response
  auto frame = RtuFrame::EncodeResponse(response);
  auto decoded = RtuFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(Integration, MultipleSlaves) {
  RtuSlave slave1{1};
  RtuSlave slave2{2};

  slave1.AddHoldingRegisters({0, 10});
  slave2.AddHoldingRegisters({0, 10});

  // Write to slave 1
  RtuRequest req1{{1, FunctionCode::kWriteSingleReg}};
  req1.SetWriteSingleRegisterData(0, 0x1111);
  RtuResponse resp1 = slave1.Process(req1);
  EXPECT_EQ(resp1.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Write to slave 2
  RtuRequest req2{{2, FunctionCode::kWriteSingleReg}};
  req2.SetWriteSingleRegisterData(0, 0x2222);
  RtuResponse resp2 = slave2.Process(req2);
  EXPECT_EQ(resp2.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read from slave 1
  RtuRequest read1{{1, FunctionCode::kReadHR}};
  read1.SetAddressSpan({0, 1});
  RtuResponse read_resp1 = slave1.Process(read1);
  auto data1 = read_resp1.GetData();
  // Skip byte_count byte, then read register value
  // ProcessReadRegisters writes: high byte first, then low byte (big-endian)
  // MakeInt16 takes: (low_byte, high_byte), so we need data1[2] (low) then data1[1] (high)
  int16_t value1 = MakeInt16(data1[2], data1[1]);
  EXPECT_EQ(value1, 0x1111);

  // Read from slave 2
  RtuRequest read2{{2, FunctionCode::kReadHR}};
  read2.SetAddressSpan({0, 1});
  RtuResponse read_resp2 = slave2.Process(read2);
  auto data2 = read_resp2.GetData();
  // Skip byte_count byte, then read register value
  // ProcessReadRegisters writes: high byte first, then low byte (big-endian)
  // MakeInt16 takes: (low_byte, high_byte), so we need data2[2] (low) then data2[1] (high)
  int16_t value2 = MakeInt16(data2[2], data2[1]);
  EXPECT_EQ(value2, 0x2222);
}
