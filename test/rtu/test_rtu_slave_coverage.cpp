#include <array>
#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
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

// Test ProcessIncomingFrame error paths
TEST(RtuSlaveCoverage, ProcessIncomingFrame_NoData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport(0);
  RtuSlave slave{kSlaveId};

  // No data in transport
  bool processed = slave.ProcessIncomingFrame(transport, 10);  // Short timeout
  EXPECT_FALSE(processed);
}

TEST(RtuSlaveCoverage, ProcessIncomingFrame_InvalidFrame) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuSlave slave{kSlaveId};

  // Invalid frame (bad CRC)
  std::vector<uint8_t> invalid_frame{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00};
  transport.SetReadData(invalid_frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);
}

TEST(RtuSlaveCoverage, ProcessIncomingFrame_WrongSlaveID) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr uint8_t kWrongSlaveId{2};
  MemoryTransport transport;
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Request with wrong slave ID
  RtuRequest request{{kWrongSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = RtuFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);  // Should not process request for wrong slave ID
}

TEST(RtuSlaveCoverage, ProcessIncomingFrame_Timeout) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport(0);  // Empty transport
  RtuSlave slave{kSlaveId};

  bool processed = slave.ProcessIncomingFrame(transport, 10);  // Short timeout
  EXPECT_FALSE(processed);
}

TEST(RtuSlaveCoverage, Poll_NoData) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport(0);
  RtuSlave slave{kSlaveId};

  bool processed = slave.Poll(transport);
  EXPECT_FALSE(processed);
}

TEST(RtuSlaveCoverage, Poll_InvalidFrame) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuSlave slave{kSlaveId};

  // Invalid frame
  std::vector<uint8_t> invalid_frame{0x01, 0x03};
  transport.SetReadData(invalid_frame);
  transport.ResetReadPosition();

  bool processed = slave.Poll(transport);
  EXPECT_FALSE(processed);
}

// Test ProcessReadWriteMultipleRegisters error paths
TEST(RtuSlaveCoverage, ProcessReadWriteMultipleRegisters_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kReadWriteMultRegs}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessReadWriteMultipleRegisters_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kReadWriteMultRegs}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessReadWriteMultipleRegisters_DataOffsetOutOfBounds) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kReadWriteMultRegs}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessWriteFileRecord error paths
TEST(RtuSlaveCoverage, ProcessWriteFileRecord_ZeroByteCount) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(0);  // byte_count = 0

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessWriteFileRecord_InsufficientDataForByteCount) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(20);  // byte_count = 20
  // But only have 1 byte

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessWriteFileRecord_InsufficientDataForHeader) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(10);    // byte_count = 10
  data.push_back(0x06);  // reference_type
  // Only 2 bytes, need 7 for header

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessWriteFileRecord_InvalidReferenceType) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kWriteFileRecord}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessWriteFileRecord_InsufficientDataForRecord) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kWriteFileRecord}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessReadFIFOQueue error paths
TEST(RtuSlaveCoverage, ProcessReadFIFOQueue_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // Only 1 byte, need 2 for address

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessReadFIFOQueue_EmptyQueue) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  request.SetReadFIFOQueueData(100);  // Address with no FIFO queue

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessWriteMultipleRegisters error paths
TEST(RtuSlaveCoverage, ProcessWriteMultipleRegisters_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultRegs}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessWriteMultipleRegisters_DataOffsetOutOfBounds) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultRegs}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessWriteMultipleRegisters_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultRegs}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessWriteMultipleCoils error paths
TEST(RtuSlaveCoverage, ProcessWriteMultipleCoils_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultCoils}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // address high
  data.push_back(0x00);  // address low
  data.push_back(0x00);  // count high
  data.push_back(0x08);  // count low (8 coils)
  data.push_back(0x01);  // byte_count (correct)
  // Missing coil data

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessWriteMultipleCoils_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultCoils}};
  std::vector<uint8_t> data;
  data.push_back(0x00);  // address high
  data.push_back(0x00);  // address low
  data.push_back(0x00);  // count high
  data.push_back(0x08);  // count low (8 coils)
  data.push_back(0x02);  // Wrong byte_count (should be 1)
  data.push_back(0xFF);  // coil data

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Test ProcessReadFileRecord error paths
TEST(RtuSlaveCoverage, ProcessReadFileRecord_EmptyData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;  // Empty data
  request.SetRawData(data);

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessReadFileRecord_ZeroByteCount) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(0);  // byte_count = 0

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessReadFileRecord_InsufficientDataForByteCount) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(10);  // byte_count = 10, but only have 1 byte

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessReadFileRecord_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(5);     // byte_count = 5 (need 6 for one record)
  data.push_back(0x00);  // file_number high
  data.push_back(0x01);  // file_number low
  data.push_back(0x00);  // record_number high
  data.push_back(0x00);  // record_number low
  // Missing record_length

  request.SetRawData(data);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

TEST(RtuSlaveCoverage, ProcessReadFileRecord_FileNotFound) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(99, 0, 10);  // File 99 doesn't exist

  request.SetReadFileRecordData(records);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(RtuSlaveCoverage, ProcessReadFileRecord_RecordNotFound) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  // Create file with one record using Process
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> write_records;
  std::vector<int16_t> record_data{100, 200};
  write_records.emplace_back(1, 0, record_data);
  write_req.SetWriteFileRecordData(write_records);
  slave.Process(write_req);  // Create the file record

  // Try to read non-existent record
  RtuRequest request{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 99, 10);  // Record 99 doesn't exist

  request.SetReadFileRecordData(records);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(RtuSlaveCoverage, ProcessReadFileRecord_RecordTooSmall) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  // Create file with small record using Process
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> write_records;
  std::vector<int16_t> record_data{100};  // Only 1 value
  write_records.emplace_back(1, 0, record_data);
  write_req.SetWriteFileRecordData(write_records);
  slave.Process(write_req);  // Create the file record

  // Try to read more than available
  RtuRequest request{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);  // Request 10 values, but only 1 available

  request.SetReadFileRecordData(records);
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessReadWriteMultipleRegisters write address out of bounds
TEST(RtuSlaveCoverage, ProcessReadWriteMultipleRegisters_WriteAddressOutOfBounds) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 5});  // Only addresses 0-4

  RtuRequest request{{kSlaveId, FunctionCode::kReadWriteMultRegs}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test ProcessReadWriteMultipleRegisters read address out of bounds
TEST(RtuSlaveCoverage, ProcessReadWriteMultipleRegisters_ReadAddressOutOfBounds) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 5});  // Only addresses 0-4

  RtuRequest request{{kSlaveId, FunctionCode::kReadWriteMultRegs}};
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
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Test SetId
TEST(RtuSlaveCoverage, SetId) {
  static constexpr uint8_t kSlaveId1{1};
  static constexpr uint8_t kSlaveId2{2};
  RtuSlave slave{kSlaveId1};
  EXPECT_EQ(slave.GetId(), kSlaveId1);

  slave.SetId(kSlaveId2);
  EXPECT_EQ(slave.GetId(), kSlaveId2);
}

// Test communication event log max size behavior
TEST(RtuSlaveCoverage, ComEventLog_MaxSize) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Process many requests to fill up event log (max 64 entries)
  for (int i = 0; i < 70; ++i) {
    RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
    request.SetAddressSpan({0, 1});
    slave.Process(request);
  }

  // Should still work (oldest entries removed)
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 1});
  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
}

// Test ProcessIncomingFrame with valid request
TEST(RtuSlaveCoverage, ProcessIncomingFrame_ValidRequest) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = RtuFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_TRUE(processed);

  // Check that response was written
  auto written = transport.GetWrittenData();
  EXPECT_GT(written.size(), 0);
}

// Test Poll with valid request
TEST(RtuSlaveCoverage, Poll_ValidRequest) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = RtuFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.Poll(transport);
  EXPECT_TRUE(processed);
}

// Test ProcessWriteSingleRegister with insufficient data
TEST(RtuSlaveCoverage, ProcessWriteSingleRegister_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kWriteSingleReg}};
  std::vector<uint8_t> data{0x00, 0x00, 0x00};  // Only 3 bytes, need 4
  request.SetRawData(data);

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalFunction);
}

// Test ProcessWriteSingleCoil with insufficient data
TEST(RtuSlaveCoverage, ProcessWriteSingleCoil_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kWriteSingleCoil}};
  std::vector<uint8_t> data{0x00, 0x00, 0x00};  // Only 3 bytes, need 4
  request.SetRawData(data);

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalFunction);
}

// Test ProcessDiagnostics with insufficient data
TEST(RtuSlaveCoverage, ProcessDiagnostics_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kDiagnostics}};
  std::vector<uint8_t> data{0x00};  // Only 1 byte, need at least 2
  request.SetRawData(data);

  RtuResponse response = slave.Process(request);
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

TEST(RtuSlaveCoverage, ProcessIncomingFrame_WriteFailure) {
  static constexpr uint8_t kSlaveId{1};
  FailingWriteTransport transport(true);  // Fail write
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = RtuFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);  // Should fail due to write failure
}

// Test ProcessIncomingFrame with broadcast read (should be rejected)
TEST(RtuSlaveCoverage, ProcessIncomingFrame_BroadcastRead) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Broadcast read request (should be rejected)
  RtuRequest request{{0, FunctionCode::kReadHR}};  // Slave ID 0 = broadcast
  request.SetAddressSpan({0, 5});
  auto frame = RtuFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);  // Broadcast reads are not allowed
}

// Test ProcessIncomingFrame with broadcast write (should process but not respond)
TEST(RtuSlaveCoverage, ProcessIncomingFrame_BroadcastWrite) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Broadcast write request (should process but not send response)
  RtuRequest request{{0, FunctionCode::kWriteSingleReg}};  // Slave ID 0 = broadcast
  request.SetWriteSingleRegisterData(0, 1234);
  auto frame = RtuFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_TRUE(processed);  // Should process broadcast write

  // Check that no response was written (broadcast doesn't get responses)
  auto written = transport.GetWrittenData();
  EXPECT_EQ(written.size(), 0);

  // Verify the write actually happened
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = read_resp.GetData();
  ASSERT_GE(data.size(), 3);
  int16_t value = supermb::MakeInt16(data[2], data[1]);
  EXPECT_EQ(value, 1234);
}

// Float range: AddFloatRange, SetFloat, ReadHR from float range, WriteMultRegs to float range
TEST(RtuSlaveCoverage, AddFloatRange_SetFloat_ReadHoldingRegistersFromFloatRange) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddFloatRange(0, 4);  // registers 0..3 = 2 floats
  ASSERT_TRUE(slave.SetFloat(0, 1.0f));
  ASSERT_TRUE(slave.SetFloat(1, 2.0f));

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 4});
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_GE(data.size(), 9u);  // byte_count + 8 bytes
  EXPECT_EQ(data[0], 8);
  float f0 = supermb::DecodeFloat(&data[1], supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst);
  float f1 = supermb::DecodeFloat(&data[5], supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst);
  EXPECT_FLOAT_EQ(f0, 1.0f);
  EXPECT_FLOAT_EQ(f1, 2.0f);
}

TEST(RtuSlaveCoverage, AddFloatRange_WriteMultipleRegistersToFloatRange) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddFloatRange(0, 4);
  slave.SetFloat(0, 0.0f);
  slave.SetFloat(1, 0.0f);

  uint8_t buf[8];
  supermb::EncodeFloat(1.5f, supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst, buf);
  supermb::EncodeFloat(2.5f, supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst, buf + 4);
  std::vector<int16_t> regs(4);
  regs[0] = static_cast<int16_t>(supermb::DecodeU16(buf[0], buf[1], supermb::ByteOrder::BigEndian));
  regs[1] = static_cast<int16_t>(supermb::DecodeU16(buf[2], buf[3], supermb::ByteOrder::BigEndian));
  regs[2] = static_cast<int16_t>(supermb::DecodeU16(buf[4], buf[5], supermb::ByteOrder::BigEndian));
  regs[3] = static_cast<int16_t>(supermb::DecodeU16(buf[6], buf[7], supermb::ByteOrder::BigEndian));

  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultRegs}};
  request.SetWriteMultipleRegistersData(0, 4, regs);

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 4});
  RtuResponse read_resp = slave.Process(read_req);
  ASSERT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = read_resp.GetData();
  ASSERT_GE(data.size(), 9u);
  EXPECT_FLOAT_EQ(supermb::DecodeFloat(&data[1], supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst),
                  1.5f);
  EXPECT_FLOAT_EQ(supermb::DecodeFloat(&data[5], supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst),
                  2.5f);
}

TEST(RtuSlaveCoverage, WriteSingleRegister_AddressInFloatRange_IllegalDataAddress) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddFloatRange(0, 4);

  RtuRequest request{{kSlaveId, FunctionCode::kWriteSingleReg}};
  request.SetWriteSingleRegisterData(0, 0x1234);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(RtuSlaveCoverage, SetFloat_OutOfRange) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddFloatRange(0, 4);  // 2 floats, indices 0 and 1
  EXPECT_FALSE(slave.SetFloat(2, 1.0f));
  EXPECT_TRUE(slave.SetFloat(1, 1.0f));
}

// Read coils from address not in map -> IllegalDataAddress
TEST(RtuSlaveCoverage, ProcessReadCoils_IllegalDataAddress) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 8});  // Coils at 0-7 only

  RtuRequest request{{kSlaveId, FunctionCode::kReadCoils}};
  request.SetAddressSpan({10, 5});  // Address 10 not in map

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Write multiple registers overlapping float range (not entirely within) -> IllegalDataAddress
TEST(RtuSlaveCoverage, ProcessWriteMultipleRegisters_FloatRangeOverlap) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddFloatRange(10, 4);          // Float range 10-13
  slave.AddHoldingRegisters({0, 20});  // Normal regs 0-19

  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> vals{0x11, 0x22, 0x33, 0x44};  // 4 registers
  request.SetWriteMultipleRegistersData(8, 4, vals);  // Write 8-11, overlaps float 10-13

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Write multiple coils to address not in map -> IllegalDataAddress
TEST(RtuSlaveCoverage, ProcessWriteMultipleCoils_AddressOutOfBounds) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 8});  // Coils 0-7 only

  RtuRequest request{{kSlaveId, FunctionCode::kWriteMultCoils}};
  const std::array<bool, 8> coils{true, false, true, false, true, false, true, false};
  ASSERT_TRUE(request.SetWriteMultipleCoilsData(10, 8, coils));  // Address 10 not in map

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// Read file record: byte_count=5, 5 payload bytes -> data_offset(1)+6 > size(6)
TEST(RtuSlaveCoverage, ProcessReadFileRecord_InsufficientDataForOneRecord) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};

  RtuRequest request{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> data;
  data.push_back(5);  // byte_count = 5 (need 6 bytes per record for file_num+rec_num+rec_len)
  data.push_back(0);
  data.push_back(1);
  data.push_back(0);
  data.push_back(0);
  data.push_back(0);  // Only 5 bytes after byte_count; first record needs 6
  request.SetRawData(data);

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataValue);
}

// Mask write to address not in map -> IllegalDataAddress
TEST(RtuSlaveCoverage, ProcessMaskWriteRegister_AddressOutOfBounds) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});  // Registers 0-9 only

  RtuRequest request{{kSlaveId, FunctionCode::kMaskWriteReg}};
  ASSERT_TRUE(request.SetMaskWriteRegisterData(20, 0xFFFF, 0x0000));

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

// ReadWriteMultipleRegisters with both read and write in float range
TEST(RtuSlaveCoverage, ProcessReadWriteMultipleRegisters_FloatRangeWriteAndRead) {
  static constexpr uint8_t kSlaveId{1};
  RtuSlave slave{kSlaveId};
  slave.AddFloatRange(0, 4);  // Registers 0-3 = 2 floats
  slave.SetFloat(0, 1.0f);
  slave.SetFloat(1, 2.0f);

  uint8_t write_buf[8];
  supermb::EncodeFloat(3.0f, supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst, write_buf);
  supermb::EncodeFloat(4.0f, supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst, write_buf + 4);

  std::vector<uint8_t> data;
  data.push_back(0x00);
  data.push_back(0x00);  // read_start = 0
  data.push_back(0x00);
  data.push_back(0x04);  // read_count = 4
  data.push_back(0x00);
  data.push_back(0x00);  // write_start = 0
  data.push_back(0x00);
  data.push_back(0x04);  // write_count = 4
  data.push_back(0x08);  // write_byte_count = 8
  data.insert(data.end(), write_buf, write_buf + 8);

  RtuRequest request{{kSlaveId, FunctionCode::kReadWriteMultRegs}};
  request.SetRawData(data);

  RtuResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto resp_data = response.GetData();
  ASSERT_GE(resp_data.size(), 8u);  // 8 bytes (2 floats); float path does not prepend byte_count
  float r0 = supermb::DecodeFloat(&resp_data[0], supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst);
  float r1 = supermb::DecodeFloat(&resp_data[4], supermb::ByteOrder::BigEndian, supermb::WordOrder::HighWordFirst);
  EXPECT_FLOAT_EQ(r0, 3.0f);
  EXPECT_FLOAT_EQ(r1, 4.0f);
}

// Com event log fill via ProcessIncomingFrame to cover erase path in ProcessIncomingFrame
TEST(RtuSlaveCoverage, ComEventLog_MaxSize_ViaProcessIncomingFrame) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  for (int i = 0; i < 66; ++i) {
    RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
    request.SetAddressSpan({0, 1});
    auto frame = RtuFrame::EncodeRequest(request);
    transport.SetReadData(frame);
    transport.ResetReadPosition();
    bool ok = slave.ProcessIncomingFrame(transport, 100);
    EXPECT_TRUE(ok);
  }
}
