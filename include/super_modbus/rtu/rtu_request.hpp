#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include "../common/address_span.hpp"
#include "../common/function_code.hpp"

namespace supermb {

class RtuRequest {
 public:
  struct Header {
    uint8_t slave_id;
    FunctionCode function_code;
  };

  explicit RtuRequest(Header header)
      : header_(header) {}

  [[nodiscard]] uint8_t GetSlaveId() const { return header_.slave_id; }
  [[nodiscard]] FunctionCode GetFunctionCode() const { return header_.function_code; }
  [[nodiscard]] std::vector<uint8_t> const &GetData() const { return data_; }
  [[nodiscard]] std::optional<AddressSpan> GetAddressSpan() const;

  bool SetAddressSpan(AddressSpan address_span);
  bool SetWriteSingleRegisterData(uint16_t register_address, int16_t register_value);

 private:
  Header header_;
  std::vector<uint8_t> data_{};
};

}  // namespace supermb
