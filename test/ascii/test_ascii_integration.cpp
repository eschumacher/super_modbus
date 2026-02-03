#include "super_modbus/ascii/ascii_frame.hpp"
#include "super_modbus/ascii/ascii_master.hpp"
#include "super_modbus/ascii/ascii_slave.hpp"
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/transport/memory_transport.hpp"
#include <gtest/gtest.h>
#include <string>

using supermb::AsciiFrame;
using supermb::AsciiMaster;
using supermb::AsciiSlave;
using supermb::ExceptionCode;
using supermb::FunctionCode;
using supermb::MemoryTransport;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

TEST(AsciiIntegration, MasterSlaveReadHoldingRegisters) {
  constexpr uint8_t kSlaveId = 1;

  AsciiSlave slave(kSlaveId);
  slave.AddHoldingRegisters({0, 10});

  RtuRequest request({kSlaveId, FunctionCode::kReadHR});
  request.SetAddressSpan({0, 5});
  std::string request_frame = AsciiFrame::EncodeRequest(request);

  MemoryTransport transport;
  transport.SetReadData(
      std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(request_frame.data()), request_frame.size()));

  EXPECT_TRUE(slave.ProcessIncomingFrame(transport, 100));

  auto written = transport.GetWrittenData();
  std::string response_str(written.begin(), written.end());
  auto decoded = AsciiFrame::DecodeResponse(response_str);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetExceptionCode(), ExceptionCode::kAcknowledge);
}

TEST(AsciiIntegration, MasterReadHoldingRegistersViaLoopback) {
  constexpr uint8_t kSlaveId = 1;

  AsciiSlave slave(kSlaveId);
  slave.AddHoldingRegisters({0, 10});

  RtuRequest write_req({kSlaveId, FunctionCode::kWriteSingleReg});
  write_req.SetWriteSingleRegisterData(0, 0x1234);
  RtuResponse write_resp = slave.Process(write_req);
  EXPECT_EQ(write_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);

  RtuRequest read_req({kSlaveId, FunctionCode::kReadHR});
  read_req.SetAddressSpan({0, 1});
  std::string read_frame = AsciiFrame::EncodeRequest(read_req);
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  std::string resp_frame = AsciiFrame::EncodeResponse(read_resp);

  MemoryTransport transport;
  transport.SetReadData(
      std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(resp_frame.data()), resp_frame.size()));

  AsciiMaster master(transport);
  auto regs = master.ReadHoldingRegisters(kSlaveId, 0, 1);
  ASSERT_TRUE(regs.has_value());
  EXPECT_EQ(regs->size(), 1u);
  EXPECT_EQ(regs->at(0), 0x1234);
}

TEST(AsciiIntegration, SlaveProcess_Broadcast) {
  AsciiSlave slave(1);
  slave.AddHoldingRegisters({0, 10});
  RtuRequest request({0, FunctionCode::kWriteSingleReg});
  request.SetWriteSingleRegisterData(0, 0x1234);
  std::string frame = AsciiFrame::EncodeRequest(request);
  MemoryTransport transport;
  transport.SetReadData(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(frame.data()), frame.size()));
  EXPECT_TRUE(slave.ProcessIncomingFrame(transport, 100));
  auto written = transport.GetWrittenData();
  EXPECT_TRUE(written.empty());
}

TEST(AsciiIntegration, SlaveProcess_WrongSlaveId) {
  AsciiSlave slave(1);
  slave.AddHoldingRegisters({0, 10});
  RtuRequest request({2, FunctionCode::kReadHR});
  request.SetAddressSpan({0, 5});
  std::string frame = AsciiFrame::EncodeRequest(request);
  MemoryTransport transport;
  transport.SetReadData(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(frame.data()), frame.size()));
  EXPECT_FALSE(slave.ProcessIncomingFrame(transport, 100));
}

TEST(AsciiIntegration, SlavePoll) {
  AsciiSlave slave(1);
  slave.AddHoldingRegisters({0, 10});
  MemoryTransport transport(0);
  EXPECT_FALSE(slave.Poll(transport));
}

TEST(AsciiIntegration, MasterWriteFloatsReadFloats) {
  constexpr uint8_t kSlaveId = 1;

  AsciiSlave slave(kSlaveId);
  slave.AddHoldingRegisters({0, 20});
  slave.AddFloatRange(0, 4);

  slave.SetFloat(0, 3.14f);
  slave.SetFloat(1, 2.718f);

  std::vector<float> expected{3.14f, 2.718f};
  RtuRequest read_req({kSlaveId, FunctionCode::kReadHR});
  read_req.SetAddressSpan({0, 4});
  RtuResponse read_resp = slave.Process(read_req);
  std::string resp_frame = AsciiFrame::EncodeResponse(read_resp);

  MemoryTransport transport;
  transport.SetReadData(
      std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(resp_frame.data()), resp_frame.size()));

  AsciiMaster master(transport);
  auto floats = master.ReadFloats(kSlaveId, 0, 2);
  ASSERT_TRUE(floats.has_value());
  EXPECT_EQ(floats->size(), 2u);
  EXPECT_NEAR(floats->at(0), 3.14f, 0.001f);
  EXPECT_NEAR(floats->at(1), 2.718f, 0.001f);
}
