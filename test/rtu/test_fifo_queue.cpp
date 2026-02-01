#include <gtest/gtest.h>
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"

using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::MakeInt16;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

TEST(FIFOQueue, SlaveReadFIFOQueue_ProperQueue) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};

  // Set up FIFO queue with multiple values
  std::vector<int16_t> fifo_data{0x1111, 0x2222, 0x3333, 0x4444, 0x5555};
  slave.SetFIFOQueue(0, fifo_data);

  // Read FIFO queue
  RtuRequest request{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  request.SetReadFIFOQueueData(0);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  ASSERT_GE(data.size(), 6);

  // Parse response (Modbus RTU uses big-endian: high byte first, then low byte)
  uint16_t byte_count = MakeInt16(data[1], data[0]);
  uint16_t fifo_count = MakeInt16(data[3], data[2]);

  EXPECT_EQ(fifo_count, fifo_data.size());
  EXPECT_EQ(byte_count, fifo_count * 2);

  // Verify data
  for (size_t i = 0; i < fifo_data.size(); ++i) {
    size_t byte_idx = 4 + i * 2;
    int16_t value = MakeInt16(data[byte_idx + 1], data[byte_idx]);
    EXPECT_EQ(value, fifo_data[i]);
  }
}

TEST(FIFOQueue, SlaveReadFIFOQueue_EmptyQueue) {
  static constexpr uint8_t kSlaveId{2};

  RtuSlave slave{kSlaveId};

  // Set up empty FIFO queue
  std::vector<int16_t> empty_fifo;
  slave.SetFIFOQueue(0, empty_fifo);

  RtuRequest request{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  request.SetReadFIFOQueueData(0);
  RtuResponse response = slave.Process(request);

  // Empty queue should return illegal data address
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(FIFOQueue, SlaveReadFIFOQueue_InvalidAddress) {
  static constexpr uint8_t kSlaveId{3};

  RtuSlave slave{kSlaveId};

  // Set up FIFO at address 0
  std::vector<int16_t> fifo_data{0x1234};
  slave.SetFIFOQueue(0, fifo_data);

  // Try to read from non-existent FIFO address
  RtuRequest request{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  request.SetReadFIFOQueueData(100);  // Invalid address
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(FIFOQueue, SlaveReadFIFOQueue_MultipleQueues) {
  static constexpr uint8_t kSlaveId{4};

  RtuSlave slave{kSlaveId};

  // Set up multiple FIFO queues
  std::vector<int16_t> fifo1{static_cast<int16_t>(0xAAAA), static_cast<int16_t>(0xBBBB)};
  std::vector<int16_t> fifo2{static_cast<int16_t>(0xCCCC), static_cast<int16_t>(0xDDDD), static_cast<int16_t>(0xEEEE)};
  slave.SetFIFOQueue(0, fifo1);
  slave.SetFIFOQueue(1, fifo2);

  // Read first FIFO
  RtuRequest req1{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  req1.SetReadFIFOQueueData(0);
  RtuResponse resp1 = slave.Process(req1);
  EXPECT_EQ(resp1.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data1 = resp1.GetData();
  uint16_t count1 = MakeInt16(data1[3], data1[2]);
  EXPECT_EQ(count1, 2);

  // Read second FIFO
  RtuRequest req2{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  req2.SetReadFIFOQueueData(1);
  RtuResponse resp2 = slave.Process(req2);
  EXPECT_EQ(resp2.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data2 = resp2.GetData();
  uint16_t count2 = MakeInt16(data2[3], data2[2]);
  EXPECT_EQ(count2, 3);
}

TEST(FIFOQueue, SlaveReadFIFOQueue_LargeQueue) {
  static constexpr uint8_t kSlaveId{5};

  RtuSlave slave{kSlaveId};

  // Set up large FIFO queue (31 registers max per Modbus spec)
  std::vector<int16_t> large_fifo;
  for (int i = 0; i < 31; ++i) {
    large_fifo.push_back(static_cast<int16_t>(i * 0x100 + i));
  }
  slave.SetFIFOQueue(0, large_fifo);

  RtuRequest request{{kSlaveId, FunctionCode::kReadFIFOQueue}};
  request.SetReadFIFOQueueData(0);
  RtuResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = response.GetData();
  uint16_t fifo_count = MakeInt16(data[3], data[2]);
  EXPECT_EQ(fifo_count, 31);
}
