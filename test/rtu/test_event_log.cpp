#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"

using supermb::AddressSpan;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

TEST(EventLog, EventLogPopulation) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Generate multiple events
  RtuRequest req1{{kSlaveId, FunctionCode::kReadHR}};
  req1.SetAddressSpan({0, 5});
  slave.Process(req1);

  RtuRequest req2{{kSlaveId, FunctionCode::kWriteSingleReg}};
  req2.SetWriteSingleRegisterData(0, 0x1234);
  slave.Process(req2);

  RtuRequest req3{{kSlaveId, FunctionCode::kReadExceptionStatus}};
  slave.Process(req3);

  // Get event log
  RtuRequest log_req{{kSlaveId, FunctionCode::kGetComEventLog}};
  RtuResponse log_resp = slave.Process(log_req);

  EXPECT_EQ(log_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = log_resp.GetData();
  ASSERT_GE(data.size(), 5);

  // Verify structure
  EXPECT_EQ(data[0], 0x00);  // Status
  uint16_t event_count = (static_cast<uint16_t>(data[2]) << 8) | data[1];
  uint16_t message_count = (static_cast<uint16_t>(data[4]) << 8) | data[3];

  EXPECT_GE(event_count, 3);
  EXPECT_GE(message_count, 3);

  // Verify event log entries exist
  if (data.size() > 5) {
    size_t entry_count = (data.size() - 5) / 4;  // Each entry is 4 bytes
    EXPECT_GT(entry_count, 0);
    EXPECT_LE(entry_count, 64);  // Max 64 entries per Modbus spec
  }
}

TEST(EventLog, EventLogOverflow) {
  static constexpr uint8_t kSlaveId{2};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Generate more than 64 events to test overflow
  for (int i = 0; i < 70; ++i) {
    RtuRequest req{{kSlaveId, FunctionCode::kReadHR}};
    req.SetAddressSpan({0, 1});
    slave.Process(req);
  }

  // Get event log
  RtuRequest log_req{{kSlaveId, FunctionCode::kGetComEventLog}};
  RtuResponse log_resp = slave.Process(log_req);

  EXPECT_EQ(log_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = log_resp.GetData();
  ASSERT_GE(data.size(), 5);

  uint16_t event_count = (static_cast<uint16_t>(data[2]) << 8) | data[1];
  uint16_t message_count = (static_cast<uint16_t>(data[4]) << 8) | data[3];

  // Event count should continue incrementing
  EXPECT_GE(event_count, 70);
  EXPECT_GE(message_count, 70);

  // But log entries should be capped at 64
  if (data.size() > 5) {
    size_t entry_count = (data.size() - 5) / 4;
    EXPECT_LE(entry_count, 64);
    EXPECT_GE(entry_count, 1);  // Should have at least some entries
  }
}

TEST(EventLog, EventLogContent) {
  static constexpr uint8_t kSlaveId{3};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Generate specific events
  RtuRequest req1{{kSlaveId, FunctionCode::kReadHR}};
  req1.SetAddressSpan({0, 1});
  slave.Process(req1);

  RtuRequest req2{{kSlaveId, FunctionCode::kWriteSingleReg}};
  req2.SetWriteSingleRegisterData(0, 0x5678);
  slave.Process(req2);

  // Get event log
  RtuRequest log_req{{kSlaveId, FunctionCode::kGetComEventLog}};
  RtuResponse log_resp = slave.Process(log_req);

  EXPECT_EQ(log_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = log_resp.GetData();
  ASSERT_GE(data.size(), 5);

  // Check that event log contains entries
  if (data.size() > 8) {
    // First entry should be recent
    uint16_t event_id1 = (static_cast<uint16_t>(data[6]) << 8) | data[5];
    uint16_t event_count1 = (static_cast<uint16_t>(data[8]) << 8) | data[7];

    // Event ID should be a function code
    EXPECT_GT(event_id1, 0);
    EXPECT_LE(event_id1, 24);
    EXPECT_GT(event_count1, 0);
  }
}

TEST(EventLog, MessageCountTracking) {
  static constexpr uint8_t kSlaveId{4};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Process multiple requests
  for (int i = 0; i < 10; ++i) {
    RtuRequest req{{kSlaveId, FunctionCode::kReadHR}};
    req.SetAddressSpan({0, 1});
    slave.Process(req);
  }

  // Get event log
  RtuRequest log_req{{kSlaveId, FunctionCode::kGetComEventLog}};
  RtuResponse log_resp = slave.Process(log_req);

  EXPECT_EQ(log_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = log_resp.GetData();
  ASSERT_GE(data.size(), 5);

  uint16_t message_count = (static_cast<uint16_t>(data[4]) << 8) | data[3];
  EXPECT_GE(message_count, 11);  // 10 read requests + 1 event log request
}

TEST(EventLog, EventLogAfterException) {
  static constexpr uint8_t kSlaveId{5};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Generate a request that causes exception
  RtuRequest bad_req{{kSlaveId, FunctionCode::kReadHR}};
  bad_req.SetAddressSpan({100, 5});  // Invalid address
  slave.Process(bad_req);

  // Get event log - should still track the event
  RtuRequest log_req{{kSlaveId, FunctionCode::kGetComEventLog}};
  RtuResponse log_resp = slave.Process(log_req);

  EXPECT_EQ(log_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = log_resp.GetData();
  ASSERT_GE(data.size(), 5);

  uint16_t event_count = (static_cast<uint16_t>(data[2]) << 8) | data[1];
  uint16_t message_count = (static_cast<uint16_t>(data[4]) << 8) | data[3];

  EXPECT_GE(event_count, 1);
  EXPECT_GE(message_count, 2);  // Bad request + event log request
}
