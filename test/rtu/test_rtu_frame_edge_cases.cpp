#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/crc16.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_frame.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"

using supermb::AddressSpan;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::GetHighByte;
using supermb::GetLowByte;
using supermb::RtuFrame;
using supermb::RtuRequest;
using supermb::RtuResponse;

TEST(RTUFrameEdgeCases, DecodeRequestTooShort) {
  std::vector<uint8_t> frame{0x01};  // Only slave ID
  auto decoded = RtuFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(RTUFrameEdgeCases, DecodeRequestInvalidCRC) {
  std::vector<uint8_t> frame{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00};  // Wrong CRC
  auto decoded = RtuFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(RTUFrameEdgeCases, DecodeRequestMinimalFrame) {
  // Minimal valid frame: slave_id + function_code + CRC
  std::vector<uint8_t> frame{0x01, 0x07};  // Read Exception Status
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), 0x01);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadExceptionStatus);
}

TEST(RTUFrameEdgeCases, DecodeRequestReadWithInsufficientData) {
  // Read request with less than 4 bytes of data
  std::vector<uint8_t> frame{0x01, 0x03, 0x00, 0x00};  // Only 2 bytes of data
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  // Should fall back to raw data
  EXPECT_EQ(decoded->GetData().size(), 2);
}

TEST(RTUFrameEdgeCases, DecodeRequestWriteSingleRegWithInsufficientData) {
  // Write single register with less than 4 bytes
  std::vector<uint8_t> frame{0x01, 0x06, 0x00};  // Only 1 byte of data
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  // Should fall back to raw data
  EXPECT_EQ(decoded->GetData().size(), 1);
}

TEST(RTUFrameEdgeCases, DecodeRequestWriteSingleCoilWithInsufficientData) {
  // Write single coil with less than 4 bytes
  std::vector<uint8_t> frame{0x01, 0x05, 0x00};  // Only 1 byte of data
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  // Should fall back to raw data
  EXPECT_EQ(decoded->GetData().size(), 1);
}

TEST(RTUFrameEdgeCases, DecodeRequestOtherFunctionCodeWithData) {
  // Function code that uses SetRawData (e.g., WriteMultipleRegisters)
  std::vector<uint8_t> frame{0x01, 0x10, 0x00, 0x00, 0x00, 0x02, 0x04, 0x12, 0x34, 0x56, 0x78};
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteMultRegs);
  // Should have raw data
  EXPECT_GT(decoded->GetData().size(), 0);
}

TEST(RTUFrameEdgeCases, DecodeResponseTooShort) {
  std::vector<uint8_t> frame{0x01};  // Only slave ID
  auto decoded = RtuFrame::DecodeResponse(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(RTUFrameEdgeCases, DecodeResponseInvalidCRC) {
  std::vector<uint8_t> frame{0x01, 0x03, 0x02, 0x12, 0x34, 0x00, 0x00};  // Wrong CRC
  auto decoded = RtuFrame::DecodeResponse(frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(RTUFrameEdgeCases, DecodeResponseMinimalFrame) {
  // Minimal valid response: slave_id + function_code + CRC
  std::vector<uint8_t> frame{0x01, 0x07};  // Read Exception Status response
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), 0x01);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kReadExceptionStatus);
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kAcknowledge);
}

TEST(RTUFrameEdgeCases, DecodeResponseExceptionWithMinimalSize) {
  // Exception response with minimal size (3 bytes + CRC = 5 bytes total)
  std::vector<uint8_t> frame{0x01, 0x83, 0x02};  // Exception: Illegal Data Address
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(RTUFrameEdgeCases, GetMinFrameSize) {
  EXPECT_EQ(RtuFrame::GetMinFrameSize(FunctionCode::kReadHR), 8);  // 4 base + 4 data
  EXPECT_EQ(RtuFrame::GetMinFrameSize(FunctionCode::kReadIR), 8);
  EXPECT_EQ(RtuFrame::GetMinFrameSize(FunctionCode::kReadCoils), 8);
  EXPECT_EQ(RtuFrame::GetMinFrameSize(FunctionCode::kReadDI), 8);
  EXPECT_EQ(RtuFrame::GetMinFrameSize(FunctionCode::kWriteSingleReg), 8);
  EXPECT_EQ(RtuFrame::GetMinFrameSize(FunctionCode::kWriteSingleCoil), 8);
  EXPECT_EQ(RtuFrame::GetMinFrameSize(FunctionCode::kReadExceptionStatus), 4);  // Base only
  EXPECT_EQ(RtuFrame::GetMinFrameSize(FunctionCode::kDiagnostics), 4);
}

TEST(RTUFrameEdgeCases, IsRequestFrameComplete) {
  // Too short
  std::vector<uint8_t> frame1{0x01};
  EXPECT_FALSE(RtuFrame::IsRequestFrameComplete(frame1));

  // Just slave_id and function_code
  std::vector<uint8_t> frame2{0x01, 0x03};
  EXPECT_FALSE(RtuFrame::IsRequestFrameComplete(frame2));

  // Complete read request (without CRC, but function checks minimum size)
  std::vector<uint8_t> frame3{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xCD, 0xC5};
  EXPECT_TRUE(RtuFrame::IsRequestFrameComplete(frame3));

  // Complete write single register
  std::vector<uint8_t> frame4{0x01, 0x06, 0x00, 0x01, 0x00, 0x17, 0x9B, 0x9A};
  EXPECT_TRUE(RtuFrame::IsRequestFrameComplete(frame4));
}

TEST(RTUFrameEdgeCases, IsResponseFrameComplete) {
  // Too short
  std::vector<uint8_t> frame1{0x01};
  EXPECT_FALSE(RtuFrame::IsResponseFrameComplete(frame1));

  // Exception response - too short
  std::vector<uint8_t> frame2{0x01, 0x83};
  EXPECT_FALSE(RtuFrame::IsResponseFrameComplete(frame2));

  // Complete exception response
  std::vector<uint8_t> frame3{0x01, 0x83, 0x02, 0xC1, 0xF0};
  EXPECT_TRUE(RtuFrame::IsResponseFrameComplete(frame3));

  // Read response - need byte_count
  std::vector<uint8_t> frame4{0x01, 0x03, 0x02};
  EXPECT_FALSE(RtuFrame::IsResponseFrameComplete(frame4));

  // Complete read response
  std::vector<uint8_t> frame5{0x01, 0x03, 0x02, 0x12, 0x34, 0xCD, 0xC5};
  EXPECT_TRUE(RtuFrame::IsResponseFrameComplete(frame5));

  // Write single register response
  std::vector<uint8_t> frame6{0x01, 0x06, 0x00, 0x01, 0x00, 0x17, 0x9B, 0x9A};
  EXPECT_TRUE(RtuFrame::IsResponseFrameComplete(frame6));
}

TEST(RTUFrameEdgeCases, IsRequestFrameComplete_IsResponseFrameComplete) {
  // Too short
  std::vector<uint8_t> frame1{0x01};
  EXPECT_FALSE(RtuFrame::IsRequestFrameComplete(frame1));
  EXPECT_FALSE(RtuFrame::IsResponseFrameComplete(frame1));

  // Exception response
  std::vector<uint8_t> frame2{0x01, 0x83, 0x02, 0xC1, 0xF0};
  EXPECT_TRUE(RtuFrame::IsResponseFrameComplete(frame2));

  // Read request (exact size match)
  std::vector<uint8_t> frame3{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xCD, 0xC5};
  EXPECT_TRUE(RtuFrame::IsRequestFrameComplete(frame3));

  // Read response (variable size)
  std::vector<uint8_t> frame4{0x01, 0x03, 0x02, 0x12, 0x34, 0xCD, 0xC5};
  EXPECT_TRUE(RtuFrame::IsResponseFrameComplete(frame4));
}

TEST(RTUFrameEdgeCases, DecodeRequestWithRawData) {
  // Request with function code that doesn't parse address span
  std::vector<uint8_t> frame{0x01, 0x08};  // Diagnostics
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeRequest(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kDiagnostics);
  EXPECT_TRUE(decoded->GetData().empty());
}

TEST(RTUFrameEdgeCases, DecodeResponseEmptyData) {
  // Response with no data (just acknowledge)
  std::vector<uint8_t> frame{0x01, 0x07};  // Read Exception Status
  uint16_t crc = supermb::CalculateCrc16(frame);
  frame.push_back(supermb::GetLowByte(crc));
  frame.push_back(supermb::GetHighByte(crc));

  auto decoded = RtuFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_TRUE(decoded->GetData().empty());
}

TEST(RTUFrameEdgeCases, EncodeDecodeRoundTripAllFunctionCodes) {
  // Test encoding and decoding for various function codes
  const FunctionCode codes[] = {FunctionCode::kReadHR,
                                FunctionCode::kReadIR,
                                FunctionCode::kReadCoils,
                                FunctionCode::kReadDI,
                                FunctionCode::kWriteSingleReg,
                                FunctionCode::kWriteSingleCoil,
                                FunctionCode::kReadExceptionStatus,
                                FunctionCode::kDiagnostics};

  for (FunctionCode code : codes) {
    RtuRequest request{{1, code}};
    if (code == FunctionCode::kReadHR || code == FunctionCode::kReadIR || code == FunctionCode::kReadCoils ||
        code == FunctionCode::kReadDI) {
      request.SetAddressSpan({0, 10});
    } else if (code == FunctionCode::kWriteSingleReg) {
      request.SetWriteSingleRegisterData(0, 1234);
    } else if (code == FunctionCode::kWriteSingleCoil) {
      request.SetWriteSingleCoilData(0, true);
    }

    auto encoded = RtuFrame::EncodeRequest(request);
    auto decoded = RtuFrame::DecodeRequest(encoded);

    ASSERT_TRUE(decoded.has_value()) << "Failed for function code: " << static_cast<int>(code);
    EXPECT_EQ(decoded->GetSlaveId(), 1);
    EXPECT_EQ(decoded->GetFunctionCode(), code);
  }
}
