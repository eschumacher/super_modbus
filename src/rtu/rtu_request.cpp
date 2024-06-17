#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include "common/address_span.hpp"
#include "common/byte_helpers.hpp"
#include "common/function_code.hpp"
#include "rtu/rtu_request.hpp"

namespace supermb {

static constexpr std::array<FunctionCode, 10> kAddressSpanValidFunctions{
    FunctionCode::kReadCoils,        FunctionCode::kReadDI,          FunctionCode::kReadHR,
    FunctionCode::kReadIR,           FunctionCode::kWriteSingleCoil, FunctionCode::kWriteSingleReg,
    FunctionCode::kWriteMultCoils,   FunctionCode::kWriteMultRegs,   FunctionCode::kMaskWriteReg,
    FunctionCode::kReadWriteMultRegs};

static constexpr uint8_t kAddressSpanStartAddressIndex{0};
static constexpr uint8_t kAddressSpanRegCountIndex{2};
static constexpr uint8_t kAddressSpanMinDataSize{4};

std::optional<AddressSpan> RtuRequest::GetAddressSpan() const {
  if ((data_.size() < kAddressSpanMinDataSize) ||
      (std::find(kAddressSpanValidFunctions.begin(), kAddressSpanValidFunctions.end(), header_.function_code) ==
       kAddressSpanValidFunctions.end())) {
    return {};
  }

  // TODO: endianness?
  AddressSpan address_span;
  address_span.start_address =
      MakeInt16(data_[kAddressSpanStartAddressIndex + 1], data_[kAddressSpanStartAddressIndex]);
  address_span.reg_count = MakeInt16(data_[kAddressSpanRegCountIndex + 1], data_[kAddressSpanRegCountIndex]);
  return address_span;
}

bool RtuRequest::SetAddressSpan(AddressSpan address_span) {
  if (std::find(kAddressSpanValidFunctions.begin(), kAddressSpanValidFunctions.end(), header_.function_code) ==
      kAddressSpanValidFunctions.end()) {
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  data_.emplace_back(GetHighByte(address_span.start_address));
  data_.emplace_back(GetLowByte(address_span.start_address));
  data_.emplace_back(GetHighByte(address_span.reg_count));
  data_.emplace_back(GetLowByte(address_span.reg_count));
  return true;
}

bool RtuRequest::SetWriteSingleRegisterData(uint16_t register_address, int16_t register_value) {
  if (header_.function_code != FunctionCode::kWriteSingleReg) {
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  data_.emplace_back(GetHighByte(register_address));
  data_.emplace_back(GetLowByte(register_address));
  data_.emplace_back(GetHighByte(register_value));
  data_.emplace_back(GetLowByte(register_value));
  return true;
}

}  // namespace supermb
