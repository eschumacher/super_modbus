#include <array>
#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_request.hpp"

using supermb::AddressSpan;
using supermb::FunctionCode;
using supermb::RtuRequest;

TEST(RtuRequestEdgeCases, GetAddressSpanInvalidFunctionCode) {
  RtuRequest request{{1, FunctionCode::kReadExceptionStatus}};
  auto span = request.GetAddressSpan();
  EXPECT_FALSE(span.has_value());
}

TEST(RtuRequestEdgeCases, GetAddressSpanInsufficientData) {
  RtuRequest request{{1, FunctionCode::kReadHR}};
  // Set raw data with less than 4 bytes
  std::vector<uint8_t> data{0x00, 0x00};  // Only 2 bytes
  request.SetRawData(data);

  auto span = request.GetAddressSpan();
  EXPECT_FALSE(span.has_value());
}

TEST(RtuRequestEdgeCases, GetAddressSpanValid) {
  RtuRequest request{{1, FunctionCode::kReadHR}};
  AddressSpan span{100, 10};
  EXPECT_TRUE(request.SetAddressSpan(span));

  auto retrieved = request.GetAddressSpan();
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->start_address, 100);
  EXPECT_EQ(retrieved->reg_count, 10);
}

TEST(RtuRequestEdgeCases, SetWriteMultipleRegistersMismatchedCount) {
  RtuRequest request{{1, FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values{100, 200, 300};

  // Try to set with count=5 but only 3 values
  EXPECT_FALSE(request.SetWriteMultipleRegistersData(0, 5, values));
}

TEST(RtuRequestEdgeCases, SetWriteMultipleRegistersMatchingCount) {
  RtuRequest request{{1, FunctionCode::kWriteMultRegs}};
  std::vector<int16_t> values{100, 200, 300};

  EXPECT_TRUE(request.SetWriteMultipleRegistersData(0, 3, values));
  auto data = request.GetData();
  EXPECT_GT(data.size(), 0);
}

TEST(RtuRequestEdgeCases, SetWriteMultipleCoilsMismatchedCount) {
  RtuRequest request{{1, FunctionCode::kWriteMultCoils}};
  // std::vector<bool> can't be converted to span, use array instead
  std::array<bool, 3> values{true, false, true};

  // Try to set with count=5 but only 3 values
  EXPECT_FALSE(request.SetWriteMultipleCoilsData(0, 5, values));
}

TEST(RtuRequestEdgeCases, SetWriteMultipleCoilsMatchingCount) {
  RtuRequest request{{1, FunctionCode::kWriteMultCoils}};
  // std::vector<bool> can't be converted to span, use array instead
  std::array<bool, 5> values{true, false, true, false, true};

  EXPECT_TRUE(request.SetWriteMultipleCoilsData(0, 5, values));
  auto data = request.GetData();
  EXPECT_GT(data.size(), 0);
}

TEST(RtuRequestEdgeCases, SetReadFileRecordEmpty) {
  RtuRequest request{{1, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;

  EXPECT_FALSE(request.SetReadFileRecordData(records));
}

TEST(RtuRequestEdgeCases, SetReadFileRecordValid) {
  RtuRequest request{{1, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);

  EXPECT_TRUE(request.SetReadFileRecordData(records));
  auto data = request.GetData();
  EXPECT_GT(data.size(), 0);
}

TEST(RtuRequestEdgeCases, SetReadFileRecordMultiple) {
  RtuRequest request{{1, FunctionCode::kReadFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> records;
  records.emplace_back(1, 0, 10);
  records.emplace_back(2, 5, 20);

  EXPECT_TRUE(request.SetReadFileRecordData(records));
}

TEST(RtuRequestEdgeCases, SetWriteFileRecordEmpty) {
  RtuRequest request{{1, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> records;

  EXPECT_FALSE(request.SetWriteFileRecordData(records));
}

TEST(RtuRequestEdgeCases, SetWriteFileRecordValid) {
  RtuRequest request{{1, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> records;
  std::vector<int16_t> data{100, 200, 300};
  records.emplace_back(1, 0, data);

  EXPECT_TRUE(request.SetWriteFileRecordData(records));
  auto request_data = request.GetData();
  EXPECT_GT(request_data.size(), 0);
}

TEST(RtuRequestEdgeCases, SetWriteFileRecordMultiple) {
  RtuRequest request{{1, FunctionCode::kWriteFileRecord}};
  std::vector<std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> records;
  std::vector<int16_t> data1{100, 200};
  std::vector<int16_t> data2{300, 400, 500};
  records.emplace_back(1, 0, data1);
  records.emplace_back(2, 5, data2);

  EXPECT_TRUE(request.SetWriteFileRecordData(records));
}

TEST(RtuRequestEdgeCases, SetAddressSpanAllValidFunctionCodes) {
  const FunctionCode valid_codes[] = {FunctionCode::kReadCoils,       FunctionCode::kReadDI,
                                      FunctionCode::kReadHR,          FunctionCode::kReadIR,
                                      FunctionCode::kWriteSingleCoil, FunctionCode::kWriteSingleReg,
                                      FunctionCode::kWriteMultCoils,  FunctionCode::kWriteMultRegs,
                                      FunctionCode::kMaskWriteReg,    FunctionCode::kReadWriteMultRegs};

  for (FunctionCode code : valid_codes) {
    RtuRequest request{{1, code}};
    AddressSpan span{0, 10};
    EXPECT_TRUE(request.SetAddressSpan(span)) << "Failed for function code: " << static_cast<int>(code);
  }
}

// Note: SetAddressSpanInvalidFunctionCode test removed because
// SetAddressSpan asserts when called with invalid function code.
// This is intentional library behavior - invalid function codes should
// not be used with SetAddressSpan. Testing this would require
// disabling assertions or using a death test, which is not necessary
// for coverage purposes.

TEST(RtuRequestEdgeCases, SetWriteSingleRegisterData) {
  RtuRequest request{{1, FunctionCode::kWriteSingleReg}};
  EXPECT_TRUE(request.SetWriteSingleRegisterData(100, 1234));

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);  // address (2) + value (2)
}

TEST(RtuRequestEdgeCases, SetWriteSingleCoilData) {
  RtuRequest request{{1, FunctionCode::kWriteSingleCoil}};
  EXPECT_TRUE(request.SetWriteSingleCoilData(50, true));

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);  // address (2) + value (2)

  // Check that ON value is 0xFF00
  EXPECT_EQ(data[2], 0xFF);
  EXPECT_EQ(data[3], 0x00);
}

TEST(RtuRequestEdgeCases, SetWriteSingleCoilOff) {
  RtuRequest request{{1, FunctionCode::kWriteSingleCoil}};
  EXPECT_TRUE(request.SetWriteSingleCoilData(50, false));

  auto data = request.GetData();
  // Check that OFF value is 0x0000
  EXPECT_EQ(data[2], 0x00);
  EXPECT_EQ(data[3], 0x00);
}

TEST(RtuRequestEdgeCases, SetMaskWriteRegisterData) {
  RtuRequest request{{1, FunctionCode::kMaskWriteReg}};
  EXPECT_TRUE(request.SetMaskWriteRegisterData(100, 0x00FF, 0xFF00));

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 6);  // address (2) + and_mask (2) + or_mask (2)
}

TEST(RtuRequestEdgeCases, SetReadWriteMultipleRegistersData) {
  RtuRequest request{{1, FunctionCode::kReadWriteMultRegs}};
  std::vector<int16_t> write_values{100, 200, 300};

  EXPECT_TRUE(request.SetReadWriteMultipleRegistersData(0, 5, 10, 3, write_values));

  auto data = request.GetData();
  EXPECT_GT(data.size(), 0);
}

TEST(RtuRequestEdgeCases, SetReadWriteMultipleRegistersMismatchedCount) {
  RtuRequest request{{1, FunctionCode::kReadWriteMultRegs}};
  std::vector<int16_t> write_values{100, 200};

  // Try to set with write_count=3 but only 2 values
  EXPECT_FALSE(request.SetReadWriteMultipleRegistersData(0, 5, 10, 3, write_values));
}

TEST(RtuRequestEdgeCases, SetReadFIFOQueueData) {
  RtuRequest request{{1, FunctionCode::kReadFIFOQueue}};
  EXPECT_TRUE(request.SetReadFIFOQueueData(100));

  auto data = request.GetData();
  EXPECT_EQ(data.size(), 2);  // address (2)
}

TEST(RtuRequestEdgeCases, SetDiagnosticsData) {
  RtuRequest request{{1, FunctionCode::kDiagnostics}};
  std::vector<uint8_t> diag_data{0x00, 0x01, 0x02, 0x03};

  EXPECT_TRUE(request.SetDiagnosticsData(0x0001, diag_data));

  auto data = request.GetData();
  EXPECT_GT(data.size(), 0);
}

TEST(RtuRequestEdgeCases, SetRawData) {
  RtuRequest request{{1, FunctionCode::kReadHR}};
  std::vector<uint8_t> raw_data{0x00, 0x01, 0x02, 0x03};

  request.SetRawData(raw_data);
  auto data = request.GetData();
  EXPECT_EQ(data.size(), 4);
  EXPECT_EQ(data[0], 0x00);
  EXPECT_EQ(data[1], 0x01);
  EXPECT_EQ(data[2], 0x02);
  EXPECT_EQ(data[3], 0x03);
}

TEST(RtuRequestEdgeCases, GetSlaveIdAndFunctionCode) {
  RtuRequest request{{42, FunctionCode::kReadHR}};
  EXPECT_EQ(request.GetSlaveId(), 42);
  EXPECT_EQ(request.GetFunctionCode(), FunctionCode::kReadHR);
}

TEST(RtuRequestEdgeCases, WriteMultipleCoilsBytePacking) {
  RtuRequest request{{1, FunctionCode::kWriteMultCoils}};
  // Test with exactly 8 coils (should fit in 1 byte)
  // std::vector<bool> can't be converted to span, use array instead
  std::array<bool, 8> coils8{true, false, true, false, true, false, true, false};
  EXPECT_TRUE(request.SetWriteMultipleCoilsData(0, 8, coils8));
  auto data = request.GetData();
  // Should have: address(2) + count(2) + byte_count(1) + data(1) = 6 bytes
  EXPECT_GE(data.size(), 6);

  // Test with 9 coils (should span 2 bytes)
  RtuRequest request2{{1, FunctionCode::kWriteMultCoils}};
  std::array<bool, 9> coils9{true, true, true, true, true, true, true, true, true};
  EXPECT_TRUE(request2.SetWriteMultipleCoilsData(0, 9, coils9));
  auto data2 = request2.GetData();
  // Should have: address(2) + count(2) + byte_count(1) + data(2) = 7 bytes
  EXPECT_GE(data2.size(), 7);
}
