#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_frame.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

using supermb::AddressSpan;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::MemoryTransport;
using supermb::RtuFrame;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

TEST(ErrorHandling, InvalidAddress) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Try to read from invalid address
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({100, 5});  // Address not in range
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(ErrorHandling, WrongSlaveID) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr uint8_t kWrongSlaveId{99};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Request with wrong slave ID
  RtuRequest request{{kWrongSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  // Process on transport - should not process (wrong slave ID)
  MemoryTransport transport;
  auto frame = RtuFrame::EncodeRequest(request);
  transport.SetReadData(frame);

  bool processed = slave.ProcessIncomingFrame(transport, 1000);
  EXPECT_FALSE(processed);  // Should not process request for wrong slave
}

TEST(ErrorHandling, InvalidFunctionCode) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Request with invalid function code (0)
  RtuRequest request{{kSlaveId, FunctionCode::kInvalid}};
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalFunction);
}

TEST(ErrorHandling, WriteToReadOnlyRegister) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddInputRegisters({0, 10});  // Input registers are read-only

  // Try to write to input register
  RtuRequest request{{kSlaveId, FunctionCode::kWriteSingleReg}};
  request.SetWriteSingleRegisterData(0, 100);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(ErrorHandling, WriteToReadOnlyDiscreteInput) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddDiscreteInputs({0, 10});  // Discrete inputs are read-only

  // Try to write to discrete input
  RtuRequest request{{kSlaveId, FunctionCode::kWriteSingleCoil}};
  request.SetWriteSingleCoilData(0, true);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(ErrorHandling, InvalidFrameSize) {
  // Frame too small
  std::vector<uint8_t> invalid_frame{0x01, 0x03};  // Missing CRC
  auto decoded = RtuFrame::DecodeRequest(invalid_frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(ErrorHandling, CorruptedCRC) {
  static constexpr uint8_t kSlaveId{1};

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = RtuFrame::EncodeRequest(request);

  // Corrupt the CRC
  frame[frame.size() - 1] ^= 0xFF;

  auto decoded = RtuFrame::DecodeRequest(frame);
  EXPECT_FALSE(decoded.has_value());  // Should reject corrupted frame
}

TEST(ErrorHandling, InvalidDataValue) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Write multiple registers with invalid byte count
  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultRegs}};
  std::vector<uint8_t> invalid_data{
      0x00, 0x00,  // Address
      0x00, 0x02,  // Count = 2
      0x03,        // Byte count = 3 (should be 4 for 2 registers)
      0x12, 0x34, 0x56  // Only 3 bytes of data
  };
  request.SetRawData(invalid_data);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(ErrorHandling, AddressOutOfRange) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Try to read beyond available range
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({5, 10});  // Start at 5, read 10 = goes to 15, but only 0-9 available
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(ErrorHandling, ZeroCount) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Try to read zero registers
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 0});
  RtuResponse response = slave.Process(request);

  // Should still work but return only byte_count (which will be 0)
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  // Zero count means byte_count = 0, so data size is 1 (just the byte_count byte)
  EXPECT_EQ(response.GetData().size(), 1);
}

TEST(ErrorHandling, MaximumCount) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr uint16_t kMaxRegisters = 125;  // Modbus limit

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, kMaxRegisters});

  // Read maximum allowed registers
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, kMaxRegisters});
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  // Read register responses include byte_count (1 byte) + register data (kMaxRegisters * 2 bytes)
  EXPECT_EQ(response.GetData().size(), 1 + kMaxRegisters * 2);
}

TEST(ErrorHandling, InvalidMaskWriteData) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Mask write with insufficient data
  RtuRequest request{{kSlaveId, FunctionCode::kMaskWriteReg}};
  std::vector<uint8_t> invalid_data{0x00, 0x00};  // Missing masks
  request.SetRawData(invalid_data);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(ErrorHandling, InvalidReadWriteMultipleData) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Read/Write multiple with insufficient data
  RtuRequest request{{kSlaveId, FunctionCode::kReadWriteMultRegs}};
  std::vector<uint8_t> invalid_data{0x00, 0x00};  // Missing required fields
  request.SetRawData(invalid_data);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(ErrorHandling, FrameWithInvalidSlaveIDInResponse) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr uint8_t kExpectedSlaveId{1};
  static constexpr uint8_t kWrongSlaveId{99};

  // Create response with wrong slave ID
  RtuResponse response{kWrongSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  auto frame = RtuFrame::EncodeResponse(response);

  // Try to decode and verify slave ID mismatch
  auto decoded = RtuFrame::DecodeResponse(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_NE(decoded->GetSlaveId(), kExpectedSlaveId);
}
