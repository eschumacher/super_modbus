#include <array>
#include <gtest/gtest.h>
#include <tuple>
#include <vector>
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
using supermb::GetHighByte;
using supermb::GetLowByte;
using supermb::MakeInt16;
using supermb::MemoryTransport;
using supermb::RtuFrame;
using supermb::RtuMaster;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

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

// Test ReadFileRecord with response_length = 0
// Note: The code checks data.size() < 1, so response_length = 0 with only 1 byte
// should pass the size check but return empty map (no records)
// However, looking at the code, if response_length = 0, the while loop won't execute
// and it returns an empty map, which is valid. But the test might be hitting
// a different code path. Let's test with a different scenario.
TEST(RtuMasterCoverage, ReadFileRecord_ZeroResponseLength) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuResponse response{kSlaveId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0);  // Response length = 0

  auto frame = RtuFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);
  auto result = master.ReadFileRecord(kSlaveId, records);
  // The code checks: if (data.size() < static_cast<size_t>(1 + response_length))
  // With response_length=0, this is: if (data.size() < 1)
  // We have 1 byte (the response_length byte), so it passes
  // Then while(offset < end_offset) where offset=1, end_offset=1, so loop doesn't run
  // Returns empty map, which is valid
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
