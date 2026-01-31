#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/crc16.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_frame.hpp"
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

TEST(RTUMaster, RequestEncoding) {
  using supermb::AddressSpan;
  using supermb::FunctionCode;
  using supermb::MemoryTransport;
  using supermb::RtuFrame;
  using supermb::RtuMaster;
  using supermb::RtuRequest;

  static constexpr uint8_t kSlaveId{1};

  MemoryTransport transport;
  RtuMaster master{transport};

  // Create a request
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  // Send request (this will encode and write to transport)
  auto response = master.SendRequest(request, 0);  // 0 timeout for test
  // Response will be empty since no data in transport, but encoding should work

  // Verify the frame was written correctly
  auto written_data = transport.GetWrittenData();
  ASSERT_GT(written_data.size(), 0);

  // Decode and verify
  auto decoded = RtuFrame::DecodeRequest(written_data);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), kSlaveId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadHR);
}

TEST(RTUFrame, EncodeDecodeRequest) {
  using supermb::AddressSpan;
  using supermb::FunctionCode;
  using supermb::RtuFrame;
  using supermb::RtuRequest;

  static constexpr uint8_t kSlaveId{5};
  static constexpr AddressSpan kAddressSpan{100, 10};

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan(kAddressSpan);

  auto frame = RtuFrame::EncodeRequest(request);
  ASSERT_GE(frame.size(), 4);  // At least slave_id + function_code + CRC

  auto decoded = RtuFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), kSlaveId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadHR);

  auto span = decoded->GetAddressSpan();
  ASSERT_TRUE(span.has_value());
  EXPECT_EQ(span->start_address, kAddressSpan.start_address);
  EXPECT_EQ(span->reg_count, kAddressSpan.reg_count);
}

TEST(RTUFrame, EncodeDecodeResponse) {
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuFrame;
  using supermb::RtuResponse;

  static constexpr uint8_t kSlaveId{3};

  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x12);  // Low byte of first register
  response.EmplaceBack(0x34);  // High byte of first register

  auto frame = RtuFrame::EncodeResponse(response);
  ASSERT_GE(frame.size(), 4);

  auto decoded = RtuFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), kSlaveId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadHR);
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kAcknowledge);

  auto data = decoded->GetData();
  ASSERT_EQ(data.size(), 2);
  EXPECT_EQ(data[0], 0x12);
  EXPECT_EQ(data[1], 0x34);
}

TEST(RTUFrame, ExceptionResponse) {
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuFrame;
  using supermb::RtuResponse;

  static constexpr uint8_t kSlaveId{7};

  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  ASSERT_GE(frame.size(), 4);

  auto decoded = RtuFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), kSlaveId);
  // Exception response should have function code with MSB set
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(CRC16, CalculateAndVerify) {
  using supermb::CalculateCrc16;
  using supermb::VerifyCrc16;

  // Test vector: simple frame
  std::vector<uint8_t> frame{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  uint16_t crc = CalculateCrc16(frame);

  // Append CRC (little-endian)
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

  // Verify CRC
  EXPECT_TRUE(VerifyCrc16(frame));

  // Corrupt frame
  frame[0] = 0x02;
  EXPECT_FALSE(VerifyCrc16(frame));
}
