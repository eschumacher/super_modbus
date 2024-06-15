#include <gtest/gtest.h>
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/rtu/rtu_request.hpp"
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

  RtuRequest request{kSlaveId, FunctionCode::kReadHR, kAddressSpan};
  RtuResponse response = rtu_slave.Process(request);

  EXPECT_EQ(response.GetExceptionCode(), ExceptionCode::kAcknowledge);

  EXPECT_EQ(response.GetData().size(),
            static_cast<uint32_t>(kAddressSpan.reg_count_ * 2));
}
