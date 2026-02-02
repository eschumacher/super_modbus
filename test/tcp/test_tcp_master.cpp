#include <gtest/gtest.h>
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
using supermb::MemoryTransport;
using supermb::TcpFrame;
using supermb::TcpMaster;
using supermb::TcpRequest;
using supermb::TcpResponse;

TEST(TCPMaster, ReadHoldingRegisters) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  // Master starts with transaction ID 1, so first request will use 1
  TcpResponse response{1, kUnitId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x0A);  // Byte count (5 registers * 2)
  response.EmplaceBack(0x00);  // High byte of register 0
  response.EmplaceBack(0x01);  // Low byte of register 0
  response.EmplaceBack(0x00);  // High byte of register 1
  response.EmplaceBack(0x02);  // Low byte of register 1
  response.EmplaceBack(0x00);  // High byte of register 2
  response.EmplaceBack(0x03);  // Low byte of register 2
  response.EmplaceBack(0x00);  // High byte of register 3
  response.EmplaceBack(0x04);  // Low byte of register 3
  response.EmplaceBack(0x00);  // High byte of register 4
  response.EmplaceBack(0x05);  // Low byte of register 4

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadHoldingRegisters(kUnitId, 0, 5);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 5);
  EXPECT_EQ((*result)[0], 1);
  EXPECT_EQ((*result)[1], 2);
  EXPECT_EQ((*result)[2], 3);
  EXPECT_EQ((*result)[3], 4);
  EXPECT_EQ((*result)[4], 5);
}

TEST(TCPMaster, WriteSingleRegister) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kWriteSingleReg};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // High byte of address
  response.EmplaceBack(0x32);  // Low byte of address (50)
  response.EmplaceBack(0x04);  // High byte of value
  response.EmplaceBack(0xD2);  // Low byte of value (1234)

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.WriteSingleRegister(kUnitId, 50, 1234);
  EXPECT_TRUE(result);
}

TEST(TCPMaster, ReadCoils) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x01);  // Byte count
  response.EmplaceBack(0xAA);  // Coils: 10101010 (bit 0=0, bit 1=1, bit 2=0, etc.)

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReadCoils(kUnitId, 0, 8);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 8);
  EXPECT_FALSE((*result)[0]);
  EXPECT_TRUE((*result)[1]);
  EXPECT_FALSE((*result)[2]);
  EXPECT_TRUE((*result)[3]);
  EXPECT_FALSE((*result)[4]);
  EXPECT_TRUE((*result)[5]);
  EXPECT_FALSE((*result)[6]);
  EXPECT_TRUE((*result)[7]);
}

TEST(TCPMaster, WriteSingleCoil) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{1, kUnitId, FunctionCode::kWriteSingleCoil};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // High byte of address
  response.EmplaceBack(0x0A);  // Low byte of address (10)
  response.EmplaceBack(0xFF);  // High byte of value (ON)
  response.EmplaceBack(0x00);  // Low byte of value

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  bool result = master.WriteSingleCoil(kUnitId, 10, true);
  EXPECT_TRUE(result);
}

TEST(TCPMaster, GetNextTransactionId) {
  MemoryTransport transport;
  TcpMaster master{transport};

  uint16_t id1 = master.GetNextTransactionId();
  uint16_t id2 = master.GetNextTransactionId();
  uint16_t id3 = master.GetNextTransactionId();

  EXPECT_EQ(id1, 1);
  EXPECT_EQ(id2, 2);
  EXPECT_EQ(id3, 3);
}

TEST(TCPMaster, SendRequest) {
  static constexpr uint8_t kUnitId{1};
  MemoryTransport transport;
  TcpMaster master{transport};

  // SendRequest uses the transaction ID from the request
  uint16_t transaction_id = master.GetNextTransactionId();
  TcpRequest request{{transaction_id, kUnitId, FunctionCode::kReadExceptionStatus}};

  TcpResponse response{transaction_id, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x42);  // Exception status

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.SendRequest(request, 100);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetUnitId(), kUnitId);
  EXPECT_EQ(result->GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_EQ(result->GetData()[0], 0x42);
}

TEST(TCPMaster, ReceiveResponse) {
  static constexpr uint8_t kUnitId{1};
  static constexpr uint16_t kTransactionId{123};
  MemoryTransport transport;
  TcpMaster master{transport};

  TcpResponse response{kTransactionId, kUnitId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x42);

  auto frame = TcpFrame::EncodeResponse(response);
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kTransactionId, 100);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetTransactionId(), kTransactionId);
  EXPECT_EQ(result->GetUnitId(), kUnitId);
}

TEST(TCPMaster, ReceiveResponseWrongTransactionId) {
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
