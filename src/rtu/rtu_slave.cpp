#include <cassert>
#include <limits>
#include <optional>
#include <vector>
#include "common/address_map.hpp"
#include "common/byte_helpers.hpp"
#include "common/function_code.hpp"
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
    case FunctionCode::kWriteSingleReg: {
      ProcessWriteSingleRegister(holding_registers_, request, response);
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
  auto const maybe_address_span = request.GetAddressSpan();
  if (!maybe_address_span.has_value()) {
    assert(false);  // likely a library defect if hit - create ticket in github
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  AddressSpan const &address_span = maybe_address_span.value();
  for (int i = 0; i < address_span.reg_count; ++i) {
    auto const reg_value = address_map[address_span.start_address + i];
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

void RtuSlave::ProcessWriteSingleRegister(AddressMap<int16_t> &address_map, RtuRequest const &request,
                                          RtuResponse &response) {
  if (request.GetData().size() < 2) {
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  uint16_t const address = MakeInt16(request.GetData()[0], request.GetData()[1]);
  int16_t new_value = MakeInt16(request.GetData()[2], request.GetData()[3]);
  if (address_map[address].has_value()) {
    address_map.Set(address, new_value);
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  } else {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  }
}

}  // namespace supermb
