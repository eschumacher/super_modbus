#include <array>
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

// Helper class to simulate master-slave communication
class MasterSlaveSimulator {
 public:
  MasterSlaveSimulator(uint8_t slave_id)
      : slave_(slave_id),
        master_(transport_) {}

  void SetupRegisters(AddressSpan span) { slave_.AddHoldingRegisters(span); }
  void SetupInputRegisters(AddressSpan span) { slave_.AddInputRegisters(span); }
  void SetupCoils(AddressSpan span) { slave_.AddCoils(span); }
  void SetupDiscreteInputs(AddressSpan span) { slave_.AddDiscreteInputs(span); }

  // Helper to process master request through slave and return response to master
  void ProcessMasterRequest() {
    auto master_request = transport_.GetWrittenData();
    if (master_request.empty()) {
      return;
    }
    transport_.ClearWriteBuffer();
    transport_.SetReadData(master_request);
    transport_.ResetReadPosition();
    slave_.ProcessIncomingFrame(transport_, 100);
    auto slave_response = transport_.GetWrittenData();
    transport_.ClearWriteBuffer();
    transport_.SetReadData(slave_response);
    transport_.ResetReadPosition();
  }

  void SendRequestOnly(RtuRequest const &request) {
    auto frame = RtuFrame::EncodeRequest(request);
    (void)transport_.Write(std::span<uint8_t const>(frame.data(), frame.size()));
    (void)transport_.Flush();
  }

  RtuMaster &GetMaster() { return master_; }
  RtuSlave &GetSlave() { return slave_; }
  MemoryTransport &GetTransport() { return transport_; }

 private:
  MemoryTransport transport_;
  RtuSlave slave_;
  RtuMaster master_;
};

// Test error paths in ReadHoldingRegisters
TEST(MasterErrors, ReadHoldingRegisters_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // No data in transport - should return empty
  auto result = master.ReadHoldingRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, ReadHoldingRegisters_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Request invalid address to get exception
  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({100, 5});  // Invalid address
  sim.SendRequestOnly(request);
  sim.ProcessMasterRequest();

  auto result = sim.GetMaster().ReadHoldingRegisters(kSlaveId, 100, 5);
  EXPECT_FALSE(result.has_value());  // Should return empty on exception
}

TEST(MasterErrors, ReadHoldingRegisters_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Create a response with wrong byte count
  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(5);  // Wrong byte count (should be 10 for 5 registers)
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x02);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReadHoldingRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());  // Should fail due to wrong byte count
}

TEST(MasterErrors, ReadHoldingRegisters_TooSmallData) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Create a response with insufficient data
  RtuResponse response{kSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);  // Correct byte count
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  // Missing rest of data

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReadHoldingRegisters(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());  // Should fail due to insufficient data
}

// Test error paths in ReadInputRegisters
TEST(MasterErrors, ReadInputRegisters_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupInputRegisters({0, 10});

  // Request invalid address
  auto result = sim.GetMaster().ReadInputRegisters(kSlaveId, 100, 5);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in ReadCoils
TEST(MasterErrors, ReadCoils_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupCoils({0, 10});

  // Request invalid address
  auto result = sim.GetMaster().ReadCoils(kSlaveId, 100, 5);
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, ReadCoils_WrongByteCount) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupCoils({0, 10});

  // Create response with wrong byte count
  RtuResponse response{kSlaveId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);  // Wrong byte count (should be 1 for 5 coils)
  response.EmplaceBack(0xFF);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReadCoils(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, ReadCoils_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupCoils({0, 10});

  // Create response with insufficient data
  RtuResponse response{kSlaveId, FunctionCode::kReadCoils};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);  // Correct byte count
  // Missing coil data

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReadCoils(kSlaveId, 0, 5);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in ReadDiscreteInputs
TEST(MasterErrors, ReadDiscreteInputs_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupDiscreteInputs({0, 10});

  auto result = sim.GetMaster().ReadDiscreteInputs(kSlaveId, 100, 5);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in WriteSingleRegister
TEST(MasterErrors, WriteSingleRegister_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Try to write to invalid address
  bool result = sim.GetMaster().WriteSingleRegister(kSlaveId, 100, 1234);
  EXPECT_FALSE(result);
}

// Test error paths in WriteSingleCoil
TEST(MasterErrors, WriteSingleCoil_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupCoils({0, 10});

  bool result = sim.GetMaster().WriteSingleCoil(kSlaveId, 100, true);
  EXPECT_FALSE(result);
}

// Test error paths in WriteMultipleRegisters
TEST(MasterErrors, WriteMultipleRegisters_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  std::array<int16_t, 3> values{100, 200, 300};
  bool result = sim.GetMaster().WriteMultipleRegisters(kSlaveId, 100, values);
  EXPECT_FALSE(result);
}

// Test error paths in WriteMultipleCoils
TEST(MasterErrors, WriteMultipleCoils_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupCoils({0, 10});

  std::array<bool, 3> values{true, false, true};
  bool result = sim.GetMaster().WriteMultipleCoils(kSlaveId, 100, values);
  EXPECT_FALSE(result);
}

// Test error paths in ReadExceptionStatus
TEST(MasterErrors, ReadExceptionStatus_NoData) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};

  // Create response with no data
  RtuResponse response{kSlaveId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // No data added

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReadExceptionStatus(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, ReadExceptionStatus_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};

  // Create exception response
  RtuResponse response{kSlaveId, FunctionCode::kReadExceptionStatus};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReadExceptionStatus(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in Diagnostics
TEST(MasterErrors, Diagnostics_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};

  // Create exception response
  RtuResponse response{kSlaveId, FunctionCode::kDiagnostics};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  std::vector<uint8_t> test_data{0x12, 0x34};
  auto result = sim.GetMaster().Diagnostics(kSlaveId, 0x0000, test_data);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in GetComEventCounter
TEST(MasterErrors, GetComEventCounter_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};

  // Create response with insufficient data
  RtuResponse response{kSlaveId, FunctionCode::kGetComEventCounter};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Only status, missing event count

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().GetComEventCounter(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, GetComEventCounter_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};

  RtuResponse response{kSlaveId, FunctionCode::kGetComEventCounter};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().GetComEventCounter(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in GetComEventLog
TEST(MasterErrors, GetComEventLog_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};

  RtuResponse response{kSlaveId, FunctionCode::kGetComEventLog};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().GetComEventLog(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in ReportSlaveID
TEST(MasterErrors, ReportSlaveID_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};

  RtuResponse response{kSlaveId, FunctionCode::kReportSlaveID};
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReportSlaveID(kSlaveId);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in MaskWriteRegister
TEST(MasterErrors, MaskWriteRegister_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  bool result = sim.GetMaster().MaskWriteRegister(kSlaveId, 100, 0xFFFF, 0x0000);
  EXPECT_FALSE(result);
}

TEST(MasterErrors, MaskWriteRegister_WrongResponse) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Create response with wrong values
  RtuResponse response{kSlaveId, FunctionCode::kMaskWriteReg};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  // Echo back wrong address
  response.EmplaceBack(0x00);  // Low byte of address (wrong)
  response.EmplaceBack(0x01);  // High byte of address (wrong)
  response.EmplaceBack(0xFF);  // Low byte of and_mask
  response.EmplaceBack(0xFF);  // High byte of and_mask
  response.EmplaceBack(0x00);  // Low byte of or_mask
  response.EmplaceBack(0x00);  // High byte of or_mask

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  bool result = sim.GetMaster().MaskWriteRegister(kSlaveId, 0, 0xFFFF, 0x0000);
  EXPECT_FALSE(result);  // Should fail because response doesn't match
}

TEST(MasterErrors, MaskWriteRegister_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Create response with insufficient data
  RtuResponse response{kSlaveId, FunctionCode::kMaskWriteReg};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  // Missing rest of data

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  bool result = sim.GetMaster().MaskWriteRegister(kSlaveId, 0, 0xFFFF, 0x0000);
  EXPECT_FALSE(result);
}

// Test error paths in ReadWriteMultipleRegisters
TEST(MasterErrors, ReadWriteMultipleRegisters_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  std::array<int16_t, 2> write_values{100, 200};
  auto result = sim.GetMaster().ReadWriteMultipleRegisters(kSlaveId, 100, 3, 0, write_values);
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, ReadWriteMultipleRegisters_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Create response with insufficient read data
  RtuResponse response{kSlaveId, FunctionCode::kReadWriteMultRegs};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Only one byte, need 6 for 3 registers
  response.EmplaceBack(0x01);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  std::array<int16_t, 2> write_values{100, 200};
  auto result = sim.GetMaster().ReadWriteMultipleRegisters(kSlaveId, 0, 3, 0, write_values);
  EXPECT_FALSE(result.has_value());
}

// Test error paths in ReadFIFOQueue
TEST(MasterErrors, ReadFIFOQueue_Exception) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  auto result = sim.GetMaster().ReadFIFOQueue(kSlaveId, 100);
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, ReadFIFOQueue_InsufficientData) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Create response with insufficient data
  RtuResponse response{kSlaveId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // Low byte of byte count
  response.EmplaceBack(0x02);  // High byte of byte count
  response.EmplaceBack(0x00);  // Low byte of FIFO count
  response.EmplaceBack(0x01);  // High byte of FIFO count
  // Missing FIFO data

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReadFIFOQueue(kSlaveId, 0);
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, ReadFIFOQueue_TooSmallHeader) {
  static constexpr uint8_t kSlaveId{1};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Create response with too small header
  RtuResponse response{kSlaveId, FunctionCode::kReadFIFOQueue};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x02);
  // Missing FIFO count

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReadFIFOQueue(kSlaveId, 0);
  EXPECT_FALSE(result.has_value());
}

// Test ReceiveResponse error paths
TEST(MasterErrors, ReceiveResponse_WrongSlaveID) {
  static constexpr uint8_t kSlaveId{1};
  static constexpr uint8_t kWrongSlaveId{2};
  MasterSlaveSimulator sim{kSlaveId};
  sim.SetupRegisters({0, 10});

  // Create response with wrong slave ID
  RtuResponse response{kWrongSlaveId, FunctionCode::kReadHR};
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);

  auto frame = RtuFrame::EncodeResponse(response);
  sim.GetTransport().SetReadData(frame);
  sim.GetTransport().ResetReadPosition();

  auto result = sim.GetMaster().ReceiveResponse(kSlaveId, 100);
  EXPECT_FALSE(result.has_value());  // Should fail due to wrong slave ID
}

TEST(MasterErrors, ReceiveResponse_InvalidFrame) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // Set invalid frame data (too short, no CRC)
  std::vector<uint8_t> invalid_frame{0x01, 0x03};
  transport.SetReadData(invalid_frame);
  transport.ResetReadPosition();

  auto result = master.ReceiveResponse(kSlaveId, 100);
  EXPECT_FALSE(result.has_value());  // Should fail to decode
}

// Test ReadFrame timeout (indirectly through ReceiveResponse)
TEST(MasterErrors, ReceiveResponse_Timeout) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  // No data in transport - should timeout
  auto result = master.ReceiveResponse(kSlaveId, 10);  // 10ms timeout
  EXPECT_FALSE(result.has_value());
}

// Test SendRequest error paths
TEST(MasterErrors, SendRequest_NoResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  // No response in transport
  auto result = master.SendRequest(request, 10);  // Short timeout
  EXPECT_FALSE(result.has_value());
}

TEST(MasterErrors, SendRequest_InvalidResponse) {
  static constexpr uint8_t kSlaveId{1};
  MemoryTransport transport;
  RtuMaster master{transport};

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  request.SetAddressSpan({0, 5});

  // Set invalid response data
  std::vector<uint8_t> invalid_response{0x01, 0x03};  // Too short
  transport.SetReadData(invalid_response);
  transport.ResetReadPosition();

  auto result = master.SendRequest(request, 100);
  EXPECT_FALSE(result.has_value());
}
