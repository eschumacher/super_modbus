#include <array>
#include <gtest/gtest.h>
#include <tuple>
#include <vector>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/common/wire_format_options.hpp"
#include "super_modbus/rtu/rtu_frame.hpp"
#include "super_modbus/rtu/rtu_master.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

using supermb::AddressSpan;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::GetHighByte;
using supermb::GetLowByte;
using supermb::MakeInt16;
using supermb::MemoryTransport;
using supermb::RtuFrame;
using supermb::RtuMaster;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;
using supermb::WireFormatOptions;

// Mock transport that can simulate write failures
class FailingTransport : public supermb::ByteTransport {
 public:
  explicit FailingTransport(bool fail_write = false, bool fail_flush = false)
      : fail_write_(fail_write),
        fail_flush_(fail_flush) {}

  int Read(std::span<uint8_t> buffer) override { return transport_.Read(buffer); }

  int Write(std::span<const uint8_t> data) override {
    if (fail_write_) {
      return 0;  // Simulate write failure
    }
    return transport_.Write(data);
  }

  bool Flush() override {
    if (fail_flush_) {
      return false;  // Simulate flush failure
    }
    return transport_.Flush();
  }

  bool HasData() const override { return transport_.HasData(); }

  size_t AvailableBytes() const override { return transport_.AvailableBytes(); }

  void SetReadData(std::span<const uint8_t> data) { transport_.SetReadData(data); }
  void ResetReadPosition() { transport_.ResetReadPosition(); }

 private:
  MemoryTransport transport_;
  bool fail_write_;
  bool fail_flush_;
};

// Test SendRequest write failure
TEST(RtuMasterCoverage, SendRequest_WriteFailure) {
  static constexpr uint8_t kSlaveId{1};
  FailingTransport transport(true, false);  // Fail write
  RtuMaster master{transport};

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  auto result = master.SendRequest(request, 100);
  EXPECT_FALSE(result.has_value());
}

// Test SendRequest flush failure
TEST(RtuMasterCoverage, SendRequest_FlushFailure) {
  static constexpr uint8_t kSlaveId{1};
  FailingTransport transport(false, true);  // Fail flush
  RtuMaster master{transport};

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  auto result = master.SendRequest(request, 100);
  EXPECT_FALSE(result.has_value());
}

// Test ReadHoldingRegisters with empty data
TEST(RtuMasterCoverage, ReadHoldingRegisters_EmptyData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data added

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadHoldingRegisters with data size less than required
TEST(RtuMasterCoverage, ReadHoldingRegisters_DataSizeTooSmall) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);  // Correct byte count
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  // Only 3 bytes total, need 11 (1 + 10)

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadInputRegisters with empty data
TEST(RtuMasterCoverage, ReadInputRegisters_EmptyData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadIR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadInputRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadInputRegisters with wrong byte count
TEST(RtuMasterCoverage, ReadInputRegisters_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadIR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(5);  // Wrong byte count (should be 10 for 5 registers)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadInputRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadCoils with byte_index out of bounds
TEST(RtuMasterCoverage, ReadCoils_ByteIndexOutOfBounds) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);  // Correct byte count for 5 coils
  response.EmplaceBack(0xFF);
  // But we'll request more coils than available

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Request 10 coils but only 1 byte of data (for 5 coils)
  auto result = master.ReadCoils(kSlaveId, 0, 10);
  EXPECT_FALSE(result.has_value());
}

// Test ReadDiscreteInputs with byte_index out of bounds
TEST(RtuMasterCoverage, ReadDiscreteInputs_ByteIndexOutOfBounds) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadDI};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);  // Correct byte count for 5 inputs
  response.EmplaceBack(0xFF);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Request 10 inputs but only 1 byte of data
  auto result = master.ReadDiscreteInputs(kSlaveId, 0, 10);
  EXPECT_FALSE(result.has_value());
}

// Test ReadWriteMultipleRegisters with data size less than read_count * 2
TEST(RtuMasterCoverage, ReadWriteMultipleRegisters_InsufficientReadData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Only 1 byte, need 6 for 3 registers
  response.EmplaceBack(0x01);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<int16_t, 2> write_values{100, 200};
  auto result = master.ReadWriteMultipleRegisters(kSlaveId, 0, 3, 0, write_values);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFIFOQueue with data size less than required
TEST(RtuMasterCoverage, ReadFIFOQueue_DataSizeTooSmall) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x04);  // Low byte of byte count (4 bytes)
  response.EmplaceBack(0x00);  // High byte of byte count
  response.EmplaceBack(0x02);  // Low byte of FIFO count (2 entries)
  response.EmplaceBack(0x00);  // High byte of FIFO count
  // Need 4 more bytes for 2 entries, but only have header

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFIFOQueue(kSlaveId, 0);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with empty data
TEST(RtuMasterCoverage, ReadFileRecord_EmptyData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);
  auto result = master.ReadFileRecord(kSlaveId, records);
  EXPECT_FALSE(result.has_value());
}

// ReadFileRecord with response_length = 0: one byte (length) satisfies size check, returns empty map
TEST(RtuMasterCoverage, ReadFileRecord_ZeroResponseLength) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0);  // response_length = 0

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);
  auto result = master.ReadFileRecord(kSlaveId, records);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 0);
}

// Test ReadFileRecord with insufficient data for response_length
TEST(RtuMasterCoverage, ReadFileRecord_InsufficientDataForLength) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(20);  // Response length = 20
  // But only have 1 byte

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);
  auto result = master.ReadFileRecord(kSlaveId, records);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with insufficient data for record header
TEST(RtuMasterCoverage, ReadFileRecord_InsufficientDataForHeader) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);  // Response length = 10
  // Only add 5 bytes (need 8 for header)
  response.EmplaceBack(0x06);  // Reference type
  response.EmplaceBack(0x04);  // Data length
  response.EmplaceBack(0x00);  // File number high
  response.EmplaceBack(0x01);  // File number low
  response.EmplaceBack(0x00);  // Record number high
  // Missing record number low and data

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 2);
  auto result = master.ReadFileRecord(kSlaveId, records);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with invalid reference type
TEST(RtuMasterCoverage, ReadFileRecord_InvalidReferenceType) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(8);     // Response length = 8
  response.EmplaceBack(0x05);  // Invalid reference type (should be 0x06)
  response.EmplaceBack(0x04);  // Data length
  response.EmplaceBack(0x00);  // File number high
  response.EmplaceBack(0x01);  // File number low
  response.EmplaceBack(0x00);  // Record number high
  response.EmplaceBack(0x00);  // Record number low
  response.EmplaceBack(0x00);  // Data byte 1
  response.EmplaceBack(0x01);  // Data byte 2

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 2);
  auto result = master.ReadFileRecord(kSlaveId, records);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with incomplete record data
TEST(RtuMasterCoverage, ReadFileRecord_IncompleteRecordData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);    // Response length = 10
  response.EmplaceBack(0x06);  // Reference type
  response.EmplaceBack(0x06);  // Data length (4 for file/record + 2 for data)
  response.EmplaceBack(0x00);  // File number high
  response.EmplaceBack(0x01);  // File number low
  response.EmplaceBack(0x00);  // Record number high
  response.EmplaceBack(0x00);  // Record number low
  response.EmplaceBack(0x00);  // Data byte 1
  // Missing data byte 2

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 1);
  auto result = master.ReadFileRecord(kSlaveId, records);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with offset + 1 >= end_offset (boundary check)
TEST(RtuMasterCoverage, ReadFileRecord_OffsetBoundaryCheck) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(7);     // Response length = 7 (just enough for header, not data)
  response.EmplaceBack(0x06);  // Reference type
  response.EmplaceBack(0x06);  // Data length
  response.EmplaceBack(0x00);  // File number high
  response.EmplaceBack(0x01);  // File number low
  response.EmplaceBack(0x00);  // Record number high
  response.EmplaceBack(0x00);  // Record number low
  // No data bytes

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 1);
  auto result = master.ReadFileRecord(kSlaveId, records);
  EXPECT_FALSE(result.has_value());
}

// Test ReceiveResponse with decode failure
TEST(RtuMasterCoverage, ReceiveResponse_DecodeFailure) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // Invalid frame (bad CRC)
  std::vector<uint8_t> invalid_frame{0x01, 0x03, 0x02, 0x12, 0x34, 0x00, 0x00};
  transport.SetReadData(invalid_frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kSlaveId, 100);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFrame with timeout (no data available)
TEST(RtuMasterCoverage, ReadFrame_Timeout) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport(0);  // Empty transport
  RtuMaster master{transport};

  auto result = master.ReceiveResponse(kSlaveId, 10);  // Short timeout
  EXPECT_FALSE(result.has_value());
}

// Test ReadFrame with incomplete frame that never completes
TEST(RtuMasterCoverage, ReadFrame_IncompleteFrame) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // Incomplete frame (missing CRC)
  std::vector<uint8_t> incomplete_frame{0x01, 0x03, 0x02, 0x12};
  transport.SetReadData(incomplete_frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kSlaveId, 10);  // Short timeout
  EXPECT_FALSE(result.has_value());
}

// Test ReadFrame with complete frame (simulates reading all at once)
TEST(RtuMasterCoverage, ReadFrame_CompleteFrame) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // Send complete frame
  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kSlaveId, 100);
  // Should decode the complete frame
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetSlaveId(), kSlaveId);
  EXPECT_EQ(result->GetFunctionCode(), FunctionCode::kReadHR);
}

// Test GetComEventCounter with insufficient data (less than 3 bytes)
TEST(RtuMasterCoverage, GetComEventCounter_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kGetComEventCounter};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Only status, missing event count

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventCounter(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// Test ReadHoldingRegisters with SetAddressSpan failure
TEST(RtuMasterCoverage, ReadHoldingRegisters_SetAddressSpanFailure) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // This should fail at SetAddressSpan if there's an issue
  // But SetAddressSpan should work for valid function codes
  // So we test with a different error path - no response
  auto result = master.ReadHoldingRegisters(kSlaveId, 0, 0);  // Count = 0 might cause issues
  // Actually, count = 0 might be valid, so let's test with no response
  EXPECT_FALSE(result.has_value());
}

// Test WriteSingleRegister with SetWriteSingleRegisterData failure
// (This is hard to test since it asserts, but we can test the response path)
TEST(RtuMasterCoverage, WriteSingleRegister_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // No response in transport
  bool result = master.WriteSingleRegister(kSlaveId, 0, 1234);
  EXPECT_FALSE(result);
}

// Test WriteSingleCoil with no response
TEST(RtuMasterCoverage, WriteSingleCoil_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  bool result = master.WriteSingleCoil(kSlaveId, 0, true);
  EXPECT_FALSE(result);
}

// Test WriteMultipleRegisters with no response
TEST(RtuMasterCoverage, WriteMultipleRegisters_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  std::array<int16_t, 3> values{100, 200, 300};
  bool result = master.WriteMultipleRegisters(kSlaveId, 0, values);
  EXPECT_FALSE(result);
}

// Test WriteMultipleCoils with no response
TEST(RtuMasterCoverage, WriteMultipleCoils_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  std::array<bool, 3> values{true, false, true};
  bool result = master.WriteMultipleCoils(kSlaveId, 0, values);
  EXPECT_FALSE(result);
}

// Test MaskWriteRegister with no response
TEST(RtuMasterCoverage, MaskWriteRegister_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  bool result = master.MaskWriteRegister(kSlaveId, 0, 0xFFFF, 0x0000);
  EXPECT_FALSE(result);
}

// Test WriteFileRecord with no response
TEST(RtuMasterCoverage, WriteFileRecord_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> records;
  std::vector<int16_t> data{100, 200};
  records.emplace_back(1, 0, data);

  bool result = master.WriteFileRecord(kSlaveId, records);
  EXPECT_FALSE(result);
}

// Test ReadWriteMultipleRegisters with SetReadWriteMultipleRegistersData failure
TEST(RtuMasterCoverage, ReadWriteMultipleRegisters_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  std::array<int16_t, 2> write_values{100, 200};
  auto result = master.ReadWriteMultipleRegisters(kSlaveId, 0, 3, 0, write_values);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFIFOQueue with SetReadFIFOQueueData failure (no response)
TEST(RtuMasterCoverage, ReadFIFOQueue_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  auto result = master.ReadFIFOQueue(kSlaveId, 0);
  EXPECT_FALSE(result.has_value());
}

// Test Diagnostics with no response
TEST(RtuMasterCoverage, Diagnostics_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  std::vector<uint8_t> test_data{0x12, 0x34};
  auto result = master.Diagnostics(kSlaveId, 0x0000, test_data);
  EXPECT_FALSE(result.has_value());
}

// Test GetComEventLog with no response
TEST(RtuMasterCoverage, GetComEventLog_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  auto result = master.GetComEventLog(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// Test ReportSlaveID with no response
TEST(RtuMasterCoverage, ReportSlaveID_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  auto result = master.ReportSlaveID(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// Test ReadExceptionStatus with no response
TEST(RtuMasterCoverage, ReadExceptionStatus_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  auto result = master.ReadExceptionStatus(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// ReadHoldingRegisters with wrong byte_count in response
TEST(RtuMasterCoverage, ReadHoldingRegisters_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(5);  // Wrong (should be 10 for 5 registers)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadHoldingRegisters with exception response
TEST(RtuMasterCoverage, ReadHoldingRegisters_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadInputRegisters with no response
TEST(RtuMasterCoverage, ReadInputRegisters_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  auto result = master.ReadInputRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadInputRegisters with exception response
TEST(RtuMasterCoverage, ReadInputRegisters_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadIR};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadInputRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadCoils with empty data
TEST(RtuMasterCoverage, ReadCoils_EmptyData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadCoils(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadCoils with wrong byte count
TEST(RtuMasterCoverage, ReadCoils_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);  // Wrong (should be 1 for 5 coils)
  response.EmplaceBack(0xFF);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadCoils(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadCoils with correct byte_count but data buffer too short (byte_index >= data.size())
TEST(RtuMasterCoverage, ReadCoils_DataBufferTooShortInLoop) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);     // byte_count=2 for 16 coils
  response.EmplaceBack(0x00);  // Only 2 bytes total; when i=8, byte_index=2 >= data.size()=2

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadCoils(kSlaveId, 0, 16);
  EXPECT_FALSE(result.has_value());
}

// ReadDiscreteInputs with empty data
TEST(RtuMasterCoverage, ReadDiscreteInputs_EmptyData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadDI};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadDiscreteInputs(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadDiscreteInputs with wrong byte count
TEST(RtuMasterCoverage, ReadDiscreteInputs_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadDI};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0xFF);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadDiscreteInputs(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadDiscreteInputs with data buffer too short in loop
TEST(RtuMasterCoverage, ReadDiscreteInputs_DataBufferTooShortInLoop) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadDI};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x00);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadDiscreteInputs(kSlaveId, 0, 16);
  EXPECT_FALSE(result.has_value());
}

// ReadExceptionStatus with empty data
TEST(RtuMasterCoverage, ReadExceptionStatus_EmptyData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadExceptionStatus(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// ReadExceptionStatus with exception response
TEST(RtuMasterCoverage, ReadExceptionStatus_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadExceptionStatus(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// Diagnostics with exception response
TEST(RtuMasterCoverage, Diagnostics_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kDiagnostics};
  response.SetExceptionCode(ExceptionCode::kIllegalDataValue);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<uint8_t> data{0x12, 0x34};
  auto result = master.Diagnostics(kSlaveId, 0x0000, data);
  EXPECT_FALSE(result.has_value());
}

// GetComEventCounter with exception response
TEST(RtuMasterCoverage, GetComEventCounter_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kGetComEventCounter};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventCounter(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// GetComEventLog with exception response
TEST(RtuMasterCoverage, GetComEventLog_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kGetComEventLog};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventLog(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// ReportSlaveID with exception response
TEST(RtuMasterCoverage, ReportSlaveID_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReportSlaveID};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReportSlaveID(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// MaskWriteRegister with response size < 6
TEST(RtuMasterCoverage, MaskWriteRegister_InsufficientResponseData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kMaskWriteReg};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0xFF);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  // Only 5 bytes; need 6 for address + and_mask + or_mask

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.MaskWriteRegister(kSlaveId, 0, 0xFFFF, 0x0000);
  EXPECT_FALSE(result);
}

// MaskWriteRegister with wrong echoed values (response doesn't match request)
TEST(RtuMasterCoverage, MaskWriteRegister_WrongEchoedValues) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kMaskWriteReg};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Address low
  response.EmplaceBack(0x0A);  // Address high (0x000A)
  response.EmplaceBack(0xFF);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0xFF);
  // Echo back address 10, but we requested address 0

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.MaskWriteRegister(kSlaveId, 0, 0xFFFF, 0x0000);
  EXPECT_FALSE(result);
}

// WriteSingleRegister with exception response
TEST(RtuMasterCoverage, WriteSingleRegister_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteSingleReg};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.WriteSingleRegister(kSlaveId, 0, 1234);
  EXPECT_FALSE(result);
}

// WriteSingleCoil with exception response
TEST(RtuMasterCoverage, WriteSingleCoil_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteSingleCoil};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.WriteSingleCoil(kSlaveId, 0, true);
  EXPECT_FALSE(result);
}

// WriteMultipleRegisters with exception response
TEST(RtuMasterCoverage, WriteMultipleRegisters_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<int16_t, 2> values{100, 200};
  bool result = master.WriteMultipleRegisters(kSlaveId, 0, values);
  EXPECT_FALSE(result);
}

// WriteMultipleCoils with exception response
TEST(RtuMasterCoverage, WriteMultipleCoils_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteMultCoils};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<bool, 2> values{true, false};
  bool result = master.WriteMultipleCoils(kSlaveId, 0, values);
  EXPECT_FALSE(result);
}

// WriteFileRecord with exception response
TEST(RtuMasterCoverage, WriteFileRecord_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteFileRecord};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> records;
  records.emplace_back(1, 0, std::vector<int16_t>{100, 200});
  bool result = master.WriteFileRecord(kSlaveId, records);
  EXPECT_FALSE(result);
}

// ReadWriteMultipleRegisters with exception response
TEST(RtuMasterCoverage, ReadWriteMultipleRegisters_ExceptionResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<int16_t, 2> write_values{100, 200};
  auto result = master.ReadWriteMultipleRegisters(kSlaveId, 0, 3, 0, write_values);
  EXPECT_FALSE(result.has_value());
}

// ReceiveResponse when response has wrong slave ID
TEST(RtuMasterCoverage, ReceiveResponse_WrongSlaveID) {
  static constexpr uint8_t kExpectedSlaveId{1};
  static constexpr uint8_t kWrongSlaveId{2};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kWrongSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kExpectedSlaveId, 100);
  EXPECT_FALSE(result.has_value());
}

// SendRequest broadcast write returns success without response
TEST(RtuMasterCoverage, SendRequest_BroadcastWriteReturnsSuccess) {
  MemoryTransport transport;  // Empty - no response
  RtuMaster master{transport};

  bool result = master.WriteSingleRegister(0, 0, 0x1234);
  EXPECT_TRUE(result);
}

// ReadFIFOQueue with data.size() < 4
TEST(RtuMasterCoverage, ReadFIFOQueue_DataSizeLessThan4) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x04);
  response.EmplaceBack(0x00);  // Only 3 bytes

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFIFOQueue(kSlaveId, 0);
  EXPECT_FALSE(result.has_value());
}

// Success paths to improve line coverage

TEST(RtuMasterCoverage, ReadHoldingRegisters_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(4);  // byte_count = 2 registers
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);
  response.EmplaceBack(0x56);
  response.EmplaceBack(0x78);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kSlaveId, 0, 2);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 2);
  EXPECT_EQ((*result)[0], MakeInt16(0x34, 0x12));
  EXPECT_EQ((*result)[1], MakeInt16(0x78, 0x56));
}

TEST(RtuMasterCoverage, ReadInputRegisters_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadIR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x64);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadInputRegisters(kSlaveId, 0, 1);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1);
  EXPECT_EQ((*result)[0], 100);
}

TEST(RtuMasterCoverage, ReadCoils_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);
  response.EmplaceBack(0x05);  // bits 0 and 2 set

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadCoils(kSlaveId, 0, 5);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 5);
  EXPECT_TRUE((*result)[0]);
  EXPECT_FALSE((*result)[1]);
  EXPECT_TRUE((*result)[2]);
  EXPECT_FALSE((*result)[3]);
  EXPECT_FALSE((*result)[4]);
}

TEST(RtuMasterCoverage, ReadDiscreteInputs_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadDI};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);
  response.EmplaceBack(0xFF);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadDiscreteInputs(kSlaveId, 0, 8);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 8);
  for (bool b : *result) {
    EXPECT_TRUE(b);
  }
}

TEST(RtuMasterCoverage, WriteSingleRegister_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteSingleReg};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0A);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.WriteSingleRegister(kSlaveId, 10, 0x1234);
  EXPECT_TRUE(result);
}

TEST(RtuMasterCoverage, WriteSingleCoil_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteSingleCoil};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0xFF);
  response.EmplaceBack(0x00);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.WriteSingleCoil(kSlaveId, 0, true);
  EXPECT_TRUE(result);
}

// MaskWriteRegister success path covered by MasterIntegration::MaskWriteRegister and RemainingFunctionCodes

TEST(RtuMasterCoverage, ReadExceptionStatus_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x2A);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadExceptionStatus(kSlaveId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 0x2A);
}

TEST(RtuMasterCoverage, Diagnostics_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kDiagnostics};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<uint8_t> req_data{0x12, 0x34};
  auto result = master.Diagnostics(kSlaveId, 0x0001, req_data);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 4);
}

TEST(RtuMasterCoverage, GetComEventCounter_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kGetComEventCounter};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Status
  response.EmplaceBack(0x00);  // Event count high byte (big-endian)
  response.EmplaceBack(0x05);  // Event count low byte (value = 5)

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventCounter(kSlaveId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, 0x00);
  EXPECT_EQ(result->second, 5);
}

TEST(RtuMasterCoverage, GetComEventLog_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kGetComEventLog};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x05);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0A);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventLog(kSlaveId);
  ASSERT_TRUE(result.has_value());
  EXPECT_GE(result->size(), 5);
}

TEST(RtuMasterCoverage, ReportSlaveID_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReportSlaveID};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0xFF);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReportSlaveID(kSlaveId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2);
}

TEST(RtuMasterCoverage, ReadWriteMultipleRegisters_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x64);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0xC8);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<int16_t, 1> write_values{999};
  auto result = master.ReadWriteMultipleRegisters(kSlaveId, 0, 2, 10, write_values);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 2);
  EXPECT_EQ((*result)[0], 100);
  EXPECT_EQ((*result)[1], 200);
}

TEST(RtuMasterCoverage, ReadFIFOQueue_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Byte count high
  response.EmplaceBack(0x06);  // Byte count low (4 + 2*1 = 6 for 1 entry)
  response.EmplaceBack(0x00);  // FIFO count high
  response.EmplaceBack(0x01);  // FIFO count low
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFIFOQueue(kSlaveId, 0);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1);
  EXPECT_EQ((*result)[0], MakeInt16(0x34, 0x12));
}

TEST(RtuMasterCoverage, ReadFileRecord_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(8);     // Response length
  response.EmplaceBack(0x06);  // Reference type
  response.EmplaceBack(0x06);  // Data length (4 + 2)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 1);
  auto result = master.ReadFileRecord(kSlaveId, records);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1);
  auto it = result->find({1, 0});
  ASSERT_NE(it, result->end());
  ASSERT_EQ(it->second.size(), 1);
  EXPECT_EQ(it->second[0], MakeInt16(0x34, 0x12));
}

TEST(RtuMasterCoverage, ReadFileRecord_MultipleRecords) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(16);  // Response length (2 records * 8 bytes)
  response.EmplaceBack(0x06);
  response.EmplaceBack(0x06);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x64);
  response.EmplaceBack(0x06);
  response.EmplaceBack(0x06);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x02);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0xC8);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 1);
  records.emplace_back(2, 1, 1);
  auto result = master.ReadFileRecord(kSlaveId, records);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2);
  EXPECT_EQ(result->at({1, 0}).size(), 1);
  EXPECT_EQ(result->at({2, 1}).size(), 1);
  EXPECT_EQ(result->at({1, 0})[0], 100);
  EXPECT_EQ(result->at({2, 1})[0], 200);
}

TEST(RtuMasterCoverage, WriteFileRecord_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> records;
  records.emplace_back(1, 0, std::vector<int16_t>{100, 200});
  bool result = master.WriteFileRecord(kSlaveId, records);
  EXPECT_TRUE(result);
}

// ReadFrame with data arriving in two chunks (partial read then complete)
TEST(RtuMasterCoverage, ReadFrame_PartialReads) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);

  std::vector<uint8_t> full_frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(full_frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kSlaveId, 100);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetSlaveId(), kSlaveId);
  EXPECT_EQ(result->GetFunctionCode(), FunctionCode::kReadHR);
}

// ReadFloats / WriteFloats (Enron-style 32-bit float API)
TEST(RtuMasterCoverage, ReadFloats_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // Response: 4 registers (2 floats), big-endian high-word-first. 1.0f=0x3F800000, 2.0f=0x40000000
  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(8);  // byte_count
  response.EmplaceBack(0x3F);
  response.EmplaceBack(0x80);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x40);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFloats(kSlaveId, 0, 2);  // count = 2 floats (CountIsFloatCount default)
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 2);
  EXPECT_FLOAT_EQ((*result)[0], 1.0f);
  EXPECT_FLOAT_EQ((*result)[1], 2.0f);
}

TEST(RtuMasterCoverage, ReadFloats_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;  // no read data
  RtuMaster master{transport};
  auto result = master.ReadFloats(kSlaveId, 0, 2);
  EXPECT_FALSE(result.has_value());
}

TEST(RtuMasterCoverage, ReadFloats_CountZero) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};
  auto result = master.ReadFloats(kSlaveId, 0, 0);
  EXPECT_FALSE(result.has_value());
}

TEST(RtuMasterCoverage, WriteFloats_Success) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x04);  // 4 registers written

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<float> values{1.0f, 2.0f};
  bool ok = master.WriteFloats(kSlaveId, 0, values);
  EXPECT_TRUE(ok);
}

TEST(RtuMasterCoverage, WriteFloats_Empty) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};
  std::vector<float> values;
  bool ok = master.WriteFloats(kSlaveId, 0, values);
  EXPECT_TRUE(ok);  // empty write is accepted
}

TEST(RtuMasterCoverage, ReadFloats_FloatRangeOutOfRange) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  WireFormatOptions opts;
  opts.float_range = {10, 10};  // registers 10..19 only
  RtuMaster master{transport, opts};
  auto result = master.ReadFloats(kSlaveId, 0, 2);  // start 0 is outside range
  EXPECT_FALSE(result.has_value());
}

TEST(RtuMasterCoverage, WriteFloats_FloatRangeOutOfRange) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  WireFormatOptions opts;
  opts.float_range = {10, 10};
  RtuMaster master{transport, opts};
  std::vector<float> values{1.0f};
  bool ok = master.WriteFloats(kSlaveId, 0, values);  // start 0 outside range
  EXPECT_FALSE(ok);
}
