#include <array>
#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/tcp/tcp_request.hpp"

using supermb::AddressSpan;
using supermb::FunctionCode;
using supermb::TcpRequest;

TEST(TCPRequestEdgeCases, GetAddressSpanInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadExceptionStatus}};
  request.SetRawData({0x00, 0x01, 0x00, 0x05});

  auto span = request.GetAddressSpan();
  EXPECT_FALSE(span.has_value());  // ReadExceptionStatus doesn't use address span
}

TEST(TCPRequestEdgeCases, GetAddressSpanInsufficientData) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  request.SetRawData({0x00, 0x01});  // Only 2 bytes, need 4

  auto span = request.GetAddressSpan();
  EXPECT_FALSE(span.has_value());
}

TEST(TCPRequestEdgeCases, SetAddressSpanInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadExceptionStatus}};
  AddressSpan span{0, 10};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetAddressSpan(span), ".*");
}

TEST(TCPRequestEdgeCases, SetWriteSingleRegisterDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetWriteSingleRegisterData(0, 100), ".*");
}

TEST(TCPRequestEdgeCases, SetWriteSingleCoilDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetWriteSingleCoilData(0, true), ".*");
}

TEST(TCPRequestEdgeCases, SetWriteMultipleRegistersDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  std::vector<int16_t> values{100, 200};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetWriteMultipleRegistersData(0, 2, values), ".*");
}

TEST(TCPRequestEdgeCases, SetWriteMultipleRegistersDataCountMismatch) {
  TcpRequest request{{0, 1, FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values{100, 200, 300};

  // Count is 2 but values.size() is 3
  bool result = request.SetWriteMultipleRegistersData(0, 2, values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetWriteMultipleCoilsDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  std::array<bool, 2> values{true, false};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetWriteMultipleCoilsData(0, 2, values), ".*");
}

TEST(TCPRequestEdgeCases, SetWriteMultipleCoilsDataCountMismatch) {
  TcpRequest request{{0, 1, FunctionCode::kWriteMultCoils}};
  std::array<bool, 3> values{true, false, true};

  // Count is 2 but values.size() is 3
  bool result = request.SetWriteMultipleCoilsData(0, 2, values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetDiagnosticsDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  std::vector<uint8_t> data{0x12, 0x34};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetDiagnosticsData(0x0001, data), ".*");
}

TEST(TCPRequestEdgeCases, SetMaskWriteRegisterDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetMaskWriteRegisterData(0, 0xFF00, 0x00FF), ".*");
}

TEST(TCPRequestEdgeCases, SetReadWriteMultipleRegistersDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  std::vector<int16_t> write_values{100, 200};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetReadWriteMultipleRegistersData(0, 3, 10, 2, write_values), ".*");
}

TEST(TCPRequestEdgeCases, SetReadWriteMultipleRegistersDataCountMismatch) {
  TcpRequest request{{0, 1, FunctionCode::kReadWriteMultRegs}};
  std::vector<int16_t> write_values{100, 200, 300};

  // Write count is 2 but write_values.size() is 3
  bool result = request.SetReadWriteMultipleRegistersData(0, 3, 10, 2, write_values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetReadFIFOQueueDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetReadFIFOQueueData(0x1234), ".*");
}

TEST(TCPRequestEdgeCases, SetReadFileRecordDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records{{1, 0, 5}};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetReadFileRecordData(file_records), ".*");
}

TEST(TCPRequestEdgeCases, SetReadFileRecordDataEmpty) {
  TcpRequest request{{0, 1, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records;

  bool result = request.SetReadFileRecordData(file_records);
  EXPECT_FALSE(result);  // Empty file records should fail
}

TEST(TCPRequestEdgeCases, SetReadFileRecordDataTooMany) {
  TcpRequest request{{0, 1, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records;
  // Add 256 records (max is 255)
  for (int i = 0; i < 256; ++i) {
    file_records.emplace_back(1, i, 5);
  }

  bool result = request.SetReadFileRecordData(file_records);
  EXPECT_FALSE(result);  // Too many file records should fail
}

TEST(TCPRequestEdgeCases, SetWriteFileRecordDataInvalidFunction) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records{{1, 0, {100, 200, 300}}};

  // This should assert in debug builds
  EXPECT_DEATH(request.SetWriteFileRecordData(file_records), ".*");
}

TEST(TCPRequestEdgeCases, SetWriteFileRecordDataEmpty) {
  TcpRequest request{{0, 1, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records;

  bool result = request.SetWriteFileRecordData(file_records);
  EXPECT_FALSE(result);  // Empty file records should fail
}

TEST(TCPRequestEdgeCases, SetWriteFileRecordDataTooMany) {
  TcpRequest request{{0, 1, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records;
  // Add 256 records (max is 255)
  for (int i = 0; i < 256; ++i) {
    file_records.emplace_back(1, i, std::vector<int16_t>{100, 200});
  }

  bool result = request.SetWriteFileRecordData(file_records);
  EXPECT_FALSE(result);  // Too many file records should fail
}

// Total size > 252 (Modbus PDU limit) triggers rejection
TEST(TCPRequestEdgeCases, SetWriteFileRecordData_TotalSizeExceedsLimit) {
  TcpRequest request{{0, 1, FunctionCode::kWriteFileRecord}};
  // One record with 123 registers: 1 + 7 + 123*2 = 254 > 252
  std::vector<int16_t> large_record(123, 0);
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records;
  file_records.emplace_back(1, 0, large_record);

  bool result = request.SetWriteFileRecordData(file_records);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetRawData) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  std::vector<uint8_t> data{0x00, 0x01, 0x00, 0x05};

  request.SetRawData(data);
  EXPECT_EQ(request.GetData().size(), 4);
  EXPECT_EQ(request.GetData()[0], 0x00);
  EXPECT_EQ(request.GetData()[1], 0x01);
  EXPECT_EQ(request.GetData()[2], 0x00);
  EXPECT_EQ(request.GetData()[3], 0x05);
}

TEST(TCPRequestEdgeCases, SetRawDataSpan) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  std::vector<uint8_t> data{0x00, 0x01, 0x00, 0x05};

  request.SetRawData(std::span<const uint8_t>(data.data(), data.size()));
  EXPECT_EQ(request.GetData().size(), 4);
}

TEST(TCPRequestEdgeCases, ValidAddressSpan) {
  TcpRequest request{{0, 1, FunctionCode::kReadHR}};
  AddressSpan span{100, 50};

  bool result = request.SetAddressSpan(span);
  EXPECT_TRUE(result);

  auto retrieved_span = request.GetAddressSpan();
  ASSERT_TRUE(retrieved_span.has_value());
  EXPECT_EQ(retrieved_span->start_address, 100);
  EXPECT_EQ(retrieved_span->reg_count, 50);
}

TEST(TCPRequestEdgeCases, ValidWriteSingleRegister) {
  TcpRequest request{{0, 1, FunctionCode::kWriteSingleReg}};
  bool result = request.SetWriteSingleRegisterData(50, 1234);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);
}

TEST(TCPRequestEdgeCases, ValidWriteSingleCoil) {
  TcpRequest request{{0, 1, FunctionCode::kWriteSingleCoil}};
  bool result = request.SetWriteSingleCoilData(10, true);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);
  // Verify coil value is 0xFF00 for ON
  uint16_t value = supermb::MakeInt16(data[3], data[2]);
  EXPECT_EQ(value, supermb::kCoilOnValue);
}

TEST(TCPRequestEdgeCases, ValidWriteSingleCoilOff) {
  TcpRequest request{{0, 1, FunctionCode::kWriteSingleCoil}};
  bool result = request.SetWriteSingleCoilData(10, false);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);
  // Verify coil value is 0x0000 for OFF
  uint16_t value = supermb::MakeInt16(data[3], data[2]);
  EXPECT_EQ(value, 0x0000);
}

TEST(TCPRequestEdgeCases, ValidWriteMultipleRegisters) {
  TcpRequest request{{0, 1, FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values{100, 200, 300, 400};

  bool result = request.SetWriteMultipleRegistersData(0, 4, values);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Address (2) + Count (2) + Byte count (1) + Values (4 * 2) = 13 bytes
  EXPECT_EQ(data.size(), 13);
}

TEST(TCPRequestEdgeCases, ValidWriteMultipleCoils) {
  TcpRequest request{{0, 1, FunctionCode::kWriteMultCoils}};
  std::array<bool, 8> values{true, false, true, false, true, false, true, false};

  bool result = request.SetWriteMultipleCoilsData(0, 8, std::span<const bool>(values));
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Address (2) + Count (2) + Byte count (1) + Coil data (1) = 6 bytes
  EXPECT_EQ(data.size(), 6);
}

TEST(TCPRequestEdgeCases, ValidReadWriteMultipleRegisters) {
  TcpRequest request{{0, 1, FunctionCode::kReadWriteMultRegs}};
  std::vector<int16_t> write_values{100, 200};

  bool result = request.SetReadWriteMultipleRegistersData(0, 3, 10, 2, write_values);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Read start (2) + Read count (2) + Write start (2) + Write count (2) + Byte count (1) + Write values (2 * 2) = 13
  // bytes
  EXPECT_EQ(data.size(), 13);
}

TEST(TCPRequestEdgeCases, ValidDiagnostics) {
  TcpRequest request{{0, 1, FunctionCode::kDiagnostics}};
  std::vector<uint8_t> data{0x00, 0x01, 0x12, 0x34};

  bool result = request.SetDiagnosticsData(0x0001, data);
  EXPECT_TRUE(result);
  EXPECT_GE(request.GetData().size(), 2);
}

TEST(TCPRequestEdgeCases, ValidMaskWriteRegister) {
  TcpRequest request{{0, 1, FunctionCode::kMaskWriteReg}};
  bool result = request.SetMaskWriteRegisterData(50, 0xFF00, 0x00FF);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 6);  // address(2) + and_mask(2) + or_mask(2)
}

TEST(TCPRequestEdgeCases, ValidReadFIFOQueue) {
  TcpRequest request{{0, 1, FunctionCode::kReadFIFOQueue}};
  bool result = request.SetReadFIFOQueueData(0x1234);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 2);
}

TEST(TCPRequestEdgeCases, ValidReadFileRecord) {
  TcpRequest request{{0, 1, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records{{1, 0, 5}, {1, 1, 10}, {2, 0, 3}};

  bool result = request.SetReadFileRecordData(file_records);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Byte count (1) + 3 records * 7 bytes each (ref_type(1) + file(2) + record(2) + length(2)) = 22 bytes
  EXPECT_EQ(data.size(), 22);
}

TEST(TCPRequestEdgeCases, ValidWriteFileRecord) {
  TcpRequest request{{0, 1, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records{{1, 0, {100, 200, 300}},
                                                                                 {1, 1, {400, 500}}};

  bool result = request.SetWriteFileRecordData(file_records);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Should have byte count + record data
  EXPECT_GT(data.size(), 1);
}
