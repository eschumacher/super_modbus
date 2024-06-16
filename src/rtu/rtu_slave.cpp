#include <limits>
#include <vector>
#include "rtu/rtu_response.hpp"
#include "rtu/rtu_slave.hpp"
#include "common/exception_code.hpp"

namespace supermb {

RtuResponse RtuSlave::Process(RtuRequest const &request) {
  RtuResponse response{request.GetSlaveId(), request.GetFunctionCode()};
  switch (request.GetFunctionCode()) {
    case FunctionCode::kReadHR: {
      std::vector<uint8_t> data{};
      for (int i = 0; i < request.GetAddressSpan().reg_count; ++i) {
        auto reg_value = holding_registers_[request.GetAddressSpan().start_address + i];
        if (reg_value.has_value()) {
          data.emplace_back(reg_value.value() & 0xFF);
          data.emplace_back((reg_value.value() >> 8) & 0xFF);
        } else {
          response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
          return response;
        }
      }

      response.SetExceptionCode(ExceptionCode::kAcknowledge);
      response.SetData(data);
      break;
    }
    default: {
      response.SetExceptionCode(ExceptionCode::kIllegalFunction);
      break;
    }
  }

  return response;
}

void RtuSlave::AddHoldingRegisters(AddressSpan span) {
  holding_registers_.AddAddressSpan(span);
}

}  // namespace supermb
