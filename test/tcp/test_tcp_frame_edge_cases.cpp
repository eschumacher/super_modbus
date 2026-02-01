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

TEST(TCPFrameEdgeCases, DecodeRequestTooShort) {
  std::vector<uint8_t> frame{0x00, 0x01};  // Only 2 bytes, need at least MBAP header (7)
  auto decoded = TcpFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(TCPFrameEdgeCases, DecodeRequestInvalidProtocolId) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x01);  // Protocol ID low (invalid, should be 0x0000)
  frame.push_back(0x00);  // Length high
  frame.push_back(0x03);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x03);  // Function code
  frame.push_back(0x00);  // Data

  auto decoded = TcpFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(TCPFrameEdgeCases, DecodeRequestIncompleteFrame) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x05);  // Length low (says 5 bytes, but we only have 7 total)
  frame.push_back(0x01);  // Unit ID
  // Missing function code and data

  auto decoded = TcpFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());  // Frame not complete
}

TEST(TCPFrameEdgeCases, DecodeRequestMinimalFrame) {
  // Minimal valid frame: MBAP header + function code
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x02);  // Length low (Unit ID + Function Code)
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x07);  // Function code (Read Exception Status)

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadExceptionStatus);
}

TEST(TCPFrameEdgeCases, DecodeRequestReadWithInsufficientData) {
  // Read request with less than 4 bytes of data
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x04);  // Length low (Unit ID + Function Code + 2 bytes data)
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x03);  // Function code
  frame.push_back(0x00);  // Data byte 1
  frame.push_back(0x00);  // Data byte 2

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  // Should fall back to raw data
  EXPECT_EQ(decoded->GetData().size(), 2);
}

TEST(TCPFrameEdgeCases, DecodeRequestWriteSingleRegWithInsufficientData) {
  // Write single register with less than 4 bytes
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x03);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x06);  // Function code
  frame.push_back(0x00);  // Only 1 byte of data

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  // Should fall back to raw data
  EXPECT_EQ(decoded->GetData().size(), 1);
}

TEST(TCPFrameEdgeCases, DecodeRequestWriteSingleCoilWithInsufficientData) {
  // Write single coil with less than 4 bytes
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x03);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x05);  // Function code
  frame.push_back(0x00);  // Only 1 byte of data

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  // Should fall back to raw data
  EXPECT_EQ(decoded->GetData().size(), 1);
}

TEST(TCPFrameEdgeCases, DecodeRequestOtherFunctionCodeWithData) {
  // Function code that uses SetRawData
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x08);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x10);  // Function code (Write Multiple Registers)
  frame.push_back(0x00);  // Data
  frame.push_back(0x00);
  frame.push_back(0x00);
  frame.push_back(0x02);
  frame.push_back(0x04);
  frame.push_back(0x12);
  frame.push_back(0x34);

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteMultRegs);
  // Should have raw data
  EXPECT_GT(decoded->GetData().size(), 0);
}

TEST(TCPFrameEdgeCases, DecodeResponseTooShort) {
  std::vector<uint8_t> frame{0x00, 0x01};  // Only 2 bytes, need at least MBAP header (7)
  auto decoded = TcpFrame::DecodeResponse(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(TCPFrameEdgeCases, DecodeResponseInvalidProtocolId) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x01);  // Protocol ID low (invalid)
  frame.push_back(0x00);  // Length high
  frame.push_back(0x03);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x03);  // Function code
  frame.push_back(0x02);  // Byte count
  frame.push_back(0x12);  // Data
  frame.push_back(0x34);  // Data

  auto decoded = TcpFrame::DecodeResponse(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(TCPFrameEdgeCases, DecodeResponseIncompleteFrame) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x05);  // Length low (says 5 bytes)
  frame.push_back(0x01);  // Unit ID
  // Missing function code and data

  auto decoded = TcpFrame::DecodeResponse(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(TCPFrameEdgeCases, DecodeResponseExceptionWithInsufficientData) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x02);  // Length low (Unit ID + Function Code, but says exception)
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x83);  // Exception function code (0x03 | 0x80)
  // Missing exception code

  auto decoded = TcpFrame::DecodeResponse(frame);
  // Should still decode, but exception code might be invalid
  // Actually, length says 2, so we have Unit ID + Function Code, but exception needs 3
  // So it should fail the length check
  EXPECT_FALSE(decoded.has_value());
}

TEST(TCPFrameEdgeCases, DecodeResponseNormalWithNoData) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x02);  // Length low (Unit ID + Function Code only)
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x07);  // Function code (Read Exception Status)

  auto decoded = TcpFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetUnitId(), 0x01);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadExceptionStatus);
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_EQ(decoded->GetData().size(), 0);
}

TEST(TCPFrameEdgeCases, IsFrameComplete) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x03);  // Length low
  frame.push_back(0x01);  // Unit ID
  frame.push_back(0x03);  // Function code
  frame.push_back(0x00);  // Data

  EXPECT_TRUE(TcpFrame::IsFrameComplete(frame));
}

TEST(TCPFrameEdgeCases, IsFrameCompleteIncomplete) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x05);  // Length low (says 5 bytes)
  frame.push_back(0x01);  // Unit ID
  // Missing function code and data

  EXPECT_FALSE(TcpFrame::IsFrameComplete(frame));
}

TEST(TCPFrameEdgeCases, ExtractMbapHeaderTooShort) {
  std::vector<uint8_t> frame{0x00, 0x01, 0x00, 0x00, 0x00, 0x01};  // Only 6 bytes
  auto mbap = TcpFrame::ExtractMbapHeader(frame);
  EXPECT_FALSE(mbap.has_value());
}

TEST(TCPFrameEdgeCases, EncodeRequestWithLargeData) {
  static constexpr uint16_t kTransactionId{100};
  static constexpr uint8_t kUnitId{1};

  TcpRequest request{{FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values;
  for (int i = 0; i < 100; ++i) {
    values.push_back(static_cast<int16_t>(i));
  }
  request.SetWriteMultipleRegistersData(0, 100, values);

  auto frame = TcpFrame::EncodeRequest(request, kTransactionId, kUnitId);
  ASSERT_GT(frame.size(), TcpFrame::kMbapHeaderSize);

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteMultRegs);
}

TEST(TCPFrameEdgeCases, EncodeResponseWithLargeData) {
  static constexpr uint8_t kUnitId{1};
  static constexpr uint16_t kTransactionId{200};

  TcpResponse response{kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(200);  // Byte count
  for (int i = 0; i < 100; ++i) {
    response.EmplaceBack(static_cast<uint8_t>(i & 0xFF));
    response.EmplaceBack(static_cast<uint8_t>((i >> 8) & 0xFF));
  }

  auto frame = TcpFrame::EncodeResponse(response, kTransactionId);
  ASSERT_GT(frame.size(), TcpFrame::kMbapHeaderSize);

  auto decoded = TcpFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
  EXPECT_EQ(decoded->GetData().size(), 201);  // Byte count + 100 registers * 2
}

TEST(TCPFrameEdgeCases, TransactionIdZero) {
  static constexpr uint16_t kTransactionId{0};
  static constexpr uint8_t kUnitId{1};

  TcpRequest request{{FunctionCode::kReadExceptionStatus}};
  auto frame = TcpFrame::EncodeRequest(request, kTransactionId, kUnitId);

  auto mbap = TcpFrame::ExtractMbapHeader(frame);
  ASSERT_TRUE(mbap.has_value());
  EXPECT_EQ(mbap->transaction_id, kTransactionId);
}

TEST(TCPFrameEdgeCases, TransactionIdMax) {
  static constexpr uint16_t kTransactionId{0xFFFF};
  static constexpr uint8_t kUnitId{1};

  TcpRequest request{{FunctionCode::kReadExceptionStatus}};
  auto frame = TcpFrame::EncodeRequest(request, kTransactionId, kUnitId);

  auto mbap = TcpFrame::ExtractMbapHeader(frame);
  ASSERT_TRUE(mbap.has_value());
  EXPECT_EQ(mbap->transaction_id, kTransactionId);
}

TEST(TCPFrameEdgeCases, UnitIdZero) {
  static constexpr uint16_t kTransactionId{100};
  static constexpr uint8_t kUnitId{0};

  TcpRequest request{{FunctionCode::kReadExceptionStatus}};
  auto frame = TcpFrame::EncodeRequest(request, kTransactionId, kUnitId);

  auto mbap = TcpFrame::ExtractMbapHeader(frame);
  ASSERT_TRUE(mbap.has_value());
  EXPECT_EQ(mbap->unit_id, kUnitId);
}

TEST(TCPFrameEdgeCases, UnitIdMax) {
  static constexpr uint16_t kTransactionId{100};
  static constexpr uint8_t kUnitId{0xFF};

  TcpRequest request{{FunctionCode::kReadExceptionStatus}};
  auto frame = TcpFrame::EncodeRequest(request, kTransactionId, kUnitId);

  auto mbap = TcpFrame::ExtractMbapHeader(frame);
  ASSERT_TRUE(mbap.has_value());
  EXPECT_EQ(mbap->unit_id, kUnitId);
}
