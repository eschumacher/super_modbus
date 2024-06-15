#pragma once

#include <cstdint>
#include "../common/address_span.hpp"
#include "../common/function_code.hpp"

namespace supermb {

class RtuRequest {
 public:
  RtuRequest(uint8_t slave_id, FunctionCode function_code,
             AddressSpan address_span)
      : slave_id_(slave_id),
        function_code_(function_code),
        address_span_(address_span) {}

  [[nodiscard]] uint8_t GetSlaveId() const { return slave_id_; }
  [[nodiscard]] FunctionCode GetFunctionCode() const { return function_code_; }
  [[nodiscard]] AddressSpan GetAddressSpan() const { return address_span_; }

 private:
  uint8_t slave_id_{};
  FunctionCode function_code_{};
  AddressSpan address_span_{};
};

}  // namespace supermb
