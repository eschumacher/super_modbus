#include <gtest/gtest.h>
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
using supermb::MakeInt16;
using supermb::MemoryTransport;
using supermb::RtuFrame;
using supermb::RtuMaster;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

TEST(EdgeCases, SingleRegisterReadWrite) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 1});

  // Write single register
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_req.SetWriteSingleRegisterData(0, 0x1234);
  RtuResponse write_resp = slave.Process(write_req);
  EXPECT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read single register
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  // Read register responses include byte_count (1 byte) + register data (1 register * 2 bytes = 2 bytes)
  EXPECT_EQ(read_resp.GetData().size(), 3);

  // Skip byte_count byte, then read register value (high byte first, then low byte in Modbus RTU)
  auto data = read_resp.GetData();
  int16_t value = MakeInt16(data[2], data[1]);
  EXPECT_EQ(value, static_cast<int16_t>(0x1234));
}

TEST(EdgeCases, SingleCoilReadWrite) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 1});

  // Write single coil
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleCoil}};
  write_req.SetWriteSingleCoilData(0, true);
  RtuResponse write_resp = slave.Process(write_req);
  EXPECT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read single coil
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadCoils}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  auto data = read_resp.GetData();
  ASSERT_GE(data.size(), 2);
  EXPECT_TRUE((data[1] & 0x01) != 0);
}

TEST(EdgeCases, MaximumCoilsInOneByte) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 8});

  // Set all 8 coils
  for (int i = 0; i < 8; ++i) {
    RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleCoil}};
    write_req.SetWriteSingleCoilData(i, (i % 2 == 0));
    slave.Process(write_req);
  }

  // Read all 8 coils
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadCoils}};
  read_req.SetAddressSpan({0, 8});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  auto data = read_resp.GetData();
  ASSERT_GE(data.size(), 2);
  EXPECT_EQ(data[0], 1);  // Byte count = 1
}

TEST(EdgeCases, NineCoilsSpanTwoBytes) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddCoils({0, 9});

  // Read 9 coils (should span 2 bytes)
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadCoils}};
  read_req.SetAddressSpan({0, 9});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  auto data = read_resp.GetData();
  ASSERT_GE(data.size(), 2);
  EXPECT_EQ(data[0], 2);  // Byte count = 2 (for 9 coils)
}

TEST(EdgeCases, NegativeRegisterValue) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  // Write negative value
  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_req.SetWriteSingleRegisterData(0, -1234);
  RtuResponse write_resp = slave.Process(write_req);
  EXPECT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read it back
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Skip byte_count byte, then read register value (high byte first, then low byte in Modbus RTU)
  auto data = read_resp.GetData();
  int16_t value = MakeInt16(data[2], data[1]);
  EXPECT_EQ(value, -1234);
}

TEST(EdgeCases, MaximumRegisterValue) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr int16_t kMaxValue = 32767;

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_req.SetWriteSingleRegisterData(0, kMaxValue);
  RtuResponse write_resp = slave.Process(write_req);
  EXPECT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  // Skip byte_count byte, then read register value (high byte first, then low byte in Modbus RTU)
  auto data = read_resp.GetData();
  int16_t value = MakeInt16(data[2], data[1]);
  EXPECT_EQ(value, kMaxValue);
}

TEST(EdgeCases, MinimumRegisterValue) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr int16_t kMinValue = -32768;

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  RtuRequest write_req{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_req.SetWriteSingleRegisterData(0, kMinValue);
  RtuResponse write_resp = slave.Process(write_req);
  EXPECT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  // Skip byte_count byte, then read register value (high byte first, then low byte in Modbus RTU)
  auto data = read_resp.GetData();
  int16_t value = MakeInt16(data[2], data[1]);
  EXPECT_EQ(value, kMinValue);
}

TEST(EdgeCases, NonContiguousAddressSpans) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 5});
  slave.AddHoldingRegisters({10, 5});  // Non-contiguous

  // Read from first span
  RtuRequest read_req1{{kSlaveId, FunctionCode::kReadHR}};
  read_req1.SetAddressSpan({0, 5});
  RtuResponse resp1 = slave.Process(read_req1);
  EXPECT_EQ(resp1.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Read from second span
  RtuRequest read_req2{{kSlaveId, FunctionCode::kReadHR}};
  read_req2.SetAddressSpan({10, 5});
  RtuResponse resp2 = slave.Process(read_req2);
  EXPECT_EQ(resp2.GetExceptionCode(), ExceptionCode::kAcknowledge);

  // Try to read across the gap - should fail
  RtuRequest read_req3{{kSlaveId, FunctionCode::kReadHR}};
  read_req3.SetAddressSpan({4, 7});  // Spans from 4 to 10, crossing gap
  RtuResponse resp3 = slave.Process(read_req3);
  EXPECT_EQ(resp3.GetExceptionCode(), ExceptionCode::kIllegalDataAddress);
}

TEST(EdgeCases, OverlappingAddressSpans) {
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});
  slave.AddHoldingRegisters({5, 10});  // Overlaps with first

  // Should be able to read from overlapping area
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({5, 5});
  RtuResponse resp = slave.Process(read_req);
  EXPECT_EQ(resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
}

TEST(EdgeCases, EmptyFrame) {
  std::vector<uint8_t> empty_frame;
  auto decoded = RtuFrame::DecodeRequest(empty_frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(EdgeCases, FrameWithOnlySlaveID) {
  std::vector<uint8_t> minimal_frame{0x01};  // Only slave ID
  auto decoded = RtuFrame::DecodeRequest(minimal_frame);
  EXPECT_FALSE(decoded.has_value());
}

TEST(EdgeCases, MemoryTransportReset) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03};
  transport.SetReadData(test_data);

  // Read some data
  uint8_t buffer[10];
  (void)transport.Read(buffer);

  // Reset and read again
  transport.ResetReadPosition();
  int bytes_read = transport.Read(buffer);
  EXPECT_EQ(bytes_read, 3);
}

TEST(EdgeCases, MemoryTransportClearWrite) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03};
  (void)transport.Write(test_data);

  EXPECT_GT(transport.GetWrittenData().size(), 0);

  transport.ClearWriteBuffer();
  EXPECT_EQ(transport.GetWrittenData().size(), 0);
}
