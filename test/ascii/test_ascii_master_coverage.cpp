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
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/exception_code.hpp"
#include "super_modbus/common/function_code.hpp"
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
using supermb::FunctionCode;
using supermb::MemoryTransport;
using supermb::RtuRequest;
using supermb::RtuResponse;

namespace {

void SetAsciiResponse(MemoryTransport &transport, const RtuResponse &response) {
  std::string frame = AsciiFrame::EncodeResponse(response);
  transport.SetReadData(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(frame.data()), frame.size()));
}

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
