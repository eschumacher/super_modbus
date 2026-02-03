#include "super_modbus/ascii/ascii_frame.hpp"
#include "super_modbus/ascii/ascii_master.hpp"

namespace std {
template <>
struct hash<std::pair<uint16_t, uint16_t>> {
  size_t operator()(const std::pair<uint16_t, uint16_t> &p) const noexcept {
    return std::hash<uint16_t>{}(p.first) ^ (std::hash<uint16_t>{}(p.second) << 1);
  }
};
}  // namespace std
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/common/wire_format_options.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/transport/memory_transport.hpp"
#include <array>
#include <gtest/gtest.h>
#include <tuple>
#include <vector>

using supermb::AsciiFrame;
using supermb::AsciiMaster;
using supermb::ByteOrder;
using supermb::ExceptionCode;
using supermb::FloatCountSemantics;
using supermb::FunctionCode;
using supermb::MemoryTransport;
using supermb::RtuRequest;
using supermb::RtuResponse;
using supermb::WireFormatOptions;

namespace {

void SetAsciiResponse(MemoryTransport &transport, const RtuResponse &response) {
  std::string frame = AsciiFrame::EncodeResponse(response);
  transport.SetReadData(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(frame.data()), frame.size()));
}

// Mock transport that can simulate write/flush failures (for SendRequest coverage)
class FailingTransport : public supermb::ByteTransport {
 public:
  explicit FailingTransport(bool fail_write = false, bool fail_flush = false)
      : fail_write_(fail_write),
        fail_flush_(fail_flush) {}

  int Read(std::span<uint8_t> buffer) override { return transport_.Read(buffer); }

  int Write(std::span<const uint8_t> data) override {
    if (fail_write_) {
      return 0;
    }
    return transport_.Write(data);
  }

  bool Flush() override {
    if (fail_flush_) {
      return false;
    }
    return transport_.Flush();
  }

  bool HasData() const override { return transport_.HasData(); }

  size_t AvailableBytes() const override { return transport_.AvailableBytes(); }

  void SetReadData(std::span<const uint8_t> data) { transport_.SetReadData(data); }
  void ResetReadPosition() { transport_.ResetReadPosition(); }

 private:
  MemoryTransport transport_;
  bool fail_write_;
  bool fail_flush_;
};

}  // namespace

TEST(AsciiMasterCoverage, ReadInputRegisters) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kReadIR);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(4);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0A);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x14);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  auto regs = master.ReadInputRegisters(kSlaveId, 0, 2);
  ASSERT_TRUE(regs.has_value());
  EXPECT_EQ(regs->size(), 2u);
  EXPECT_EQ(regs->at(0), 10);
  EXPECT_EQ(regs->at(1), 20);
}

TEST(AsciiMasterCoverage, ReadCoils) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kReadCoils);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);
  response.EmplaceBack(0xAB);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  auto coils = master.ReadCoils(kSlaveId, 0, 8);
  ASSERT_TRUE(coils.has_value());
  EXPECT_EQ(coils->size(), 8u);
}

TEST(AsciiMasterCoverage, ReadDiscreteInputs) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kReadDI);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);
  response.EmplaceBack(0x0F);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  auto dis = master.ReadDiscreteInputs(kSlaveId, 0, 8);
  ASSERT_TRUE(dis.has_value());
  EXPECT_EQ(dis->size(), 8u);
}

TEST(AsciiMasterCoverage, WriteSingleRegister) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kWriteSingleReg);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  EXPECT_TRUE(master.WriteSingleRegister(kSlaveId, 0, 0x1234));
}

TEST(AsciiMasterCoverage, WriteSingleCoil) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kWriteSingleCoil);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0xFF);
  response.EmplaceBack(0x00);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  EXPECT_TRUE(master.WriteSingleCoil(kSlaveId, 0, true));
}

TEST(AsciiMasterCoverage, WriteMultipleRegisters) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kWriteMultRegs);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x03);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  std::vector<int16_t> vals{100, 200, 300};
  EXPECT_TRUE(master.WriteMultipleRegisters(kSlaveId, 0, vals));
}

TEST(AsciiMasterCoverage, WriteMultipleCoils) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kWriteMultCoils);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x08);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  std::array<bool, 8> coils{true, false, true, false, true, false, true, false};
  EXPECT_TRUE(master.WriteMultipleCoils(kSlaveId, 0, coils));
}

TEST(AsciiMasterCoverage, ReadExceptionStatus) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kReadExceptionStatus);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x05);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  auto status = master.ReadExceptionStatus(kSlaveId);
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(*status, 0x05);
}

TEST(AsciiMasterCoverage, Diagnostics) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kDiagnostics);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x02);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  std::vector<uint8_t> data{0x01, 0x02};
  auto result = master.Diagnostics(kSlaveId, 0x0000, data);
  ASSERT_TRUE(result.has_value());
}

TEST(AsciiMasterCoverage, GetComEventCounter) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kGetComEventCounter);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  auto pair = master.GetComEventCounter(kSlaveId);
  ASSERT_TRUE(pair.has_value());
  EXPECT_EQ(pair->first, 0x00);
  EXPECT_EQ(pair->second, 0x1234);
}

TEST(AsciiMasterCoverage, GetComEventLog) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kGetComEventLog);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  auto data = master.GetComEventLog(kSlaveId);
  ASSERT_TRUE(data.has_value());
}

TEST(AsciiMasterCoverage, ReportSlaveID) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kReportSlaveID);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0xFF);
  response.EmplaceBack(0x01);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  auto data = master.ReportSlaveID(kSlaveId);
  ASSERT_TRUE(data.has_value());
}

TEST(AsciiMasterCoverage, MaskWriteRegister) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kMaskWriteReg);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x10);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0F);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x05);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  EXPECT_TRUE(master.MaskWriteRegister(kSlaveId, 16, 0x000F, 0x0005));
}

TEST(AsciiMasterCoverage, ReadWriteMultipleRegisters) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kReadWriteMultRegs);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);
  response.EmplaceBack(0x56);
  response.EmplaceBack(0x78);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  std::vector<int16_t> write_vals{100};
  auto regs = master.ReadWriteMultipleRegisters(kSlaveId, 0, 2, 10, write_vals);
  ASSERT_TRUE(regs.has_value());
  EXPECT_EQ(regs->size(), 2u);
}

TEST(AsciiMasterCoverage, ReadFIFOQueue) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kReadFIFOQueue);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);  // byte_count high
  response.EmplaceBack(0x06);  // byte_count low (2 + 2*2 = 6)
  response.EmplaceBack(0x00);  // fifo_count high
  response.EmplaceBack(0x02);  // fifo_count low (2 entries)
  response.EmplaceBack(0x11);  // value 0 high
  response.EmplaceBack(0x11);  // value 0 low (0x1111)
  response.EmplaceBack(0x22);  // value 1 high
  response.EmplaceBack(0x22);  // value 1 low (0x2222)
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  auto fifo = master.ReadFIFOQueue(kSlaveId, 0);
  ASSERT_TRUE(fifo.has_value());
  EXPECT_EQ(fifo->size(), 2u);
  EXPECT_EQ(fifo->at(0), 0x1111);
  EXPECT_EQ(fifo->at(1), 0x2222);
}

TEST(AsciiMasterCoverage, ReadFileRecord) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kReadFileRecord);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);
  response.EmplaceBack(0x06);
  response.EmplaceBack(0x06);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);
  response.EmplaceBack(0x56);
  response.EmplaceBack(0x78);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> recs;
  recs.emplace_back(1, 0, 2);
  auto result = master.ReadFileRecord(kSlaveId, recs);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1u);
}

TEST(AsciiMasterCoverage, WriteFileRecord) {
  constexpr uint8_t kSlaveId = 1;
  MemoryTransport transport;
  RtuResponse response(kSlaveId, FunctionCode::kWriteFileRecord);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  SetAsciiResponse(transport, response);

  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> recs;
  recs.emplace_back(1, 0, std::vector<int16_t>{0x1234, 0x5678});
  EXPECT_TRUE(master.WriteFileRecord(kSlaveId, recs));
}

TEST(AsciiMasterCoverage, SendRequest_NoResponse) {
  MemoryTransport transport(0);
  AsciiMaster master(transport);
  RtuRequest request({1, FunctionCode::kReadHR});
  request.SetAddressSpan({0, 1});
  auto result = master.SendRequest(request, 10);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, WriteFloats_Empty) {
  MemoryTransport transport;
  AsciiMaster master(transport);
  std::vector<float> empty;
  EXPECT_TRUE(master.WriteFloats(1, 0, empty));
}

// --- SendRequest failure paths ---
TEST(AsciiMasterCoverage, SendRequest_WriteFailure) {
  FailingTransport transport(true, false);
  AsciiMaster master(transport);
  RtuRequest request({1, FunctionCode::kReadHR});
  request.SetAddressSpan({0, 1});
  auto result = master.SendRequest(request, 10);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, SendRequest_FlushFailure) {
  FailingTransport transport(false, true);
  AsciiMaster master(transport);
  RtuRequest request({1, FunctionCode::kReadHR});
  request.SetAddressSpan({0, 1});
  auto result = master.SendRequest(request, 10);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, SendRequest_BroadcastWriteReturnsAck) {
  MemoryTransport transport;
  AsciiMaster master(transport);
  RtuRequest request({0, FunctionCode::kWriteSingleReg});
  request.SetWriteSingleRegisterData(0, 0x1234);
  auto result = master.SendRequest(request, 10);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetSlaveId(), 0);
  EXPECT_EQ(result->GetExceptionCode(), ExceptionCode::kAcknowledge);
}

TEST(AsciiMasterCoverage, SendRequest_DecodeFailureReturnsEmpty) {
  MemoryTransport transport;
  transport.SetReadData(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(":INVALIDHEX\r\n"), 13));
  AsciiMaster master(transport);
  RtuRequest request({1, FunctionCode::kReadHR});
  request.SetAddressSpan({0, 1});
  auto result = master.SendRequest(request, 100);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, SendRequest_SlaveIdMismatchReturnsEmpty) {
  MemoryTransport transport;
  RtuResponse response(2, FunctionCode::kReadHR);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0A);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  RtuRequest request({1, FunctionCode::kReadHR});
  request.SetAddressSpan({0, 1});
  auto result = master.SendRequest(request, 100);
  EXPECT_FALSE(result.has_value());
}

// --- ReadHoldingRegisters failure paths ---
TEST(AsciiMasterCoverage, ReadHoldingRegisters_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadHR);
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadHoldingRegisters(1, 0, 2);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadHoldingRegisters_InsufficientData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadHR);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x00);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadHoldingRegisters(1, 0, 2);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadHoldingRegisters_Success) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadHR);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(4);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0A);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x14);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadHoldingRegisters(1, 0, 2);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);
  EXPECT_EQ(result->at(0), 10);
  EXPECT_EQ(result->at(1), 20);
}

// --- ReadFloats ---
TEST(AsciiMasterCoverage, ReadFloats_CountIsRegisterCount) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadHR);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(8);  // byte count for 4 registers
  response.EmplaceBack(0x40);
  response.EmplaceBack(0x48);
  response.EmplaceBack(0xF5);
  response.EmplaceBack(0xC3);
  response.EmplaceBack(0x40);
  response.EmplaceBack(0x49);
  response.EmplaceBack(0x0F);
  response.EmplaceBack(0xDB);
  SetAsciiResponse(transport, response);
  WireFormatOptions opts;
  opts.float_count_semantics = FloatCountSemantics::CountIsRegisterCount;
  AsciiMaster master(transport, opts);
  auto result = master.ReadFloats(1, 0, 4);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);
}

TEST(AsciiMasterCoverage, ReadFloats_NumFloatsZero) {
  MemoryTransport transport;
  WireFormatOptions opts;
  opts.float_count_semantics = FloatCountSemantics::CountIsRegisterCount;
  AsciiMaster master(transport, opts);
  auto result = master.ReadFloats(1, 0, 1);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadFloats_FloatRangeOutOfRange) {
  MemoryTransport transport;
  WireFormatOptions opts;
  opts.float_range = {10, 4};
  AsciiMaster master(transport, opts);
  auto result = master.ReadFloats(1, 0, 2);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadFloats_FloatRangeExceedsEnd) {
  MemoryTransport transport;
  WireFormatOptions opts;
  opts.float_range = {0, 2};
  AsciiMaster master(transport, opts);
  auto result = master.ReadFloats(1, 0, 2);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, WriteFloats_FloatRangeOutOfRange) {
  MemoryTransport transport;
  WireFormatOptions opts;
  opts.float_range = {10, 4};
  AsciiMaster master(transport, opts);
  std::vector<float> vals{1.0f};
  EXPECT_FALSE(master.WriteFloats(1, 0, vals));
}

TEST(AsciiMasterCoverage, WriteFloats_FloatRangeExceedsEnd) {
  MemoryTransport transport;
  WireFormatOptions opts;
  opts.float_range = {0, 2};
  AsciiMaster master(transport, opts);
  std::vector<float> vals{1.0f, 2.0f};
  EXPECT_FALSE(master.WriteFloats(1, 0, vals));
}

// --- ReadInputRegisters failure paths ---
TEST(AsciiMasterCoverage, ReadInputRegisters_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadIR);
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadInputRegisters(1, 0, 2);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadInputRegisters_InsufficientData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadIR);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0x00);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadInputRegisters(1, 0, 2);
  EXPECT_FALSE(result.has_value());
}

// --- ReadCoils failure paths ---
TEST(AsciiMasterCoverage, ReadCoils_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadCoils);
  response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadCoils(1, 0, 8);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadCoils_EmptyData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadCoils);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadCoils(1, 0, 8);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadCoils_WrongByteCount) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadCoils);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(2);
  response.EmplaceBack(0xFF);
  response.EmplaceBack(0xFF);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadCoils(1, 0, 8);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadCoils_ByteIndexOutOfBounds) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadCoils);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);
  response.EmplaceBack(0xFF);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadCoils(1, 0, 10);
  EXPECT_FALSE(result.has_value());
}

// --- ReadDiscreteInputs failure paths ---
TEST(AsciiMasterCoverage, ReadDiscreteInputs_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadDI);
  response.SetExceptionCode(ExceptionCode::kServerDeviceFailure);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadDiscreteInputs(1, 0, 8);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadDiscreteInputs_EmptyData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadDI);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadDiscreteInputs(1, 0, 8);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadDiscreteInputs_ByteIndexOutOfBounds) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadDI);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(1);
  response.EmplaceBack(0xFF);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadDiscreteInputs(1, 0, 10);
  EXPECT_FALSE(result.has_value());
}

// --- Write failure paths ---
TEST(AsciiMasterCoverage, WriteSingleRegister_Exception) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kWriteSingleReg);
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  EXPECT_FALSE(master.WriteSingleRegister(1, 0, 0x1234));
}

TEST(AsciiMasterCoverage, WriteSingleCoil_Exception) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kWriteSingleCoil);
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  EXPECT_FALSE(master.WriteSingleCoil(1, 0, true));
}

TEST(AsciiMasterCoverage, WriteMultipleRegisters_Exception) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kWriteMultRegs);
  response.SetExceptionCode(ExceptionCode::kServerDeviceBusy);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<int16_t> vals{100};
  EXPECT_FALSE(master.WriteMultipleRegisters(1, 0, vals));
}

TEST(AsciiMasterCoverage, WriteMultipleCoils_Exception) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kWriteMultCoils);
  response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::array<bool, 4> coils{true, false, true, false};
  EXPECT_FALSE(master.WriteMultipleCoils(1, 0, coils));
}

// --- ReadExceptionStatus ---
TEST(AsciiMasterCoverage, ReadExceptionStatus_EmptyData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadExceptionStatus);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadExceptionStatus(1);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadExceptionStatus_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadExceptionStatus);
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadExceptionStatus(1);
  EXPECT_FALSE(result.has_value());
}

// --- Diagnostics ---
TEST(AsciiMasterCoverage, Diagnostics_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kDiagnostics);
  response.SetExceptionCode(ExceptionCode::kServerDeviceFailure);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<uint8_t> data{0x01};
  auto result = master.Diagnostics(1, 0x0000, data);
  EXPECT_FALSE(result.has_value());
}

// --- GetComEventCounter ---
TEST(AsciiMasterCoverage, GetComEventCounter_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kGetComEventCounter);
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.GetComEventCounter(1);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, GetComEventCounter_InsufficientData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kGetComEventCounter);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x12);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.GetComEventCounter(1);
  EXPECT_FALSE(result.has_value());
}

// --- GetComEventLog, ReportSlaveID ---
TEST(AsciiMasterCoverage, GetComEventLog_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kGetComEventLog);
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.GetComEventLog(1);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReportSlaveID_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReportSlaveID);
  response.SetExceptionCode(ExceptionCode::kServerDeviceFailure);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReportSlaveID(1);
  EXPECT_FALSE(result.has_value());
}

// --- MaskWriteRegister ---
TEST(AsciiMasterCoverage, MaskWriteRegister_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kMaskWriteReg);
  response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  EXPECT_FALSE(master.MaskWriteRegister(1, 16, 0x000F, 0x0005));
}

TEST(AsciiMasterCoverage, MaskWriteRegister_InsufficientData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kMaskWriteReg);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x10);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0F);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  EXPECT_FALSE(master.MaskWriteRegister(1, 16, 0x000F, 0x0005));
}

TEST(AsciiMasterCoverage, MaskWriteRegister_DataMismatch) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kMaskWriteReg);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x20);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x0F);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x05);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  EXPECT_FALSE(master.MaskWriteRegister(1, 16, 0x000F, 0x0005));
}

// --- ReadWriteMultipleRegisters ---
TEST(AsciiMasterCoverage, ReadWriteMultipleRegisters_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadWriteMultRegs);
  response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<int16_t> write_vals{100};
  auto result = master.ReadWriteMultipleRegisters(1, 0, 2, 10, write_vals);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadWriteMultipleRegisters_InsufficientData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadWriteMultRegs);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<int16_t> write_vals{100};
  auto result = master.ReadWriteMultipleRegisters(1, 0, 3, 10, write_vals);
  EXPECT_FALSE(result.has_value());
}

// --- ReadFIFOQueue ---
TEST(AsciiMasterCoverage, ReadFIFOQueue_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadFIFOQueue);
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadFIFOQueue(1, 0);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadFIFOQueue_InsufficientHeader) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadFIFOQueue);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x02);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadFIFOQueue(1, 0);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadFIFOQueue_InsufficientFifoData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadFIFOQueue);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x06);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x03);
  response.EmplaceBack(0x11);
  response.EmplaceBack(0x11);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  auto result = master.ReadFIFOQueue(1, 0);
  EXPECT_FALSE(result.has_value());
}

// --- ReadFileRecord ---
TEST(AsciiMasterCoverage, ReadFileRecord_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadFileRecord);
  response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> recs{{1, 0, 2}};
  auto result = master.ReadFileRecord(1, recs);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadFileRecord_EmptyData) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadFileRecord);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> recs{{1, 0, 2}};
  auto result = master.ReadFileRecord(1, recs);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadFileRecord_InsufficientForLength) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadFileRecord);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(20);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> recs{{1, 0, 2}};
  auto result = master.ReadFileRecord(1, recs);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadFileRecord_InvalidReferenceType) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kReadFileRecord);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
  response.EmplaceBack(10);
  response.EmplaceBack(0x05);
  response.EmplaceBack(0x06);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x01);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x00);
  response.EmplaceBack(0x12);
  response.EmplaceBack(0x34);
  response.EmplaceBack(0x56);
  response.EmplaceBack(0x78);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> recs{{1, 0, 2}};
  auto result = master.ReadFileRecord(1, recs);
  EXPECT_FALSE(result.has_value());
}

TEST(AsciiMasterCoverage, ReadFileRecord_EmptyRecordsFailsSetData) {
  MemoryTransport transport;
  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> recs;
  auto result = master.ReadFileRecord(1, recs);
  EXPECT_FALSE(result.has_value());
}

// --- WriteFileRecord ---
TEST(AsciiMasterCoverage, WriteFileRecord_ExceptionNotAck) {
  MemoryTransport transport;
  RtuResponse response(1, FunctionCode::kWriteFileRecord);
  response.SetExceptionCode(ExceptionCode::kIllegalFunction);
  SetAsciiResponse(transport, response);
  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> recs;
  recs.emplace_back(1, 0, std::vector<int16_t>{0x1234});
  EXPECT_FALSE(master.WriteFileRecord(1, recs));
}

TEST(AsciiMasterCoverage, WriteFileRecord_EmptyRecordsFailsSetData) {
  MemoryTransport transport;
  AsciiMaster master(transport);
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> recs;
  EXPECT_FALSE(master.WriteFileRecord(1, recs));
}
