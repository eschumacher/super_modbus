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

TEST(TCPSlave, ProcessReadHoldingRegisters) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  // Set some register values
  for (int i = 0; i < 10; ++i) {
    slave.AddHoldingRegisters({static_cast<uint16_t>(i), 1});
  }

  TcpRequest request{{1, kUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_EQ(response.GetTransactionId(), 1);
  EXPECT_EQ(response.GetUnitId(), kUnitId);
}

TEST(TCPSlave, ProcessWriteSingleRegister) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteSingleReg}};
  request.SetWriteSingleRegisterData(5, 1234);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_EQ(response.GetData().size(), 4);  // Address (2) + Value (2)
}

TEST(TCPSlave, ProcessReadCoils) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddCoils({0, 8});

  TcpRequest request{{1, kUnitId, FunctionCode::kReadCoils}};
  request.SetAddressSpan({0, 8});

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GT(response.GetData().size(), 0);
}

TEST(TCPSlave, ProcessWriteSingleCoil) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};
  slave.AddCoils({0, 10});

  TcpRequest request{{1, kUnitId, FunctionCode::kWriteSingleCoil}};
  request.SetWriteSingleCoilData(5, true);

  TcpResponse response = slave.Process(request);
  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_EQ(response.GetData().size(), 4);  // Address (2) + Value (2)
}

TEST(TCPSlave, ProcessIncomingFrame) {
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

  // Verify response was written
  auto written = transport.GetWrittenData();
  ASSERT_GT(written.size(), 0);
  auto decoded = TcpFrame::DecodeResponse(written);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetUnitId(), kUnitId);
  EXPECT_EQ(decoded->GetTransactionId(), 1);
}

TEST(TCPSlave, ProcessIncomingFrameWrongUnitId) {
  static constexpr uint8_t kUnitId{1};
  static constexpr uint8_t kWrongUnitId{2};
  MemoryTransport transport;
  TcpSlave slave{kUnitId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kWrongUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);  // Should not process request for wrong unit ID
}

// Broadcast (unit ID 0): write operations are accepted and processed, but no response is sent
TEST(TCPSlave, ProcessIncomingFrame_BroadcastWrite_AcceptedNoResponse) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr uint8_t kBroadcastUnitId{0};
  MemoryTransport transport;
  TcpSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kBroadcastUnitId, FunctionCode::kWriteSingleReg}};
  request.SetWriteSingleRegisterData(0, 0x1234);

  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_TRUE(processed);

  // Broadcast: no response sent (Modbus spec)
  auto written = transport.GetWrittenData();
  EXPECT_TRUE(written.empty());

  // Verify register was written by reading via normal request
  TcpRequest read_req{{2, kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  TcpResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  ASSERT_GE(read_resp.GetData().size(), 3);  // byte_count + 2 bytes
  EXPECT_EQ(read_resp.GetData()[1], 0x12);
  EXPECT_EQ(read_resp.GetData()[2], 0x34);
}

// Broadcast (unit ID 0): read operations are rejected
TEST(TCPSlave, ProcessIncomingFrame_BroadcastRead_Rejected) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr uint8_t kBroadcastUnitId{0};
  MemoryTransport transport;
  TcpSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});

  TcpRequest request{{1, kBroadcastUnitId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  auto frame = TcpFrame::EncodeRequest(request);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);
}

TEST(TCPSlave, Poll) {
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

TEST(TCPSlave, PollNoData) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport(0);  // Empty transport
  TcpSlave slave{kUnitId};

  bool processed = slave.Poll(transport);
  EXPECT_FALSE(processed);
}

TEST(TCPSlave, GetIdSetId) {
  static constexpr uint8_t kUnitId{5};
  TcpSlave slave{kUnitId};

  EXPECT_EQ(slave.GetId(), kUnitId);

  slave.SetId(10);
  EXPECT_EQ(slave.GetId(), 10);
}

TEST(TCPSlave, ProcessReadExceptionStatus) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReadExceptionStatus}};
  TcpResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_EQ(response.GetData().size(), 1);  // Exception status byte
}

TEST(TCPSlave, ProcessReportSlaveID) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  TcpRequest request{{1, kUnitId, FunctionCode::kReportSlaveID}};
  TcpResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_GE(response.GetData().size(), 2);  // Slave ID + run indicator
  EXPECT_EQ(response.GetData()[0], kUnitId);
}

TEST(TCPSlave, ProcessIllegalFunction) {
  static constexpr uint8_t kUnitId{1};
  TcpSlave slave{kUnitId};

  // Use an invalid function code (0x00)
  TcpRequest request{{1, kUnitId, FunctionCode::kInvalid}};
  TcpResponse response = slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kIllegalFunction);
}
