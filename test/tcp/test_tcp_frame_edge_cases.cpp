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

TEST(TCPFrameEdgeCases, IsRequestFrameCompleteEmptyFrame) {
  std::vector<uint8_t> frame;
  EXPECT_FALSE(TcpFrame::IsRequestFrameComplete(frame));
}

TEST(TCPFrameEdgeCases, IsRequestFrameCompleteOnlyHeader) {
  std::vector<uint8_t> frame(7, 0);  // 7 bytes - MBAP header only, length=0 so need 6 bytes total
  frame[4] = 0x00;
  frame[5] = 0x00;                                       // length = 0
  EXPECT_TRUE(TcpFrame::IsRequestFrameComplete(frame));  // 7 >= 6+0
}

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
  // length < 3 for exception response -> invalid
  EXPECT_FALSE(decoded.has_value());
}

// Exception response with length=3 but frame too short (frame.size() < pdu_start + 2)
TEST(TCPFrameEdgeCases, DecodeResponseExceptionFrameTooShort) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);
  frame.push_back(0x01);
  frame.push_back(0x00);
  frame.push_back(0x00);
  frame.push_back(0x00);
  frame.push_back(0x03);  // Length = 3 (Unit ID + FC + Exception code)
  frame.push_back(0x01);  // Unit ID
  // Only 7 bytes - missing function code and exception code (need 9 total)
  // frame.size()=7 < 6+length=9, so we fail at frame.size() < 6+length
  auto decoded = TcpFrame::DecodeResponse(frame);
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

TEST(TCPFrameEdgeCases, IsRequestFrameComplete) {
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

  EXPECT_TRUE(TcpFrame::IsRequestFrameComplete(frame));
}

TEST(TCPFrameEdgeCases, IsRequestFrameCompleteIncomplete) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);  // Transaction ID high
  frame.push_back(0x01);  // Transaction ID low
  frame.push_back(0x00);  // Protocol ID high
  frame.push_back(0x00);  // Protocol ID low
  frame.push_back(0x00);  // Length high
  frame.push_back(0x05);  // Length low (says 5 bytes)
  frame.push_back(0x01);  // Unit ID
  // Missing function code and data

  EXPECT_FALSE(TcpFrame::IsRequestFrameComplete(frame));
}

TEST(TCPFrameEdgeCases, EncodeRequestWithLargeData) {
  static constexpr uint16_t kTransactionId{100};
  static constexpr uint8_t kUnitId{1};

  TcpRequest request{{kTransactionId, kUnitId, FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values;
  for (int i = 0; i < 100; ++i) {
    values.push_back(static_cast<int16_t>(i));
  }
  request.SetWriteMultipleRegistersData(0, 100, values);

  auto frame = TcpFrame::EncodeRequest(request);
  ASSERT_GT(frame.size(), 7);  // MBAP header size

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteMultRegs);
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
}

TEST(TCPFrameEdgeCases, EncodeResponseWithLargeData) {
  static constexpr uint8_t kUnitId{1};
  static constexpr uint16_t kTransactionId{200};

  TcpResponse response{kTransactionId, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(200);  // Byte count
  for (int i = 0; i < 100; ++i) {
    response.EmplaceBack(static_cast<uint8_t>(i & 0xFF));
    response.EmplaceBack(static_cast<uint8_t>((i >> 8) & 0xFF));
  }

  auto frame = TcpFrame::EncodeResponse(response);
  ASSERT_GT(frame.size(), 7);  // MBAP header size

  auto decoded = TcpFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
  EXPECT_EQ(decoded->GetData().size(), 201);  // Byte count + 100 registers * 2
}

TEST(TCPFrameEdgeCases, TransactionIdZero) {
  static constexpr uint16_t kTransactionId{0};
  static constexpr uint8_t kUnitId{1};

  TcpRequest request{{kTransactionId, kUnitId, FunctionCode::kReadExceptionStatus}};
  auto frame = TcpFrame::EncodeRequest(request);

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
}

TEST(TCPFrameEdgeCases, TransactionIdMax) {
  static constexpr uint16_t kTransactionId{0xFFFF};
  static constexpr uint8_t kUnitId{1};

  TcpRequest request{{kTransactionId, kUnitId, FunctionCode::kReadExceptionStatus}};
  auto frame = TcpFrame::EncodeRequest(request);

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetTransactionId(), kTransactionId);
}

TEST(TCPFrameEdgeCases, UnitIdZero) {
  static constexpr uint16_t kTransactionId{100};
  static constexpr uint8_t kUnitId{0};

  TcpRequest request{{kTransactionId, kUnitId, FunctionCode::kReadExceptionStatus}};
  auto frame = TcpFrame::EncodeRequest(request);

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
}

TEST(TCPFrameEdgeCases, UnitIdMax) {
  static constexpr uint16_t kTransactionId{100};
  static constexpr uint8_t kUnitId{0xFF};

  TcpRequest request{{kTransactionId, kUnitId, FunctionCode::kReadExceptionStatus}};
  auto frame = TcpFrame::EncodeRequest(request);

  auto decoded = TcpFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
}

TEST(TCPFrameEdgeCases, DecodeRequestLengthLessThanOne) {
  // Frame with length field = 0 (invalid - must have at least Unit ID)
  // Need 8 bytes to pass min frame size check, then length=0 fails
  std::vector<uint8_t> frame;
  frame.push_back(0x00);
  frame.push_back(0x01);  // Transaction ID
  frame.push_back(0x00);
  frame.push_back(0x00);  // Protocol ID
  frame.push_back(0x00);
  frame.push_back(0x00);  // Length = 0 (invalid)
  frame.push_back(0x01);  // Unit ID (padding)
  frame.push_back(0x07);  // Function code (padding)

  auto decoded = TcpFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());  // length < 1 is invalid
}

TEST(TCPFrameEdgeCases, EncodeResponseExceptionPath) {
  static constexpr uint16_t kTransactionId{100};
  static constexpr uint8_t kUnitId{1};

  TcpResponse response{kTransactionId, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  ASSERT_GE(frame.size(), 9);  // MBAP(7) + exception FC(1) + exception code(1)
  EXPECT_EQ(frame[7], 0x83);   // Function code with exception mask (0x03 | 0x80)
  EXPECT_EQ(frame[8], static_cast<uint8_t>(ExceptionCode::kIllegalDataAddress));

  auto decoded = TcpFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(TCPFrameEdgeCases, GetMinFrameSizeAllFunctionCodes) {
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReadCoils), 12);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReadDI), 12);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReadIR), 12);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kDiagnostics), 8);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kGetComEventCounter), 8);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReportSlaveID), 8);
}

// Default case: function codes not in switch return kMinFrameSize (8)
TEST(TCPFrameEdgeCases, GetMinFrameSizeDefaultCase) {
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReadFileRecord), 8);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kWriteFileRecord), 8);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReadFIFOQueue), 8);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kReadWriteMultRegs), 8);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kWriteMultRegs), 8);
  EXPECT_EQ(TcpFrame::GetMinFrameSize(FunctionCode::kWriteMultCoils), 8);
}

TEST(TCPFrameEdgeCases, IsResponseFrameComplete) {
  std::vector<uint8_t> frame;
  frame.push_back(0x00);
  frame.push_back(0x01);
  frame.push_back(0x00);
  frame.push_back(0x00);
  frame.push_back(0x00);
  frame.push_back(0x02);
  frame.push_back(0x01);
  frame.push_back(0x07);

  EXPECT_TRUE(TcpFrame::IsResponseFrameComplete(frame));
  frame.pop_back();
  EXPECT_FALSE(TcpFrame::IsResponseFrameComplete(frame));
}
