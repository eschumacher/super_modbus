#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/tcp/tcp_request.hpp"

using supermb::AddressSpan;
using supermb::FunctionCode;
using supermb::TcpRequest;

TEST(TCPRequestEdgeCases, GetAddressSpanInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadExceptionStatus}};
  request.SetRawData({0x00, 0x01, 0x00, 0x05});

  auto span = request.GetAddressSpan();
  EXPECT_FALSE(span.has_value());  // ReadExceptionStatus doesn't use address span
}

TEST(TCPRequestEdgeCases, GetAddressSpanInsufficientData) {
  TcpRequest request{{FunctionCode::kReadHR}};
  request.SetRawData({0x00, 0x01});  // Only 2 bytes, need 4

  auto span = request.GetAddressSpan();
  EXPECT_FALSE(span.has_value());
}

TEST(TCPRequestEdgeCases, SetAddressSpanInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadExceptionStatus}};
  AddressSpan span{0, 10};

  // This should assert or return false
  // Since we can't test assertions easily, we'll just verify the function exists
  bool result = request.SetAddressSpan(span);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetWriteSingleRegisterDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};

  // This should assert or return false
  bool result = request.SetWriteSingleRegisterData(0, 100);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetWriteSingleCoilDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};

  // This should assert or return false
  bool result = request.SetWriteSingleCoilData(0, true);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetWriteMultipleRegistersDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};
  std::vector<int16_t> values{100, 200};

  // This should assert or return false
  bool result = request.SetWriteMultipleRegistersData(0, 2, values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetWriteMultipleRegistersDataCountMismatch) {
  TcpRequest request{{FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values{100, 200, 300};

  // Count is 2 but values.size() is 3
  bool result = request.SetWriteMultipleRegistersData(0, 2, values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetWriteMultipleCoilsDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};
  std::vector<bool> values{true, false};

  // This should assert or return false
  bool result = request.SetWriteMultipleCoilsData(0, 2, values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetWriteMultipleCoilsDataCountMismatch) {
  TcpRequest request{{FunctionCode::kWriteMultCoils}};
  std::vector<bool> values{true, false, true};

  // Count is 2 but values.size() is 3
  bool result = request.SetWriteMultipleCoilsData(0, 2, values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetDiagnosticsDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};
  std::vector<uint8_t> data{0x12, 0x34};

  // This should assert or return false
  bool result = request.SetDiagnosticsData(0x0001, data);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetMaskWriteRegisterDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};

  // This should assert or return false
  bool result = request.SetMaskWriteRegisterData(0, 0xFF00, 0x00FF);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetReadWriteMultipleRegistersDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};
  std::vector<int16_t> write_values{100, 200};

  // This should assert or return false
  bool result = request.SetReadWriteMultipleRegistersData(0, 3, 10, 2, write_values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetReadWriteMultipleRegistersDataCountMismatch) {
  TcpRequest request{{FunctionCode::kReadWriteMultRegs}};
  std::vector<int16_t> write_values{100, 200, 300};

  // Write count is 2 but write_values.size() is 3
  bool result = request.SetReadWriteMultipleRegistersData(0, 3, 10, 2, write_values);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetReadFIFOQueueDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};

  // This should assert or return false
  bool result = request.SetReadFIFOQueueData(0x1234);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetReadFileRecordDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records{{1, 0, 5}};

  // This should assert or return false
  bool result = request.SetReadFileRecordData(file_records);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetReadFileRecordDataEmpty) {
  TcpRequest request{{FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records;

  bool result = request.SetReadFileRecordData(file_records);
  EXPECT_FALSE(result);  // Empty file records should fail
}

TEST(TCPRequestEdgeCases, SetReadFileRecordDataTooMany) {
  TcpRequest request{{FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records;
  // Add 256 records (max is 255)
  for (int i = 0; i < 256; ++i) {
    file_records.emplace_back(1, i, 5);
  }

  bool result = request.SetReadFileRecordData(file_records);
  EXPECT_FALSE(result);  // Too many file records should fail
}

TEST(TCPRequestEdgeCases, SetWriteFileRecordDataInvalidFunction) {
  TcpRequest request{{FunctionCode::kReadHR}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records{{1, 0, {100, 200, 300}}};

  // This should assert or return false
  bool result = request.SetWriteFileRecordData(file_records);
  EXPECT_FALSE(result);
}

TEST(TCPRequestEdgeCases, SetWriteFileRecordDataEmpty) {
  TcpRequest request{{FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records;

  bool result = request.SetWriteFileRecordData(file_records);
  EXPECT_FALSE(result);  // Empty file records should fail
}

TEST(TCPRequestEdgeCases, SetWriteFileRecordDataTooMany) {
  TcpRequest request{{FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records;
  // Add 256 records (max is 255)
  for (int i = 0; i < 256; ++i) {
    file_records.emplace_back(1, i, std::vector<int16_t>{100, 200});
  }

  bool result = request.SetWriteFileRecordData(file_records);
  EXPECT_FALSE(result);  // Too many file records should fail
}

TEST(TCPRequestEdgeCases, SetRawData) {
  TcpRequest request{{FunctionCode::kReadHR}};
  std::vector<uint8_t> data{0x00, 0x01, 0x00, 0x05};

  request.SetRawData(data);
  EXPECT_EQ(request.GetData().size(), 4);
  EXPECT_EQ(request.GetData()[0], 0x00);
  EXPECT_EQ(request.GetData()[1], 0x01);
  EXPECT_EQ(request.GetData()[2], 0x00);
  EXPECT_EQ(request.GetData()[3], 0x05);
}

TEST(TCPRequestEdgeCases, SetRawDataSpan) {
  TcpRequest request{{FunctionCode::kReadHR}};
  std::vector<uint8_t> data{0x00, 0x01, 0x00, 0x05};

  request.SetRawData(std::span<const uint8_t>(data.data(), data.size()));
  EXPECT_EQ(request.GetData().size(), 4);
}

TEST(TCPRequestEdgeCases, ValidAddressSpan) {
  TcpRequest request{{FunctionCode::kReadHR}};
  AddressSpan span{100, 50};

  bool result = request.SetAddressSpan(span);
  EXPECT_TRUE(result);

  auto retrieved_span = request.GetAddressSpan();
  ASSERT_TRUE(retrieved_span.has_value());
  EXPECT_EQ(retrieved_span->start_address, 100);
  EXPECT_EQ(retrieved_span->reg_count, 50);
}

TEST(TCPRequestEdgeCases, ValidWriteSingleRegister) {
  TcpRequest request{{FunctionCode::kWriteSingleReg}};
  bool result = request.SetWriteSingleRegisterData(50, 1234);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);
}

TEST(TCPRequestEdgeCases, ValidWriteSingleCoil) {
  TcpRequest request{{FunctionCode::kWriteSingleCoil}};
  bool result = request.SetWriteSingleCoilData(10, true);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);
  // Verify coil value is 0xFF00 for ON
  uint16_t value = supermb::MakeInt16(data[3], data[2]);
  EXPECT_EQ(value, supermb::kCoilOnValue);
}

TEST(TCPRequestEdgeCases, ValidWriteSingleCoilOff) {
  TcpRequest request{{FunctionCode::kWriteSingleCoil}};
  bool result = request.SetWriteSingleCoilData(10, false);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);
  // Verify coil value is 0x0000 for OFF
  uint16_t value = supermb::MakeInt16(data[3], data[2]);
  EXPECT_EQ(value, 0x0000);
}

TEST(TCPRequestEdgeCases, ValidWriteMultipleRegisters) {
  TcpRequest request{{FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values{100, 200, 300, 400};

  bool result = request.SetWriteMultipleRegistersData(0, 4, values);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Address (2) + Count (2) + Byte count (1) + Values (4 * 2) = 13 bytes
  EXPECT_EQ(data.size(), 13);
}

TEST(TCPRequestEdgeCases, ValidWriteMultipleCoils) {
  TcpRequest request{{FunctionCode::kWriteMultCoils}};
  std::vector<bool> values{true, false, true, false, true, false, true, false};

  bool result = request.SetWriteMultipleCoilsData(0, 8, values);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Address (2) + Count (2) + Byte count (1) + Coil data (1) = 6 bytes
  EXPECT_EQ(data.size(), 6);
}

TEST(TCPRequestEdgeCases, ValidReadWriteMultipleRegisters) {
  TcpRequest request{{FunctionCode::kReadWriteMultRegs}};
  std::vector<int16_t> write_values{100, 200};

  bool result = request.SetReadWriteMultipleRegistersData(0, 3, 10, 2, write_values);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Read start (2) + Read count (2) + Write start (2) + Write count (2) + Byte count (1) + Write values (2 * 2) = 13
  // bytes
  EXPECT_EQ(data.size(), 13);
}

TEST(TCPRequestEdgeCases, ValidReadFIFOQueue) {
  TcpRequest request{{FunctionCode::kReadFIFOQueue}};
  bool result = request.SetReadFIFOQueueData(0x1234);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 2);
}

TEST(TCPRequestEdgeCases, ValidReadFileRecord) {
  TcpRequest request{{FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> file_records{{1, 0, 5}, {1, 1, 10}, {2, 0, 3}};

  bool result = request.SetReadFileRecordData(file_records);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Byte count (1) + 3 records * 6 bytes each = 19 bytes
  EXPECT_EQ(data.size(), 19);
}

TEST(TCPRequestEdgeCases, ValidWriteFileRecord) {
  TcpRequest request{{FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records{{1, 0, {100, 200, 300}},
                                                                                 {1, 1, {400, 500}}};

  bool result = request.SetWriteFileRecordData(file_records);
  EXPECT_TRUE(result);

  auto data = request.GetData();
  // Should have byte count + record data
  EXPECT_GT(data.size(), 1);
}
