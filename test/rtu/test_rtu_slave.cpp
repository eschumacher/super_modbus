#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
#include "super_modbus/rtu/rtu_response.hpp"
#include "super_modbus/rtu/rtu_slave.hpp"

TEST(RTUSlave, ReadHoldingRegisters) {
  using supermb::AddressSpan;
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{1};
  static constexpr AddressSpan kAddressSpan{0, 10};

  RtuSlave rtu_slave{kSlaveId};
  rtu_slave.AddHoldingRegisters(kAddressSpan);

  RtuRequest request{{kSlaveId, FunctionCode::kReadHR}};
  EXPECT_TRUE(request.SetAddressSpan(kAddressSpan));

  RtuResponse response = rtu_slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  EXPECT_EQ(response.GetData().size(), static_cast<uint32_t>(kAddressSpan.reg_count * 2));
}

TEST(RTUSlave, ReadInputRegisters) {
  using supermb::AddressSpan;
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{1};
  static constexpr AddressSpan kAddressSpan{0, 10};

  RtuSlave rtu_slave{kSlaveId};
  rtu_slave.AddInputRegisters(kAddressSpan);

  RtuRequest request{{kSlaveId, FunctionCode::kReadIR}};
  EXPECT_TRUE(request.SetAddressSpan(kAddressSpan));

  RtuResponse response = rtu_slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_EQ(response.GetData().size(), static_cast<uint32_t>(kAddressSpan.reg_count * 2));
}

TEST(RTUSlave, WriteHoldingRegisters) {
  using supermb::AddressSpan;
  using supermb::ExceptionCode;
  using supermb::FunctionCode;
  using supermb::MakeInt16;
  using supermb::RtuRequest;
  using supermb::RtuResponse;
  using supermb::RtuSlave;

  static constexpr uint8_t kSlaveId{1};
  static constexpr AddressSpan kAddressSpan{0, 1};
  static constexpr int16_t kRegisterValue{5};

  RtuSlave rtu_slave{kSlaveId};
  rtu_slave.AddHoldingRegisters(kAddressSpan);

  RtuRequest write_request{{kSlaveId, FunctionCode::kWriteSingleReg}};
  write_request.SetWriteSingleRegisterData(kAddressSpan.start_address, kRegisterValue);
  RtuResponse write_response = rtu_slave.Process(write_request);

  EXPECT_EQ(write_response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  RtuRequest read_request{{kSlaveId, FunctionCode::kReadHR}};
  read_request.SetAddressSpan(kAddressSpan);
  RtuResponse read_response = rtu_slave.Process(read_request);

  EXPECT_EQ(read_response.GetExceptionCode(), ExceptionCode::kAcknowledge);
  EXPECT_EQ(read_response.GetData().size(), static_cast<uint32_t>(kAddressSpan.reg_count * 2));

  int16_t reg_value = MakeInt16(read_response.GetData()[1], read_response.GetData()[0]);
  EXPECT_EQ(reg_value, kRegisterValue);
}
