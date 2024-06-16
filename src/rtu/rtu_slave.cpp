#include <limits>
#include <vector>
#include "common/address_map.hpp"
#include "common/byte_helpers.hpp"
#include "rtu/rtu_request.hpp"
#include "rtu/rtu_response.hpp"
#include "rtu/rtu_slave.hpp"
#include "common/exception_code.hpp"

namespace supermb {

RtuResponse RtuSlave::Process(RtuRequest const &request) {
  RtuResponse response{request.GetSlaveId(), request.GetFunctionCode()};
  switch (request.GetFunctionCode()) {
    case FunctionCode::kReadHR: {
      ProcessReadRegisters(holding_registers_, request, response);
      break;
    }
    case FunctionCode::kReadIR: {
      ProcessReadRegisters(input_registers_, request, response);
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

void RtuSlave::AddInputRegisters(AddressSpan span) {
  input_registers_.AddAddressSpan(span);
}

void RtuSlave::ProcessReadRegisters(AddressMap<int16_t> const &address_map, RtuRequest const &request,
                                    RtuResponse &response) {
  bool exception_hit = false;
  for (int i = 0; i < request.GetAddressSpan().reg_count; ++i) {
    auto reg_value = address_map[request.GetAddressSpan().start_address + i];
    if (reg_value.has_value()) {
      response.EmplaceBack(GetLowByte(reg_value.value()));
      response.EmplaceBack(GetHighByte(reg_value.value()));
    } else {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      exception_hit = true;
    }
  }

  if (!exception_hit) {
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  }
}

}  // namespace supermb
