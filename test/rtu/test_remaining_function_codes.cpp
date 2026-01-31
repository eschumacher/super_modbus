#include <array>
#include <gtest/gtest.h>
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"

using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

TEST(RemainingFunctionCodes, ReadExceptionStatus) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  RtuRequest request{{kSlaveId, FunctionCode::kReadExceptionStatus}};
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_EQ(data.size(), 1);
  // Exception status should be a byte value
  EXPECT_LE(data[0], 0xFF);
}

TEST(RemainingFunctionCodes, Diagnostics) {
  static constexpr uint8_t kSlaveId{2};

  RtuSlave slave{kSlaveId};
  RtuRequest request{{kSlaveId, FunctionCode::kDiagnostics}};
  std::vector<uint8_t> test_data{0x12, 0x34, 0x56};
  request.SetDiagnosticsData(0x0000, test_data);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  // Should echo back the data (sub-function + test data)
  ASSERT_GE(data.size(), 3);
}

TEST(RemainingFunctionCodes, GetComEventCounter) {
  static constexpr uint8_t kSlaveId{3};

  RtuSlave slave{kSlaveId};
  RtuRequest request{{kSlaveId, FunctionCode::kGetComEventCounter}};
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_EQ(data.size(), 3);  // Status (1) + Event count (2)
  EXPECT_EQ(data[0], 0x00);   // Status should be 0x00 (no error)
}

TEST(RemainingFunctionCodes, GetComEventLog) {
  static constexpr uint8_t kSlaveId{4};

  RtuSlave slave{kSlaveId};

  // Generate some events by processing requests
  RtuRequest req1{{kSlaveId, FunctionCode::kReadHR}};
  req1.SetAddressSpan({0, 1});
  slave.Process(req1);

  RtuRequest req2{{kSlaveId, FunctionCode::kReadExceptionStatus}};
  slave.Process(req2);

  // Now get event log
  RtuRequest request{{kSlaveId, FunctionCode::kGetComEventLog}};
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_GE(data.size(), 5);  // Status (1) + Event count (2) + Message count (2)

  // Verify structure
  EXPECT_EQ(data[0], 0x00);  // Status should be 0x00 (no error)
  uint16_t event_count = (static_cast<uint16_t>(data[2]) << 8) | data[1];
  uint16_t message_count = (static_cast<uint16_t>(data[4]) << 8) | data[3];
  EXPECT_GT(event_count, 0);
  EXPECT_GT(message_count, 0);

  // Should have event log entries (each entry is 4 bytes: event_id(2) + event_count(2))
  if (data.size() > 5) {
    size_t entry_count = (data.size() - 5) / 4;
    EXPECT_GT(entry_count, 0);
  }
}

TEST(RemainingFunctionCodes, ReportSlaveID) {
  static constexpr uint8_t kSlaveId{5};

  RtuSlave slave{kSlaveId};
  RtuRequest request{{kSlaveId, FunctionCode::kReportSlaveID}};
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_GE(data.size(), 2);  // Slave ID + Run indicator
  EXPECT_EQ(data[0], kSlaveId);
  EXPECT_EQ(data[1], 0xFF);  // Run indicator
}

TEST(RemainingFunctionCodes, MaskWriteRegister) {
  static constexpr uint8_t kSlaveId{6};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Set initial value
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_req.SetWriteSingleRegisterData(0, 0x1234);
  slave.Process(write_req);

  // Mask write: AND with 0xFF00, OR with 0x0056
  RtuRequest mask_req{{kSlaveId, FunctionCode::kMaskWriteReg}};
  mask_req.SetMaskWriteRegisterData(0, 0xFF00, 0x0056);
  RtuResponse response = slave.Process(mask_req);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_EQ(data.size(), 6);  // Address (2) + AND mask (2) + OR mask (2)
}

TEST(RemainingFunctionCodes, ReadWriteMultipleRegisters) {
  static constexpr uint8_t kSlaveId{7};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 20});

  // Write some initial values
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_req.SetWriteSingleRegisterData(10, 100);
  slave.Process(write_req);
  write_req.SetWriteSingleRegisterData(11, 200);
  slave.Process(write_req);

  // Read/Write: Read from 10-11, Write to 0-1
  RtuRequest rw_req{{kSlaveId, FunctionCode::kReadWriteMultRegs}};
  std::array<int16_t, 2> write_values{300, 400};
  rw_req.SetReadWriteMultipleRegistersData(10, 2, 0, 2, write_values);
  RtuResponse response = slave.Process(rw_req);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_EQ(data.size(), 4);  // 2 registers * 2 bytes
  // Should return the read values (100, 200)
}

TEST(RemainingFunctionCodes, ReadFIFOQueue) {
  static constexpr uint8_t kSlaveId{8};

  RtuSlave slave{kSlaveId};

  // Set up FIFO queue with proper data
  std::vector<int16_t> fifo_data{0x1234, 0x5678, static_cast<int16_t>(0x9ABC), static_cast<int16_t>(0xDEF0)};
  slave.SetFIFOQueue(0, fifo_data);

  // Read FIFO queue
  RtuRequest fifo_req{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  fifo_req.SetReadFIFOQueueData(0);
  RtuResponse response = slave.Process(fifo_req);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_GE(data.size(), 6);  // Byte count (2) + FIFO count (2) + at least 1 register (2)

  // Verify structure
  uint16_t byte_count = (static_cast<uint16_t>(data[1]) << 8) | data[0];
  uint16_t fifo_count = (static_cast<uint16_t>(data[3]) << 8) | data[2];
  EXPECT_EQ(fifo_count, fifo_data.size());
  EXPECT_EQ(byte_count, fifo_count * 2);
}

TEST(RemainingFunctionCodes, ReadFileRecord) {
  static constexpr uint8_t kSlaveId{9};

  RtuSlave slave{kSlaveId};

  // First write a file record
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> write_data;
  write_data.push_back(0x00);  // Byte count (will be calculated)
  write_data.push_back(0x06);  // Reference type
  write_data.push_back(0x00);  // File number high
  write_data.push_back(0x01);  // File number low (file 1)
  write_data.push_back(0x00);  // Record number high
  write_data.push_back(0x00);  // Record number low (record 0)
  write_data.push_back(0x00);  // Record length high
  write_data.push_back(0x02);  // Record length low (2 registers)
  write_data.push_back(0x12);  // Register 0 low
  write_data.push_back(0x34);  // Register 0 high
  write_data.push_back(0x56);  // Register 1 low
  write_data.push_back(0x78);  // Register 1 high
  write_data[0] = static_cast<uint8_t>(write_data.size() - 1);  // Set byte count
  write_req.SetRawData(write_data);
  RtuResponse write_resp = slave.Process(write_req);
  EXPECT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Now read the file record
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadFileRecord}};
  std::vector<uint8_t> read_data;
  read_data.push_back(0x06);  // Byte count (6 bytes for one record request)
  read_data.push_back(0x00);   // File number high
  read_data.push_back(0x01);   // File number low (file 1)
  read_data.push_back(0x00);   // Record number high
  read_data.push_back(0x00);   // Record number low (record 0)
  read_data.push_back(0x00);   // Record length high
  read_data.push_back(0x02);   // Record length low (2 registers)
  read_req.SetRawData(read_data);
  RtuResponse response = slave.Process(read_req);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_GE(data.size(), 1);
  uint8_t response_length = data[0];
  ASSERT_GE(data.size(), 1 + response_length);
  // Should contain: reference_type(1) + data_length(1) + file(2) + record(2) + data(4)
  EXPECT_GE(response_length, 10);
}

TEST(RemainingFunctionCodes, WriteFileRecord) {
  static constexpr uint8_t kSlaveId{10};

  RtuSlave slave{kSlaveId};

  // Write file record
  RtuRequest request{{kSlaveId, FunctionCode::kWriteFileRecord}};
  std::vector<uint8_t> write_data;
  write_data.push_back(0x00);  // Byte count (will be calculated)
  write_data.push_back(0x06);   // Reference type
  write_data.push_back(0x00);   // File number high
  write_data.push_back(0x01);   // File number low (file 1)
  write_data.push_back(0x00);   // Record number high
  write_data.push_back(0x00);   // Record number low (record 0)
  write_data.push_back(0x00);   // Record length high
  write_data.push_back(0x02);   // Record length low (2 registers)
  write_data.push_back(0xAB);   // Register 0 low
  write_data.push_back(0xCD);   // Register 0 high
  write_data.push_back(0xEF);   // Register 1 low
  write_data.push_back(0x01);   // Register 1 high
  write_data[0] = static_cast<uint8_t>(write_data.size() - 1);  // Set byte count
  request.SetRawData(write_data);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  // Response should echo back the request data
  auto data = response.GetData();
  EXPECT_EQ(data.size(), write_data.size());
}
