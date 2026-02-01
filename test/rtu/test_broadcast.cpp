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
using supermb::IsBroadcastableWrite;
using supermb::MakeInt16;
using supermb::MemoryTransport;
using supermb::RtuFrame;
using supermb::RtuMaster;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::RtuSlave;

// Helper class to simulate master-slave communication
class BroadcastSimulator {
 public:
  explicit BroadcastSimulator(uint8_t slave_id)
      : slave_(slave_id),
        master_(transport_) {}

  void SetupRegisters(AddressSpan span) { slave_.AddHoldingRegisters(span); }
  void SetupCoils(AddressSpan span) { slave_.AddCoils(span); }

  // Process master request through slave
  void ProcessMasterRequest() {
    auto master_request = transport_.GetWrittenData();
    if (master_request.empty()) {
      return;
    }

    transport_.ClearWriteBuffer();
    transport_.SetReadData(master_request);
    transport_.ResetReadPosition();

    // Slave processes (may or may not send response for broadcast)
    slave_.ProcessIncomingFrame(transport_, 100);

    auto slave_response = transport_.GetWrittenData();
    transport_.ClearWriteBuffer();
    transport_.SetReadData(slave_response);
    transport_.ResetReadPosition();
  }

  RtuMaster &GetMaster() { return master_; }
  RtuSlave &GetSlave() { return slave_; }
  MemoryTransport &GetTransport() { return transport_; }

 private:
  MemoryTransport transport_;
  RtuSlave slave_;
  RtuMaster master_;
};

TEST(Broadcast, MasterWriteSingleRegister_Broadcast) {
  static constexpr uint8_t kBroadcastId{0};
  static constexpr uint8_t kSlaveId{1};

  BroadcastSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Master sends broadcast write
  bool result = sim.GetMaster().WriteSingleRegister(kBroadcastId, 0, 0x1234);

  // Broadcast should succeed (returns dummy success response)
  EXPECT_TRUE(result);

  // Verify the request was sent
  auto written_data = sim.GetTransport().GetWrittenData();
  ASSERT_GT(written_data.size(), 0);

  // Decode and verify it's a broadcast
  auto decoded = RtuFrame::DecodeRequest(written_data);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), kBroadcastId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteSingleReg);
}

TEST(Broadcast, MasterWriteMultipleRegisters_Broadcast) {
  static constexpr uint8_t kBroadcastId{0};
  static constexpr uint8_t kSlaveId{1};

  BroadcastSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  std::vector<int16_t> values{100, 200, 300};
  bool result = sim.GetMaster().WriteMultipleRegisters(kBroadcastId, 0, values);

  EXPECT_TRUE(result);

  auto written_data = sim.GetTransport().GetWrittenData();
  auto decoded = RtuFrame::DecodeRequest(written_data);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), kBroadcastId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteMultRegs);
}

TEST(Broadcast, MasterWriteSingleCoil_Broadcast) {
  static constexpr uint8_t kBroadcastId{0};
  static constexpr uint8_t kSlaveId{1};

  BroadcastSimulator sim{kSlaveId};
  sim.SetupCoils({0, 10});

  bool result = sim.GetMaster().WriteSingleCoil(kBroadcastId, 0, true);

  EXPECT_TRUE(result);

  auto written_data = sim.GetTransport().GetWrittenData();
  auto decoded = RtuFrame::DecodeRequest(written_data);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), kBroadcastId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteSingleCoil);
}

TEST(Broadcast, MasterWriteMultipleCoils_Broadcast) {
  static constexpr uint8_t kBroadcastId{0};
  static constexpr uint8_t kSlaveId{1};

  BroadcastSimulator sim{kSlaveId};
  sim.SetupCoils({0, 10});

  std::vector<bool> values{true, false, true, false};
  // Convert vector<bool> to regular bool array for span
  bool bool_array[4] = {values[0], values[1], values[2], values[3]};
  bool result = sim.GetMaster().WriteMultipleCoils(kBroadcastId, 0, std::span<bool const>(bool_array));

  EXPECT_TRUE(result);

  auto written_data = sim.GetTransport().GetWrittenData();
  auto decoded = RtuFrame::DecodeRequest(written_data);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->GetSlaveId(), kBroadcastId);
  EXPECT_EQ(decoded->GetFunctionCode(), FunctionCode::kWriteMultCoils);
}

TEST(Broadcast, SlaveAcceptsBroadcastWrite_NoResponse) {
  static constexpr uint8_t kBroadcastId{0};
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});
  MemoryTransport transport;

  // Create broadcast write request
  RtuRequest request{{kBroadcastId, FunctionCode::kWriteSingleReg}};
  request.SetWriteSingleRegisterData(0, 0x5678);
  auto frame = RtuFrame::EncodeRequest(request);

  // Put request in transport
  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Process broadcast - should accept but not send response
  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_TRUE(processed);

  // Verify no response was sent (broadcast doesn't get responses)
  auto written_data = transport.GetWrittenData();
  EXPECT_EQ(written_data.size(), 0);

  // Verify the write actually happened
  RtuRequest read_req{{kSlaveId, FunctionCode::kReadHR}};
  read_req.SetAddressSpan({0, 1});
  RtuResponse read_resp = slave.Process(read_req);
  EXPECT_EQ(read_resp.GetExceptionCode(), ExceptionCode::kAcknowledge);
  auto data = read_resp.GetData();
  ASSERT_GE(data.size(), 3);
  int16_t value = MakeInt16(data[2], data[1]);
  EXPECT_EQ(value, 0x5678);
}

TEST(Broadcast, SlaveRejectsBroadcastRead) {
  static constexpr uint8_t kBroadcastId{0};
  static constexpr uint8_t kSlaveId{1};

  RtuSlave slave{kSlaveId};
  slave.AddHoldingRegisters({0, 10});
  MemoryTransport transport;

  // Create broadcast read request (invalid - reads can't be broadcast)
  RtuRequest request{{kBroadcastId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});
  auto frame = RtuFrame::EncodeRequest(request);

  transport.SetReadData(frame);
  transport.ResetReadPosition();

  // Process broadcast read - should reject (reads can't be broadcast)
  bool processed = slave.ProcessIncomingFrame(transport, 100);
  EXPECT_FALSE(processed);
}

TEST(Broadcast, MultipleSlavesReceiveBroadcast) {
  static constexpr uint8_t kBroadcastId{0};
  static constexpr uint8_t kSlaveId1{1};
  static constexpr uint8_t kSlaveId2{2};

  RtuSlave slave1{kSlaveId1};
  RtuSlave slave2{kSlaveId2};
  slave1.AddHoldingRegisters({0, 10});
  slave2.AddHoldingRegisters({0, 10});

  MemoryTransport transport1;
  MemoryTransport transport2;

  // Create broadcast write
  RtuRequest request{{kBroadcastId, FunctionCode::kWriteSingleReg}};
  request.SetWriteSingleRegisterData(0, 0xABCD);
  auto frame = RtuFrame::EncodeRequest(request);

  // Both slaves should process the broadcast
  transport1.SetReadData(frame);
  transport1.ResetReadPosition();
  bool processed1 = slave1.ProcessIncomingFrame(transport1, 100);
  EXPECT_TRUE(processed1);

  transport2.SetReadData(frame);
  transport2.ResetReadPosition();
  bool processed2 = slave2.ProcessIncomingFrame(transport2, 100);
  EXPECT_TRUE(processed2);

  // Both should have the value written
  RtuRequest read_req1{{kSlaveId1, FunctionCode::kReadHR}};
  read_req1.SetAddressSpan({0, 1});
  RtuResponse resp1 = slave1.Process(read_req1);
  EXPECT_EQ(resp1.GetExceptionCode(), ExceptionCode::kAcknowledge);

  RtuRequest read_req2{{kSlaveId2, FunctionCode::kReadHR}};
  read_req2.SetAddressSpan({0, 1});
  RtuResponse resp2 = slave2.Process(read_req2);
  EXPECT_EQ(resp2.GetExceptionCode(), ExceptionCode::kAcknowledge);
}

TEST(Broadcast, IsBroadcastableWrite_HelperFunction) {
  // Test the helper function
  EXPECT_TRUE(IsBroadcastableWrite(FunctionCode::kWriteSingleCoil));
  EXPECT_TRUE(IsBroadcastableWrite(FunctionCode::kWriteSingleReg));
  EXPECT_TRUE(IsBroadcastableWrite(FunctionCode::kWriteMultCoils));
  EXPECT_TRUE(IsBroadcastableWrite(FunctionCode::kWriteMultRegs));
  EXPECT_TRUE(IsBroadcastableWrite(FunctionCode::kWriteFileRecord));

  EXPECT_FALSE(IsBroadcastableWrite(FunctionCode::kReadCoils));
  EXPECT_FALSE(IsBroadcastableWrite(FunctionCode::kReadHR));
  EXPECT_FALSE(IsBroadcastableWrite(FunctionCode::kReadExceptionStatus));
  EXPECT_FALSE(IsBroadcastableWrite(FunctionCode::kReadFIFOQueue));
}
