#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_frame.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

TEST(RTUSlave, ReadHoldingRegisters) {
  using supermb::AddressSpan;
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{1};
  static constexpr AddressSpan kAddressSpan{0, 10};

  RtuSlave rtu_slave{kSlaveId};
  rtu_slave.AddHoldingRegisters(kAddressSpan);

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  EXPECT_TRUE(request.SetAddressSpan(kAddressSpan));

  RtuResponse response = rtu_slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read register responses include byte_count (1 byte) + register data (reg_count * 2 bytes)
  EXPECT_EQ(response.GetData().size(), static_cast<uint32_t>(1 + kAddressSpan.reg_count * 2));
}

TEST(RTUSlave, ReadInputRegisters) {
  using supermb::AddressSpan;
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{1};
  static constexpr AddressSpan kAddressSpan{0, 10};

  RtuSlave rtu_slave{kSlaveId};
  rtu_slave.AddInputRegisters(kAddressSpan);

  RtuRequest request{{kSlaveId, FunctionCode::kReadIR}};
  EXPECT_TRUE(request.SetAddressSpan(kAddressSpan));

  RtuResponse response = rtu_slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  // Read register responses include byte_count (1 byte) + register data (reg_count * 2 bytes)
  EXPECT_EQ(response.GetData().size(), static_cast<uint32_t>(1 + kAddressSpan.reg_count * 2));
}

TEST(RTUSlave, WriteHoldingRegisters) {
  using supermb::AddressSpan;
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::MakeInt16;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{1};
  static constexpr AddressSpan kAddressSpan{0, 1};
  static constexpr int16_t kRegisterValue{5};

  RtuSlave rtu_slave{kSlaveId};
  rtu_slave.AddHoldingRegisters(kAddressSpan);

  RtuRequest write_request{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_request.SetWriteSingleRegisterData(kAddressSpan.start_address, kRegisterValue);
  RtuResponse write_response = rtu_slave.Process(write_request);

  EXPECT_EQ(write_response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  RtuRequest read_request{{kSlaveId, FunctionCode::kReadHR}};
  read_request.SetAddressSpan(kAddressSpan);
  RtuResponse read_response = rtu_slave.Process(read_request);

  EXPECT_EQ(read_response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  // Read register responses include byte_count (1 byte) + register data (reg_count * 2 bytes)
  EXPECT_EQ(read_response.GetData().size(), static_cast<uint32_t>(1 + kAddressSpan.reg_count * 2));

  // Skip byte_count byte, then read register value (low byte first, then high byte)
  auto data = read_response.GetData();
  int16_t reg_value = MakeInt16(data[1], data[2]);
  EXPECT_EQ(reg_value, kRegisterValue);
}

TEST(RTUSlave, ProcessIncomingFrame) {
  using supermb::AddressSpan;
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::MemoryTransport;
  using supermb::RtuFrame;
  using supermb::RtuRequest;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{1};
  static constexpr AddressSpan kAddressSpan{0, 5};

  // Setup slave
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters(kAddressSpan);

  // Create a request frame
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 3});
  auto request_frame = RtuFrame::EncodeRequest(request);

  // Setup transport with the request frame
  MemoryTransport transport;
  transport.SetReadData(request_frame);

  // Process the incoming frame
  bool processed = slave.ProcessIncomingFrame(transport, 1000);
  EXPECT_TRUE(processed);

  // Check that response was written
  auto written_data = transport.GetWrittenData();
  EXPECT_GT(written_data.size(), 0);

  // Decode and verify response
  auto response = RtuFrame::DecodeResponse(written_data);
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->GetSlaveId(), kSlaveId);
  EXPECT_EQ(response->GetFunctionCode(), FunctionCode::kReadHR);
  EXPECT_EQ(response->GetExceptionCode(), ExceptionCode::kAcknowledge);
  // Read register responses include byte_count (1 byte) + register data (3 registers * 2 bytes = 6 bytes)
  EXPECT_EQ(response->GetData().size(), 7);  // 1 byte_count + 3 registers * 2 bytes
}

TEST(RTUSlave, Poll) {
  using supermb::AddressSpan;
  using supermb::FunctionCode;
  using supermb::MemoryTransport;
  using supermb::RtuFrame;
  using supermb::RtuRequest;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{2};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  MemoryTransport transport;

  // Poll with no data should return false
  bool processed = slave.Poll(transport);
  EXPECT_FALSE(processed);

  // Add a request frame
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 2});
  auto request_frame = RtuFrame::EncodeRequest(request);
  transport.SetReadData(request_frame);

  // Poll should now process the frame
  processed = slave.Poll(transport);
  EXPECT_TRUE(processed);
}
