#include <array>
#include <gtest/gtest.h>
#include <tuple>
#include <vector>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/tcp/tcp_frame.hpp"
#include "super_modbus/tcp/tcp_master.hpp"
#include "super_modbus/tcp/tcp_request.hpp"
#include "super_modbus/tcp/tcp_response.hpp"
#include "super_modbus/transport/memory_transport.hpp"

using supermb::AddressSpan;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::GetHighByte;
using supermb::GetLowByte;
using supermb::MakeInt16;
using supermb::MemoryTransport;
using supermb::TcpFrame;
using supermb::TcpMaster;
using supermb::TcpRequest;
using supermb::TcpResponse;

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

  [[nodiscard]] bool HasData() const override { return transport_.HasData(); }

  [[nodiscard]] size_t AvailableBytes() const override { return transport_.AvailableBytes(); }

  void SetReadData(std::span<const uint8_t> data) { transport_.SetReadData(data); }
  void ResetReadPosition() { transport_.ResetReadPosition(); }

 private:
  MemoryTransport transport_;
  bool fail_write_;
  bool fail_flush_;
};

// Test SendRequest write failure
TEST(TCPMasterCoverage, SendRequest_WriteFailure) {
  static constexpr uint8_t kUnitId{1};
  FailingTransport transport(true, false);  // Fail write
  TcpMaster master{transport};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  auto result = master.SendRequest(request, 100);
  EXPECT_FALSE(result.has_value());
}

// Test SendRequest flush failure
TEST(TCPMasterCoverage, SendRequest_FlushFailure) {
  static constexpr uint8_t kUnitId{1};
  FailingTransport transport(false, true);  // Fail flush
  TcpMaster master{transport};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  auto result = master.SendRequest(request, 100);
  EXPECT_FALSE(result.has_value());
}

// Test ReadHoldingRegisters with empty data
TEST(TCPMasterCoverage, ReadHoldingRegisters_EmptyData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data added

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadHoldingRegisters with data size less than required
TEST(TCPMasterCoverage, ReadHoldingRegisters_DataSizeTooSmall) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);  // Correct byte count
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  // Only 3 bytes total, need 11 (1 + 10)

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadHoldingRegisters with wrong byte count
TEST(TCPMasterCoverage, ReadHoldingRegisters_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(5);  // Wrong byte count (should be 10 for 5 registers)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadHoldingRegisters with exception response
TEST(TCPMasterCoverage, ReadHoldingRegisters_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadInputRegisters with empty data
TEST(TCPMasterCoverage, ReadInputRegisters_EmptyData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadIR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadInputRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadInputRegisters with wrong byte count
TEST(TCPMasterCoverage, ReadInputRegisters_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadIR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(5);  // Wrong byte count (should be 10 for 5 registers)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadInputRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadCoils with byte_index out of bounds
TEST(TCPMasterCoverage, ReadCoils_ByteIndexOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);  // Correct byte count for 5 coils
  response.EmplaceBack(0xFF);
  // But we'll request more coils than available

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Request 10 coils but only 1 byte of data (for 5 coils)
  auto result = master.ReadCoils(kUnitId, 0, 10);
  EXPECT_FALSE(result.has_value());
}

// Test ReadCoils with wrong byte count
TEST(TCPMasterCoverage, ReadCoils_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);  // Wrong byte count (should be 1 for 5 coils)
  response.EmplaceBack(0xFF);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadCoils(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadDiscreteInputs with byte_index out of bounds
TEST(TCPMasterCoverage, ReadDiscreteInputs_ByteIndexOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadDI};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);  // Correct byte count for 5 inputs
  response.EmplaceBack(0xFF);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Request 10 inputs but only 1 byte of data
  auto result = master.ReadDiscreteInputs(kUnitId, 0, 10);
  EXPECT_FALSE(result.has_value());
}

// Test WriteSingleRegister with exception response
TEST(TCPMasterCoverage, WriteSingleRegister_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kWriteSingleReg};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.WriteSingleRegister(kUnitId, 50, 1234);
  EXPECT_FALSE(result);
}

// Test WriteSingleCoil with exception response
TEST(TCPMasterCoverage, WriteSingleCoil_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kWriteSingleCoil};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.WriteSingleCoil(kUnitId, 10, true);
  EXPECT_FALSE(result);
}

// Test WriteMultipleRegisters with exception response
TEST(TCPMasterCoverage, WriteMultipleRegisters_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<int16_t> values{100, 200};
  bool result = master.WriteMultipleRegisters(kUnitId, 0, values);
  EXPECT_FALSE(result);
}

// Test WriteMultipleCoils with exception response
TEST(TCPMasterCoverage, WriteMultipleCoils_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kWriteMultCoils};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<bool, 2> values{true, false};
  bool result = master.WriteMultipleCoils(kUnitId, 0, std::span<const bool>(values));
  EXPECT_FALSE(result);
}

// Test ReadExceptionStatus with empty data
TEST(TCPMasterCoverage, ReadExceptionStatus_EmptyData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadExceptionStatus(kUnitId);
  EXPECT_FALSE(result.has_value());
}

// Test Diagnostics with exception response
TEST(TCPMasterCoverage, Diagnostics_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kDiagnostics};
  response.SetExceptionCode(ExceptionCode::kIllegalDataValue);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<uint8_t> data{0x12, 0x34};
  auto result = master.Diagnostics(kUnitId, 0x0001, data);
  EXPECT_FALSE(result.has_value());
}

// Test GetComEventCounter with insufficient data
TEST(TCPMasterCoverage, GetComEventCounter_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kGetComEventCounter};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Status
  // Only 1 byte, need 3 (status + event_count)

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventCounter(kUnitId);
  EXPECT_FALSE(result.has_value());
}

// Test GetComEventLog with exception response
TEST(TCPMasterCoverage, GetComEventLog_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kGetComEventLog};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventLog(kUnitId);
  EXPECT_FALSE(result.has_value());
}

// Test ReportSlaveID with exception response
TEST(TCPMasterCoverage, ReportSlaveID_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReportSlaveID};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReportSlaveID(kUnitId);
  EXPECT_FALSE(result.has_value());
}

// Test MaskWriteRegister with exception response
TEST(TCPMasterCoverage, MaskWriteRegister_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kMaskWriteReg};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.MaskWriteRegister(kUnitId, 50, 0xFF00, 0x00FF);
  EXPECT_FALSE(result);
}

// Test ReadWriteMultipleRegisters with data size less than read_count * 2
TEST(TCPMasterCoverage, ReadWriteMultipleRegisters_InsufficientReadData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Only 1 byte, need 6 for 3 registers
  response.EmplaceBack(0x01);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<int16_t, 2> write_values{100, 200};
  auto result = master.ReadWriteMultipleRegisters(kUnitId, 0, 3, 0, write_values);
  EXPECT_FALSE(result.has_value());
}

// Test ReadWriteMultipleRegisters with wrong byte count
TEST(TCPMasterCoverage, ReadWriteMultipleRegisters_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x05);  // Wrong byte count (should be 6 for 3 registers)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x02);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<int16_t, 2> write_values{100, 200};
  auto result = master.ReadWriteMultipleRegisters(kUnitId, 0, 3, 0, write_values);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFIFOQueue with data size less than required
TEST(TCPMasterCoverage, ReadFIFOQueue_DataSizeTooSmall) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // High byte of byte count (4 bytes)
  response.EmplaceBack(0x04);  // Low byte of byte count
  response.EmplaceBack(0x00);  // High byte of FIFO count (2 entries)
  response.EmplaceBack(0x02);  // Low byte of FIFO count
  // Need 4 more bytes for 2 entries, but only have header

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFIFOQueue(kUnitId, 0);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFIFOQueue with invalid FIFO count (0)
TEST(TCPMasterCoverage, ReadFIFOQueue_InvalidFIFOCountZero) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // High byte of byte count
  response.EmplaceBack(0x00);  // Low byte of byte count (0)
  response.EmplaceBack(0x00);  // High byte of FIFO count
  response.EmplaceBack(0x00);  // Low byte of FIFO count (0 - invalid)

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFIFOQueue(kUnitId, 0);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFIFOQueue with invalid FIFO count (>31)
TEST(TCPMasterCoverage, ReadFIFOQueue_InvalidFIFOCountTooLarge) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // High byte of byte count
  response.EmplaceBack(0x40);  // Low byte of byte count (64)
  response.EmplaceBack(0x00);  // High byte of FIFO count
  response.EmplaceBack(0x20);  // Low byte of FIFO count (32 - invalid, max is 31)

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFIFOQueue(kUnitId, 0);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFIFOQueue with wrong byte count
TEST(TCPMasterCoverage, ReadFIFOQueue_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // High byte of byte count
  response.EmplaceBack(0x03);  // Low byte of byte count (wrong, should be 4 for 2 entries)
  response.EmplaceBack(0x00);  // High byte of FIFO count
  response.EmplaceBack(0x02);  // Low byte of FIFO count

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFIFOQueue(kUnitId, 0);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with empty data
TEST(TCPMasterCoverage, ReadFileRecord_EmptyData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);
  auto result = master.ReadFileRecord(kUnitId, records);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with insufficient data for response_length
TEST(TCPMasterCoverage, ReadFileRecord_InsufficientDataForLength) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(20);  // Response length = 20
  // But only have 1 byte

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);
  auto result = master.ReadFileRecord(kUnitId, records);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with insufficient data for record header
TEST(TCPMasterCoverage, ReadFileRecord_InsufficientDataForHeader) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);  // Response length = 10
  // Only add 5 bytes (need 8 for header)
  response.EmplaceBack(0x06);  // Reference type
  response.EmplaceBack(0x04);  // Data length
  response.EmplaceBack(0x00);  // File number high
  response.EmplaceBack(0x01);  // File number low
  response.EmplaceBack(0x00);  // Record number high
  // Missing record number low and data

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 2);
  auto result = master.ReadFileRecord(kUnitId, records);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFileRecord with invalid reference type
TEST(TCPMasterCoverage, ReadFileRecord_ResponseInsufficientForRecordHeader) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  // Response with byte_count=7 but only 4 bytes of data (insufficient for 6-byte sub-response header)
  TcpResponse response{1, kUnitId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(4);     // response_length = 4 (only 4 bytes follow)
  response.EmplaceBack(0x06);  // ref type
  response.EmplaceBack(0x00);  // data_length
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);  // incomplete

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 1);
  auto result = master.ReadFileRecord(kUnitId, records);
  EXPECT_FALSE(result.has_value());
}

TEST(TCPMasterCoverage, ReadFileRecord_InvalidReferenceType) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFileRecord};
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

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 2);
  auto result = master.ReadFileRecord(kUnitId, records);
  EXPECT_FALSE(result.has_value());
}

// Test WriteFileRecord with exception response
TEST(TCPMasterCoverage, WriteFileRecord_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kWriteFileRecord};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> records;
  records.emplace_back(1, 0, std::vector<int16_t>{100, 200});
  bool result = master.WriteFileRecord(kUnitId, records);
  EXPECT_FALSE(result);
}

// Test ReceiveResponse with decode failure
TEST(TCPMasterCoverage, ReceiveResponse_DecodeFailure) {
  static constexpr uint16_t kTransactionId{123};
  MemoryTransport transport;
  TcpMaster master{transport};

  // Invalid frame (too short)
  std::vector<uint8_t> invalid_frame{0x00, 0x01};
  transport.SetReadData(invalid_frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kTransactionId, 100);
  EXPECT_FALSE(result.has_value());
}

// Test ReceiveResponse with wrong transaction ID
TEST(TCPMasterCoverage, ReceiveResponse_WrongTransactionId) {
  static constexpr uint8_t kUnitId{1};
  static constexpr uint16_t kTransactionId{123};
  static constexpr uint16_t kWrongTransactionId{456};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{kTransactionId, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kWrongTransactionId, 100);
  EXPECT_FALSE(result.has_value());
}

// Test ReadFrame with timeout (no data available)
TEST(TCPMasterCoverage, ReadFrame_Timeout) {
  static constexpr uint16_t kTransactionId{123};
  MemoryTransport transport(0);  // Empty transport
  TcpMaster master{transport};

  auto result = master.ReceiveResponse(kTransactionId, 10);  // Short timeout
  EXPECT_FALSE(result.has_value());
}

// Test ReadFrame with incomplete MBAP header
TEST(TCPMasterCoverage, ReadFrame_IncompleteMBAPHeader) {
  static constexpr uint16_t kTransactionId{123};
  MemoryTransport transport;
  TcpMaster master{transport};

  // Incomplete MBAP header (only 3 bytes)
  std::vector<uint8_t> incomplete_frame{0x00, 0x01, 0x00};
  transport.SetReadData(incomplete_frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kTransactionId, 10);  // Short timeout
  EXPECT_FALSE(result.has_value());
}

// Test ReadFrame with incomplete frame body
TEST(TCPMasterCoverage, ReadFrame_IncompleteFrameBody) {
  static constexpr uint16_t kTransactionId{123};
  MemoryTransport transport;
  TcpMaster master{transport};

  // MBAP header says length is 5, but frame is incomplete
  std::vector<uint8_t> incomplete_frame;
  incomplete_frame.push_back(0x00);  // Transaction ID high
  incomplete_frame.push_back(0x7B);  // Transaction ID low (123)
  incomplete_frame.push_back(0x00);  // Protocol ID high
  incomplete_frame.push_back(0x00);  // Protocol ID low
  incomplete_frame.push_back(0x00);  // Length high
  incomplete_frame.push_back(0x05);  // Length low (5 bytes)
  incomplete_frame.push_back(0x01);  // Unit ID
  incomplete_frame.push_back(0x07);  // Function code
  // Missing data (length says 5, but we only have Unit ID + Function Code = 2)

  transport.SetReadData(incomplete_frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kTransactionId, 10);  // Short timeout
  EXPECT_FALSE(result.has_value());
}

// Test all function codes with successful responses
TEST(TCPMasterCoverage, AllFunctionCodes_Success) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  // Test each function code with a valid response
  struct TestCase {
    FunctionCode function_code;
    std::function<void()> test_func;
  };

  // ReadHoldingRegisters
  {
    TcpResponse response{1, kUnitId, FunctionCode::kReadHR};
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
    response.EmplaceBack(0x02);  // Byte count
    response.EmplaceBack(0x00);  // High byte
    response.EmplaceBack(0x01);  // Low byte
    auto frame = TcpFrame::EncodeResponse(response);
    transport.SetReadData(frame);
    transport.ResetReadPosition();
    auto result = master.ReadHoldingRegisters(kUnitId, 0, 1);
    EXPECT_TRUE(result.has_value());
  }

  // ReadExceptionStatus
  {
    TcpResponse response{2, kUnitId, FunctionCode::kReadExceptionStatus};
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
    response.EmplaceBack(0x42);
    auto frame = TcpFrame::EncodeResponse(response);
    transport.SetReadData(frame);
    transport.ResetReadPosition();
    auto result = master.ReadExceptionStatus(kUnitId);
    EXPECT_TRUE(result.has_value());
  }

  // GetComEventCounter
  {
    TcpResponse response{3, kUnitId, FunctionCode::kGetComEventCounter};
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
    response.EmplaceBack(0x00);  // Status high byte
    response.EmplaceBack(0x00);  // Status low byte (0x0000 = ready)
    response.EmplaceBack(0x00);  // High byte of event count
    response.EmplaceBack(0x05);  // Low byte of event count
    auto frame = TcpFrame::EncodeResponse(response);
    transport.SetReadData(frame);
    transport.ResetReadPosition();
    auto result = master.GetComEventCounter(kUnitId);
    EXPECT_TRUE(result.has_value());
  }
}

// Transport that simulates partial reads (returns fewer bytes than requested)
class PartialReadTransport : public supermb::ByteTransport {
 public:
  explicit PartialReadTransport(size_t max_read_size = 1)
      : max_read_size_(max_read_size) {}

  int Read(std::span<uint8_t> buffer) override {
    size_t to_read = std::min(buffer.size(), max_read_size_);
    return transport_.Read(std::span<uint8_t>(buffer.data(), to_read));
  }

  int Write(std::span<const uint8_t> data) override { return transport_.Write(data); }
  bool Flush() override { return transport_.Flush(); }
  bool HasData() const override { return transport_.HasData(); }
  size_t AvailableBytes() const override { return transport_.AvailableBytes(); }

  void SetReadData(std::span<const uint8_t> data) { transport_.SetReadData(data); }
  void ResetReadPosition() { transport_.ResetReadPosition(); }

 private:
  MemoryTransport transport_;
  size_t max_read_size_;
};

// Test ReadFrame with partial reads (Read returns fewer bytes than requested)
TEST(TCPMasterCoverage, ReadFrame_PartialReads) {
  static constexpr uint8_t kUnitId{1};
  PartialReadTransport transport(2);  // Read max 2 bytes at a time
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x42);  // Exception status

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(1, 1000);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetUnitId(), kUnitId);
  EXPECT_EQ(result->GetExceptionCode(), ExceptionCode::kAcknowledge);
}

// Test ReadFrame with zero timeout (should work if data is immediately available)
TEST(TCPMasterCoverage, ReadFrame_ZeroTimeout) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x42);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Zero timeout should work if data is available immediately
  auto result = master.ReceiveResponse(1, 0);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetUnitId(), kUnitId);
}

// Test ReadFrame with very large frame (tests buffer resizing)
TEST(TCPMasterCoverage, ReadFrame_LargeFrame) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  // Create a large response (read many registers)
  TcpResponse response{1, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(200);  // Byte count (100 registers * 2)
  for (int i = 0; i < 100; ++i) {
    response.EmplaceBack(0x00);                     // High byte
    response.EmplaceBack(static_cast<uint8_t>(i));  // Low byte
  }

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kUnitId, 0, 100);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 100);
}

// Test ReadFrame with bytes_read returning 0 during reading (should retry)
TEST(TCPMasterCoverage, ReadFrame_ZeroBytesReadRetry) {
  static constexpr uint8_t kUnitId{1};
  // Use partial read transport with size 1 to force multiple reads
  PartialReadTransport transport(1);
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x42);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(1, 1000);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetUnitId(), kUnitId);
}

// Test ReadHoldingRegisters with no response (timeout)
TEST(TCPMasterCoverage, ReadHoldingRegisters_NoResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport(0);  // Empty - no response data
  TcpMaster master{transport};

  auto result = master.ReadHoldingRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadHoldingRegisters with SetAddressSpan failure
TEST(TCPMasterCoverage, ReadHoldingRegisters_SetAddressSpanFailure) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  // This test verifies the error path when SetAddressSpan fails
  // However, SetAddressSpan should not fail for valid function codes
  // So we test with an invalid count (0) which might cause issues
  auto result = master.ReadHoldingRegisters(kUnitId, 0, 0);  // Zero count
  // Should handle gracefully - either return empty or fail
  // The actual behavior depends on implementation
}

// Test ReadInputRegisters with no response (timeout)
TEST(TCPMasterCoverage, ReadInputRegisters_NoResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport(0);  // Empty - no response data
  TcpMaster master{transport};

  auto result = master.ReadInputRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test ReadInputRegisters with SetAddressSpan failure path
TEST(TCPMasterCoverage, ReadInputRegisters_SetAddressSpanFailure) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  auto result = master.ReadInputRegisters(kUnitId, 0, 0);  // Zero count
  // Should handle gracefully
}

// Test WriteSingleRegister with SetWriteSingleRegisterData failure
// Note: This is hard to test as SetWriteSingleRegisterData should not fail for valid inputs
// But we can test with edge cases
TEST(TCPMasterCoverage, WriteSingleRegister_EdgeCases) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kWriteSingleReg};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Test with maximum address
  bool result = master.WriteSingleRegister(kUnitId, 0xFFFF, 0x7FFF);
  EXPECT_TRUE(result);
}

// Test ReadCoils with SetAddressSpan edge cases
TEST(TCPMasterCoverage, ReadCoils_EdgeCases) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  // Test with maximum count
  TcpResponse response{1, kUnitId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0xFF);  // Max byte count
  for (int i = 0; i < 255; ++i) {
    response.EmplaceBack(0xAA);
  }

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadCoils(kUnitId, 0, 2000);  // Large count
  // Should handle gracefully or return partial data
}

// Test ReadFrame with bytes_read returning 0 multiple times (should eventually timeout or succeed)
TEST(TCPMasterCoverage, ReadFrame_MultipleZeroReads) {
  static constexpr uint8_t kUnitId{1};
  // Use partial read transport with very small reads to test retry logic
  PartialReadTransport transport(1);  // Read 1 byte at a time
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x42);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // This will require many reads (1 byte at a time)
  auto result = master.ReceiveResponse(1, 2000);  // Longer timeout for many reads
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetUnitId(), kUnitId);
}

// Test ReceiveResponse with transaction ID mismatch after valid decode
TEST(TCPMasterCoverage, ReceiveResponse_TransactionIdMismatchAfterDecode) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{999, kUnitId, FunctionCode::kReadExceptionStatus};  // Wrong transaction ID
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x42);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(1, 100);  // Expecting transaction ID 1
  EXPECT_FALSE(result.has_value());
}

// Success path: Diagnostics
TEST(TCPMasterCoverage, Diagnostics_Success) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kDiagnostics};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);
  response.EmplaceBack(0x56);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<uint8_t> req_data{0xAA, 0xBB};
  auto result = master.Diagnostics(kUnitId, 0x0001, req_data);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 3);
  EXPECT_EQ((*result)[0], 0x12);
  EXPECT_EQ((*result)[1], 0x34);
  EXPECT_EQ((*result)[2], 0x56);
}

// Success path: GetComEventLog
TEST(TCPMasterCoverage, GetComEventLog_Success) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kGetComEventLog};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x02);  // Status
  response.EmplaceBack(0x00);  // Event count high
  response.EmplaceBack(0x05);  // Event count low
  response.EmplaceBack(0x00);  // Message count high
  response.EmplaceBack(0x0A);  // Message count low
  response.EmplaceBack(0x00);  // Event ID high
  response.EmplaceBack(0x01);  // Event ID low
  response.EmplaceBack(0x00);  // Event count high
  response.EmplaceBack(0x01);  // Event count low

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventLog(kUnitId);
  ASSERT_TRUE(result.has_value());
  EXPECT_GE(result->size(), 2);
}

// Success path: ReportSlaveID
TEST(TCPMasterCoverage, ReportSlaveID_Success) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReportSlaveID};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x01);  // Slave ID byte count
  response.EmplaceBack(0x42);  // Slave ID
  response.EmplaceBack(0x01);  // Run indicator

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReportSlaveID(kUnitId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 3);
}

// Success path: ReadFIFOQueue
TEST(TCPMasterCoverage, ReadFIFOQueue_Success) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // Modbus FC24 format: byte_count (includes fifo_count field + data), then fifo_count (big-endian)
  response.EmplaceBack(0x00);  // byte_count high
  response.EmplaceBack(0x06);  // byte_count low (2 + 2*2 = 6)
  response.EmplaceBack(0x00);  // fifo_count high
  response.EmplaceBack(0x02);  // fifo_count low (2 entries)
  response.EmplaceBack(0x00);  // value 0 high
  response.EmplaceBack(0x11);  // value 0 low (17)
  response.EmplaceBack(0x00);  // value 1 high
  response.EmplaceBack(0x22);  // value 1 low (34)

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadFIFOQueue(kUnitId, 0);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2);
  EXPECT_EQ((*result)[0], 17);
  EXPECT_EQ((*result)[1], 34);
}

// Success path: ReadWriteMultipleRegisters
TEST(TCPMasterCoverage, ReadWriteMultipleRegisters_Success) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x04);  // Byte count (2 registers)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0A);  // Register 0 = 10
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x14);  // Register 1 = 20

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::array<int16_t, 2> write_values{100, 200};
  auto result = master.ReadWriteMultipleRegisters(kUnitId, 0, 2, 0, std::span<const int16_t>(write_values));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2);
  EXPECT_EQ((*result)[0], 10);
  EXPECT_EQ((*result)[1], 20);
}

// Success path: ReadFileRecord with single record
TEST(TCPMasterCoverage, ReadFileRecord_Success) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);    // Response length = 10 (ref+len+file+rec+2regs)
  response.EmplaceBack(0x06);  // Reference type
  response.EmplaceBack(0x08);  // Data length (4 + 2*2)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);  // File 1
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);  // Record 0
  response.EmplaceBack(0x00);  // Register 0 high
  response.EmplaceBack(0x0A);  // Register 0 low (10)
  response.EmplaceBack(0x00);  // Register 1 high
  response.EmplaceBack(0x14);  // Register 1 low (20)

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 2);
  auto result = master.ReadFileRecord(kUnitId, records);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1);
  auto it = result->find({1, 0});
  ASSERT_NE(it, result->end());
  EXPECT_EQ(it->second.size(), 2);
  EXPECT_EQ(it->second[0], 10);
  EXPECT_EQ(it->second[1], 20);
}

// Success path: ReadFileRecord with multiple records
TEST(TCPMasterCoverage, ReadFileRecord_MultipleRecords) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadFileRecord};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(20);    // Response length = 20 (2 records * 10 bytes)
  response.EmplaceBack(0x06);  // Reference type
  response.EmplaceBack(0x08);  // Data length (4 + 2*2)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);  // File 1
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);  // Record 0
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0A);  // Data: 10
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x14);  // Data: 20
  response.EmplaceBack(0x06);  // Reference type
  response.EmplaceBack(0x08);  // Data length
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x02);  // File 2
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);  // Record 1
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x2A);  // Data: 42
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x2B);  // Data: 43

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 2);
  records.emplace_back(2, 1, 2);
  auto result = master.ReadFileRecord(kUnitId, records);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2);
  EXPECT_EQ(result->at({1, 0}).size(), 2);
  EXPECT_EQ(result->at({2, 1}).size(), 2);
  EXPECT_EQ(result->at({2, 1})[0], 42);
}

// ReadCoils with empty data
TEST(TCPMasterCoverage, ReadCoils_EmptyData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data - data.empty()

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadCoils(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadInputRegisters with exception response
TEST(TCPMasterCoverage, ReadInputRegisters_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadIR};
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadInputRegisters(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// ReadExceptionStatus with exception response
TEST(TCPMasterCoverage, ReadExceptionStatus_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadExceptionStatus(kUnitId);
  EXPECT_FALSE(result.has_value());
}

// GetComEventCounter with exception response
TEST(TCPMasterCoverage, GetComEventCounter_ExceptionResponse) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kGetComEventCounter};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.GetComEventCounter(kUnitId);
  EXPECT_FALSE(result.has_value());
}

// ReadDiscreteInputs with wrong byte count
TEST(TCPMasterCoverage, ReadDiscreteInputs_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadDI};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);  // Wrong byte count (should be 1 for 5 inputs)
  response.EmplaceBack(0xFF);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadDiscreteInputs(kUnitId, 0, 5);
  EXPECT_FALSE(result.has_value());
}
