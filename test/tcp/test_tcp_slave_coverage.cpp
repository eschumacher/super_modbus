#include <array>
#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/tcp/tcp_frame.hpp"
#include "super_modbus/tcp/tcp_request.hpp"
#include "super_modbus/tcp/tcp_response.hpp"
#include "super_modbus/tcp/tcp_slave.hpp"
#include "super_modbus/transport/memory_transport.hpp"

using supermb::AddressSpan;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::MemoryTransport;
using supermb::TcpFrame;
using supermb::TcpRequest;
using supermb::TcpResponse;
using supermb::TcpSlave;

// Test ProcessIncomingFrame error paths
TEST(TCPSlaveCoverage, ProcessIncomingFrame_NoData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport(0);
  TcpSlave slave{kUnitId};

  // No data in transport
  bool processed = slave.ProcessIncomingFrame(transport, 10);  // Short timeout
  EXPECT_FALSE(processed);
}

TEST(TCPSlaveCoverage, ProcessIncomingFrame_InvalidFrame) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};

  // Invalid frame (too short)
  std::vector<uint8_t> invalid_frame{0x00, 0x01, 0x00, 0x00};
  transport.SetReadData(invalid_frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);
}

TEST(TCPSlaveCoverage, ProcessIncomingFrame_WrongUnitId) {
  static constexpr uint8_t kUnitId{1};
  static constexpr uint8_t kWrongUnitId{2};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  // Request with wrong unit ID
  TcpRequest request{{1, kWrongUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);  // Should not process request for wrong unit ID
}

TEST(TCPSlaveCoverage, ProcessIncomingFrame_Timeout) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport(0);  // Empty transport
  TcpSlave slave{kUnitId};

  bool processed = slave.ProcessIncomingFrame(transport, 10);  // Short timeout
  EXPECT_FALSE(processed);
}

TEST(TCPSlaveCoverage, Poll_NoData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport(0);
  TcpSlave slave{kUnitId};

  bool processed = slave.Poll(transport);
  EXPECT_FALSE(processed);
}

TEST(TCPSlaveCoverage, Poll_InvalidFrame) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};

  // Invalid frame
  std::vector<uint8_t> invalid_frame{0x00, 0x01, 0x00, 0x00};
  transport.SetReadData(invalid_frame);
  transport.ResetReadPosition();

  bool processed = slave.Poll(transport);
  EXPECT_FALSE(processed);
}

// Test ProcessReadWriteMultipleRegisters error paths
TEST(TCPSlaveCoverage, ProcessReadWriteMultipleRegisters_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadWriteMultRegs}};
  // Set data with wrong byte count
  std::vector<uint8_t> data;
  // read_start(2) + read_count(2) + write_start(2) + write_count(2) + byte_count(1) + values(2)
  data.push_back(0x00);  // read_start high
  data.push_back(0x00);  // read_start low
  data.push_back(0x00);  // read_count high
  data.push_back(0x01);  // read_count low (1)
  data.push_back(0x00);  // write_start high
  data.push_back(0x00);  // write_start low
  data.push_back(0x00);  // write_count high
  data.push_back(0x01);  // write_count low (1)
  data.push_back(0x01);  // Wrong byte_count (should be 2 for 1 register)
  data.push_back(0x00);  // value high
  data.push_back(0x01);  // value low

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessReadWriteMultipleRegisters_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadWriteMultRegs}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // read_start high
  data.push_back(0x00);  // read_start low
  data.push_back(0x00);  // read_count high
  data.push_back(0x01);  // read_count low
  data.push_back(0x00);  // write_start high
  data.push_back(0x00);  // write_start low
  data.push_back(0x00);  // write_count high
  data.push_back(0x01);  // write_count low
  data.push_back(0x02);  // byte_count
  data.push_back(0x00);  // Only 1 byte of value, need 2

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessReadWriteMultipleRegisters_DataOffsetOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadWriteMultRegs}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // read_start high
  data.push_back(0x00);  // read_start low
  data.push_back(0x00);  // read_count high
  data.push_back(0x01);  // read_count low
  data.push_back(0x00);  // write_start high
  data.push_back(0x00);  // write_start low
  data.push_back(0x00);  // write_count high
  data.push_back(0x01);  // write_count low
  data.push_back(0x02);  // byte_count
  data.push_back(0x00);  // value high
  // Missing value low - data_offset + 1 will be >= data.size()

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessWriteFileRecord error paths
TEST(TCPSlaveCoverage, ProcessWriteFileRecord_ZeroByteCount) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(0);  // byte_count = 0

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessWriteFileRecord_InsufficientDataForByteCount) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(20);  // byte_count = 20
  // But only have 1 byte

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessWriteFileRecord_InsufficientDataForHeader) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(10);    // byte_count = 10
  data.push_back(0x06);  // reference_type
  // Only 2 bytes, need 7 for header

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessWriteFileRecord_InvalidReferenceType) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(7);     // byte_count = 7
  data.push_back(0x05);  // Invalid reference type (should be 0x06)
  data.push_back(0x00);  // file_number high
  data.push_back(0x01);  // file_number low
  data.push_back(0x00);  // record_number high
  data.push_back(0x00);  // record_number low
  data.push_back(0x00);  // record_length high
  data.push_back(0x00);  // record_length low

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessWriteFileRecord_InsufficientDataForRecord) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(9);     // byte_count = 9
  data.push_back(0x06);  // reference_type
  data.push_back(0x00);  // file_number high
  data.push_back(0x01);  // file_number low
  data.push_back(0x00);  // record_number high
  data.push_back(0x00);  // record_number low
  data.push_back(0x00);  // record_length high
  data.push_back(0x01);  // record_length low (1 register = 2 bytes)
  // Missing record data (need 2 more bytes)

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessReadFIFOQueue error paths
TEST(TCPSlaveCoverage, ProcessReadFIFOQueue_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadFIFOQueue}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // Only 1 byte, need 2 for address

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessReadFIFOQueue_EmptyQueue) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadFIFOQueue}};
  request.SetReadFIFOQueueData(100);  // Address with no FIFO queue

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessWriteMultipleRegisters error paths
TEST(TCPSlaveCoverage, ProcessWriteMultipleRegisters_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteMultRegs}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // address high
  data.push_back(0x00);  // address low
  data.push_back(0x00);  // count high
  data.push_back(0x02);  // count low (2 registers)
  data.push_back(0x04);  // byte_count (correct)
  data.push_back(0x00);  // value 1 high
  data.push_back(0x01);  // value 1 low
  // Missing value 2

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessWriteMultipleRegisters_DataOffsetOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteMultRegs}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // address high
  data.push_back(0x00);  // address low
  data.push_back(0x00);  // count high
  data.push_back(0x02);  // count low (2 registers)
  data.push_back(0x04);  // byte_count (correct)
  data.push_back(0x00);  // value 1 high
  data.push_back(0x01);  // value 1 low
  data.push_back(0x00);  // value 2 high
  // Missing value 2 low - data_offset + 1 will be >= data.size()

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessWriteMultipleRegisters_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteMultRegs}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // address high
  data.push_back(0x00);  // address low
  data.push_back(0x00);  // count high
  data.push_back(0x02);  // count low (2 registers)
  data.push_back(0x02);  // Wrong byte_count (should be 4)
  data.push_back(0x00);  // value 1 high
  data.push_back(0x01);  // value 1 low
  data.push_back(0x00);  // value 2 high
  data.push_back(0x02);  // value 2 low

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessWriteMultipleCoils error paths
TEST(TCPSlaveCoverage, ProcessWriteMultipleCoils_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddCoils({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteMultCoils}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // address high
  data.push_back(0x00);  // address low
  data.push_back(0x00);  // count high
  data.push_back(0x08);  // count low (8 coils)
  data.push_back(0x01);  // byte_count (correct)
  // Missing coil data

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessWriteMultipleCoils_WrongByteCount) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddCoils({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteMultCoils}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // address high
  data.push_back(0x00);  // address low
  data.push_back(0x00);  // count high
  data.push_back(0x08);  // count low (8 coils)
  data.push_back(0x02);  // Wrong byte_count (should be 1)
  data.push_back(0xFF);  // coil data

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessReadFileRecord error paths
TEST(TCPSlaveCoverage, ProcessReadFileRecord_EmptyData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;  // Empty data
  request.SetRawData(data);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessReadFileRecord_ZeroByteCount) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(0);  // byte_count = 0

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessReadFileRecord_InsufficientDataForByteCount) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(10);  // byte_count = 10, but only have 1 byte

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessReadFileRecord_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(5);     // byte_count = 5 (need 6 for one record)
  data.push_back(0x00);  // file_number high
  data.push_back(0x01);  // file_number low
  data.push_back(0x00);  // record_number high
  data.push_back(0x00);  // record_number low
  // Missing record_length

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(TCPSlaveCoverage, ProcessReadFileRecord_FileNotFound) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(99, 0, 10);  // File 99 doesn't exist

  request.SetReadFileRecordData(records);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(TCPSlaveCoverage, ProcessReadFileRecord_RecordNotFound) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  // Create file with one record using Process
  TcpRequest write_req{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> write_records;
  std::vector<int16_t> record_data{100, 200};
  write_records.emplace_back(1, 0, record_data);
  write_req.SetWriteFileRecordData(write_records);
  slave.Process(write_req);  // Create the file record

  // Try to read non-existent record
  TcpRequest request{{1, kUnitId, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 99, 10);  // Record 99 doesn't exist

  request.SetReadFileRecordData(records);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(TCPSlaveCoverage, ProcessReadFileRecord_RecordTooSmall) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  // Create file with small record using Process
  TcpRequest write_req{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> write_records;
  std::vector<int16_t> record_data{100};  // Only 1 value
  write_records.emplace_back(1, 0, record_data);
  write_req.SetWriteFileRecordData(write_records);
  slave.Process(write_req);  // Create the file record

  // Try to read more than available
  TcpRequest request{{1, kUnitId, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);  // Request 10 values, but only 1 available

  request.SetReadFileRecordData(records);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessReadWriteMultipleRegisters write address out of bounds
TEST(TCPSlaveCoverage, ProcessReadWriteMultipleRegisters_WriteAddressOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 5});  // Only addresses 0-4

  TcpRequest request{{1, kUnitId, FunctionCode::kReadWriteMultRegs}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // read_start high
  data.push_back(0x00);  // read_start low
  data.push_back(0x00);  // read_count high
  data.push_back(0x01);  // read_count low
  data.push_back(0x00);  // write_start high
  data.push_back(0x05);  // write_start low (address 5, out of bounds)
  data.push_back(0x00);  // write_count high
  data.push_back(0x01);  // write_count low
  data.push_back(0x02);  // byte_count
  data.push_back(0x00);  // value high
  data.push_back(0x01);  // value low

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessReadWriteMultipleRegisters read address out of bounds
TEST(TCPSlaveCoverage, ProcessReadWriteMultipleRegisters_ReadAddressOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 5});  // Only addresses 0-4

  TcpRequest request{{1, kUnitId, FunctionCode::kReadWriteMultRegs}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // read_start high
  data.push_back(0x05);  // read_start low (address 5, out of bounds)
  data.push_back(0x00);  // read_count high
  data.push_back(0x01);  // read_count low
  data.push_back(0x00);  // write_start high
  data.push_back(0x00);  // write_start low
  data.push_back(0x00);  // write_count high
  data.push_back(0x01);  // write_count low
  data.push_back(0x02);  // byte_count
  data.push_back(0x00);  // value high
  data.push_back(0x01);  // value low

  request.SetRawData(data);
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test SetId
TEST(TCPSlaveCoverage, SetId) {
  static constexpr uint8_t kUnitId1{1};
  static constexpr uint8_t kUnitId2{2};
  TcpSlave slave{kUnitId1};
  EXPECT_EQ(slave.GetId(), kUnitId1);

  slave.SetId(kUnitId2);
  EXPECT_EQ(slave.GetId(), kUnitId2);
}

// Test communication event log max size behavior
TEST(TCPSlaveCoverage, ComEventLog_MaxSize) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  // Process many requests to fill up event log (max 64 entries)
  for (int i = 0; i < 70; ++i) {
    TcpRequest request{{static_cast<uint16_t>(i), kUnitId, FunctionCode::kReadHR}};
    request.SetAddressSpan({0, 1});
    slave.Process(request);
  }

  // Should still work (oldest entries removed)
  TcpRequest request{{100, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 1});
  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
}

// Test ProcessIncomingFrame with valid request
TEST(TCPSlaveCoverage, ProcessIncomingFrame_ValidRequest) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_TRUE(processed);

  // Check that response was written
  auto written = transport.GetWrittenData();
  EXPECT_GT(written.size(), 0);
}

// Test Poll with valid request
TEST(TCPSlaveCoverage, Poll_ValidRequest) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.Poll(transport);
  EXPECT_TRUE(processed);
}

// Test ProcessWriteSingleRegister with insufficient data
TEST(TCPSlaveCoverage, ProcessWriteSingleRegister_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteSingleReg}};
  std::vector<uint8_t> data{0x00, 0x00, 0x00};  // Only 3 bytes, need 4
  request.SetRawData(data);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalFunction);
}

// Test ProcessWriteSingleCoil with insufficient data
TEST(TCPSlaveCoverage, ProcessWriteSingleCoil_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddCoils({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteSingleCoil}};
  std::vector<uint8_t> data{0x00, 0x00, 0x00};  // Only 3 bytes, need 4
  request.SetRawData(data);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalFunction);
}

// Test ProcessDiagnostics with insufficient data
TEST(TCPSlaveCoverage, ProcessDiagnostics_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kDiagnostics}};
  std::vector<uint8_t> data{0x00};  // Only 1 byte, need at least 2
  request.SetRawData(data);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessIncomingFrame with write failure
class FailingWriteTransport : public supermb::ByteTransport {
 public:
  explicit FailingWriteTransport(bool fail_write = false)
      : fail_write_(fail_write) {}

  int Read(std::span<uint8_t> buffer) override { return transport_.Read(buffer); }

  int Write(std::span<const uint8_t> data) override {
    if (fail_write_) {
      return 0;  // Simulate write failure
    }
    return transport_.Write(data);
  }

  bool Flush() override { return transport_.Flush(); }

  bool HasData() const override { return transport_.HasData(); }

  size_t AvailableBytes() const override { return transport_.AvailableBytes(); }

  void SetReadData(std::span<const uint8_t> data) { transport_.SetReadData(data); }
  void ResetReadPosition() { transport_.ResetReadPosition(); }

 private:
  MemoryTransport transport_;
  bool fail_write_;
};

TEST(TCPSlaveCoverage, ProcessIncomingFrame_WriteFailure) {
  static constexpr uint8_t kUnitId{1};
  FailingWriteTransport transport(true);  // Fail write
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);  // Write failure should cause ProcessIncomingFrame to fail
}

// Test ProcessReadRegisters with address out of bounds
TEST(TCPSlaveCoverage, ProcessReadRegisters_AddressOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 5});  // Only addresses 0-4

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({5, 1});  // Address 5 is out of bounds

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessReadCoils with address out of bounds
TEST(TCPSlaveCoverage, ProcessReadCoils_AddressOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddCoils({0, 5});  // Only addresses 0-4

  TcpRequest request{{1, kUnitId, FunctionCode::kReadCoils}};
  request.SetAddressSpan({5, 1});  // Address 5 is out of bounds

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessMaskWriteRegister with address out of bounds
TEST(TCPSlaveCoverage, ProcessMaskWriteRegister_AddressOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 5});  // Only addresses 0-4

  TcpRequest request{{1, kUnitId, FunctionCode::kMaskWriteReg}};
  request.SetMaskWriteRegisterData(5, 0xFF00, 0x00FF);  // Address 5 is out of bounds

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessMaskWriteRegister with insufficient data
TEST(TCPSlaveCoverage, ProcessMaskWriteRegister_InsufficientData) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kMaskWriteReg}};
  std::vector<uint8_t> data{0x00, 0x00, 0x00, 0x00, 0x00};  // Only 5 bytes, need 6
  request.SetRawData(data);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test SetFIFOQueue
TEST(TCPSlaveCoverage, SetFIFOQueue) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  std::vector<int16_t> fifo_data{100, 200, 300};
  slave.SetFIFOQueue(0x1234, fifo_data);

  TcpRequest request{{1, kUnitId, FunctionCode::kReadFIFOQueue}};
  request.SetReadFIFOQueueData(0x1234);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GT(response.GetData().size(), 0);
}

// Test ProcessReadRegisters with partial address range out of bounds
TEST(TCPSlaveCoverage, ProcessReadRegisters_PartialRangeOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 5});  // Only addresses 0-4

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({3, 3});  // Addresses 3, 4, 5 - address 5 is out of bounds

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessWriteMultipleRegisters with address out of bounds
TEST(TCPSlaveCoverage, ProcessWriteMultipleRegisters_AddressOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 5});  // Only addresses 0-4

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values{100, 200};
  request.SetWriteMultipleRegistersData(4, 2, values);  // Addresses 4, 5 - address 5 is out of bounds

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessWriteMultipleCoils with address out of bounds
TEST(TCPSlaveCoverage, ProcessWriteMultipleCoils_AddressOutOfBounds) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddCoils({0, 5});  // Only addresses 0-4

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteMultCoils}};
  std::array<bool, 2> values{true, false};
  request.SetWriteMultipleCoilsData(4, 2,
                                    std::span<const bool>(values));  // Addresses 4, 5 - address 5 is out of bounds

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Transport that simulates partial reads for slave tests
class PartialReadTransportSlave : public supermb::ByteTransport {
 public:
  explicit PartialReadTransportSlave(size_t max_read_size = 1)
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

// Test slave ReadFrame with partial reads
TEST(TCPSlaveCoverage, ReadFrame_PartialReads) {
  static constexpr uint8_t kUnitId{1};
  PartialReadTransportSlave transport(2);  // Read max 2 bytes at a time
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 1000);
  EXPECT_TRUE(processed);
}

// Test slave ReadFrame with zero timeout (should work if data is immediately available)
TEST(TCPSlaveCoverage, ReadFrame_ZeroTimeout) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Zero timeout should work if data is available immediately
  bool processed = slave.ProcessIncomingFrame(transport, 0);
  EXPECT_TRUE(processed);
}

// Test slave ReadFrame with very large frame
TEST(TCPSlaveCoverage, ReadFrame_LargeFrame) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 200});  // Large address span

  // Request to read many registers
  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 100});
  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 1000);
  EXPECT_TRUE(processed);
}

// Test ReadFrame with incomplete frame that times out during body read
TEST(TCPSlaveCoverage, ReadFrame_TimeoutDuringBodyRead) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};

  // MBAP header says length is 10, but we only provide partial data
  std::vector<uint8_t> partial_frame;
  partial_frame.push_back(0x00);  // Transaction ID high
  partial_frame.push_back(0x01);  // Transaction ID low
  partial_frame.push_back(0x00);  // Protocol ID high
  partial_frame.push_back(0x00);  // Protocol ID low
  partial_frame.push_back(0x00);  // Length high
  partial_frame.push_back(0x0A);  // Length low (10 bytes)
  partial_frame.push_back(0x01);  // Unit ID
  partial_frame.push_back(0x03);  // Function code
  // Missing remaining data (length says 10, but we only have 2 so far)

  transport.SetReadData(partial_frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 10);  // Short timeout
  EXPECT_FALSE(processed);                                     // Should timeout waiting for remaining data
}

// Test Process with illegal function code (default case)
TEST(TCPSlaveCoverage, Process_IllegalFunction) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, static_cast<FunctionCode>(0xFF)}};  // Unknown function code
  request.SetRawData(std::vector<uint8_t>{});

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalFunction);
}

// Test ProcessDiagnostics success path
TEST(TCPSlaveCoverage, ProcessDiagnostics_Success) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kDiagnostics}};
  std::vector<uint8_t> data{0x00, 0x01, 0x12, 0x34};  // sub_func + echo data
  request.SetRawData(data);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GE(response.GetData().size(), 2);
}

// Test ProcessGetComEventCounter success path
TEST(TCPSlaveCoverage, ProcessGetComEventCounter_Success) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 1});  // Trigger at least one message for counter

  TcpRequest req{{1, kUnitId, FunctionCode::kReadHR}};
  req.SetAddressSpan({0, 1});
  slave.Process(req);  // Increment counter

  TcpRequest request{{2, kUnitId, FunctionCode::kGetComEventCounter}};
  request.SetRawData(std::vector<uint8_t>{});

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GE(response.GetData().size(), 3);
}

// Test ProcessGetComEventLog success path
TEST(TCPSlaveCoverage, ProcessGetComEventLog_Success) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 1});

  TcpRequest req{{1, kUnitId, FunctionCode::kReadHR}};
  req.SetAddressSpan({0, 1});
  slave.Process(req);

  TcpRequest request{{2, kUnitId, FunctionCode::kGetComEventLog}};
  request.SetRawData(std::vector<uint8_t>{});

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GE(response.GetData().size(), 2);
}

// Test ProcessReportSlaveID success path
TEST(TCPSlaveCoverage, ProcessReportSlaveID_Success) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReportSlaveID}};
  request.SetRawData(std::vector<uint8_t>{});

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GE(response.GetData().size(), 2);
}

// Test ProcessWriteFileRecord success path
TEST(TCPSlaveCoverage, ProcessWriteFileRecord_Success) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> records;
  records.emplace_back(1, 0, std::vector<int16_t>{100, 200});
  request.SetWriteFileRecordData(records);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GT(response.GetData().size(), 0);
}

// Test ProcessReadFileRecord success path (read after write)
TEST(TCPSlaveCoverage, ProcessReadFileRecord_Success) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  // Create file 1, record 0 with data via WriteFileRecord
  TcpRequest write_req{{1, kUnitId, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> write_records;
  write_records.emplace_back(1, 0, std::vector<int16_t>{0x0D, 0xFE, 0x00, 0x20});
  write_req.SetWriteFileRecordData(write_records);
  TcpResponse write_resp = slave.Process(write_req);
  ASSERT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read file 1, record 0, length 4 (4 registers)
  TcpRequest read_req{{2, kUnitId, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> read_records;
  read_records.emplace_back(1, 0, 4);
  read_req.SetReadFileRecordData(read_records);

  TcpResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GT(read_resp.GetData().size(), 0);
}
