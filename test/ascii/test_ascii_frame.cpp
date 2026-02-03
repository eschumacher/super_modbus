#include "super_modbus/ascii/ascii_frame.hpp"
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include <gtest/gtest.h>
#include <string>

using supermb::AsciiFrame;
using supermb::ByteOrder;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::RtuRequest;
using supermb::RtuResponse;

TEST(AsciiFrame, EncodeDecodeRequest) {
  RtuRequest request({1, FunctionCode::kReadHR}, ByteOrder::BigEndian);
  request.SetAddressSpan({0, 10});

  std::string encoded = AsciiFrame::EncodeRequest(request);
  ASSERT_FALSE(encoded.empty());
  EXPECT_EQ(encoded[0], ':');
  EXPECT_NE(encoded.find("\r\n"), std::string::npos);

  auto decoded = AsciiFrame::DecodeRequest(encoded, ByteOrder::BigEndian);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), 1);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadHR);
}

TEST(AsciiFrame, EncodeDecodeResponse) {
  RtuResponse response(1, FunctionCode::kReadHR);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(20);  // byte count
  response.EmplaceBack(0);
  response.EmplaceBack(1);
  response.EmplaceBack(0);
  response.EmplaceBack(2);

  std::string encoded = AsciiFrame::EncodeResponse(response);
  ASSERT_FALSE(encoded.empty());
  EXPECT_EQ(encoded[0], ':');

  auto decoded = AsciiFrame::DecodeResponse(encoded);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), 1);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadHR);
}

TEST(AsciiFrame, BytesToHexHexToBytes) {
  uint8_t bytes[] = {0x01, 0x03, 0x00, 0x0A};
  auto hex = AsciiFrame::BytesToHex(bytes);
  EXPECT_EQ(hex, "0103000A");

  auto back = AsciiFrame::HexToBytes(hex);
  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(back->size(), 4u);
  EXPECT_EQ((*back)[0], 0x01);
  EXPECT_EQ((*back)[1], 0x03);
  EXPECT_EQ((*back)[2], 0x00);
  EXPECT_EQ((*back)[3], 0x0A);
}

TEST(AsciiFrame, DecodeRequestLittleEndian) {
  RtuRequest request({1, FunctionCode::kReadHR}, ByteOrder::LittleEndian);
  request.SetAddressSpan({10, 5});
  std::string encoded = AsciiFrame::EncodeRequest(request);
  auto decoded = AsciiFrame::DecodeRequest(encoded, ByteOrder::LittleEndian);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), 1);
  auto span = decoded->GetAddressSpan();
  ASSERT_TRUE(span.has_value());
  EXPECT_EQ(span->start_address, 10u);
  EXPECT_EQ(span->reg_count, 5u);
}
