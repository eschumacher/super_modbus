#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/tcp/tcp_frame.hpp"
#include "super_modbus/tcp/tcp_request.hpp"
#include "super_modbus/tcp/tcp_response.hpp"

using supermb::AddressSpan;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::GetHighByte;
using supermb::GetLowByte;
using supermb::TcpFrame;
using supermb::TcpRequest;
using supermb::TcpResponse;

TEST(TCPFrame, EncodeDecodeRequest) {
  static constexpr uint16_t kTransactionId{1234};
  static constexpr uint8_t kUnitId{5};
  static constexpr AddressSpan kAddressSpan{100, 10};

  TcpRequest request{{kTransactionId, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan(kAddressSpan);

  auto frame = TcpFrame::EncodeRequest(request);
  ASSERT_GE(frame.size(), 8);  // At least MBAP header (7) + function_code (1)

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadHR);

  auto span = decoded->GetAddressSpan();
  ASSERT_TRUE(span.has_value());
  EXPECT_EQ(span->start_address, kAddressSpan.start_address);
  EXPECT_EQ(span->reg_count, kAddressSpan.reg_count);
}

TEST(TCPFrame, EncodeDecodeResponse) {
  static constexpr uint16_t kTransactionId{5678};
  static constexpr uint8_t kUnitId{3};

  TcpResponse response{kTransactionId, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x0A);  // Byte count
  response.EmplaceBack(0x12);  // High byte of first register
  response.EmplaceBack(0x34);  // Low byte of first register

  auto frame = TcpFrame::EncodeResponse(response);
  ASSERT_GE(frame.size(), 8);

  auto decoded = TcpFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadHR);
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kAcknowledge);

  auto data = decoded->GetData();
  ASSERT_EQ(data.size(), 3);
  EXPECT_EQ(data[0], 0x0A);
  EXPECT_EQ(data[1], 0x12);
  EXPECT_EQ(data[2], 0x34);
}

TEST(TCPFrame, ExceptionResponse) {
  static constexpr uint16_t kTransactionId{9999};
  static constexpr uint8_t kUnitId{7};

  TcpResponse response{kTransactionId, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  ASSERT_GE(frame.size(), 8);

  auto decoded = TcpFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(TCPFrame, ProtocolIdValidation) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low (correct)
  frame.push_back(0x00);  // Length high
  frame.push_back(0x02);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x07);  // Function code

  auto decoded = TcpFrame::DecodeRequest(frame);
  EXPECT_TRUE(decoded.has_value());

  // Invalid protocol ID
  frame[3] = 0x01;
  decoded = TcpFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(TCPFrame, LengthFieldValidation) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x02);  // Length low (Unit ID + Function Code)
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x07);  // Function code

  auto decoded = TcpFrame::DecodeRequest(frame);
  EXPECT_TRUE(decoded.has_value());

  // Frame too short for declared length
  frame.pop_back();  // Remove function code
  frame[5] = 0x03;   // Length says 3 bytes
  decoded = TcpFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(TCPFrame, GetMinFrameSize) {
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReadHR), 12);  // MBAP(7) + FC(1) + addr(2) + count(2)
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kWriteSingleReg), 12);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReadExceptionStatus), 8);  // MBAP(7) + FC(1)
}

TEST(TCPFrame, IsRequestFrameComplete) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x02);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x07);  // Function code

  EXPECT_TRUE(TcpFrame::IsRequestFrameComplete(frame));

  // Incomplete frame
  frame.pop_back();
  EXPECT_FALSE(TcpFrame::IsRequestFrameComplete(frame));
}

TEST(TCPFrame, IsResponseFrameComplete) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x02);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x07);  // Function code

  EXPECT_TRUE(TcpFrame::IsResponseFrameComplete(frame));

  // Incomplete frame
  frame.pop_back();
  EXPECT_FALSE(TcpFrame::IsResponseFrameComplete(frame));
}

TEST(TCPFrame, WriteSingleRegisterRoundTrip) {
  static constexpr uint16_t kTransactionId{100};
  static constexpr uint8_t kUnitId{1};
  static constexpr uint16_t kAddress{50};
  static constexpr int16_t kValue{1234};

  TcpRequest request{{kTransactionId, kUnitId, FunctionCode::kWriteSingleReg}};
  request.SetWriteSingleRegisterData(kAddress, kValue);

  auto frame = TcpFrame::EncodeRequest(request);
  auto decoded = TcpFrame::DecodeRequest(frame);

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteSingleReg);

  auto data = decoded->GetData();
  ASSERT_EQ(data.size(), 4);
  uint16_t address = supermb::MakeInt16(data[1], data[0]);
  int16_t value = supermb::MakeInt16(data[3], data[2]);
  EXPECT_EQ(address, kAddress);
  EXPECT_EQ(value, kValue);
}

TEST(TCPFrame, WriteSingleCoilRoundTrip) {
  static constexpr uint16_t kTransactionId{200};
  static constexpr uint8_t kUnitId{2};
  static constexpr uint16_t kAddress{10};
  static constexpr bool kValue{true};

  TcpRequest request{{kTransactionId, kUnitId, FunctionCode::kWriteSingleCoil}};
  request.SetWriteSingleCoilData(kAddress, kValue);

  auto frame = TcpFrame::EncodeRequest(request);
  auto decoded = TcpFrame::DecodeRequest(frame);

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteSingleCoil);

  auto data = decoded->GetData();
  ASSERT_EQ(data.size(), 4);
  uint16_t address = supermb::MakeInt16(data[1], data[0]);
  uint16_t coil_value = supermb::MakeInt16(data[3], data[2]);
  EXPECT_EQ(address, kAddress);
  EXPECT_EQ(coil_value, supermb::kCoilOnValue);
}
