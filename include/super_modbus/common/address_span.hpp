#pragma once

#include <cstdint>

namespace supermb {

struct AddressSpan {
  uint16_t start_address_{0};
  uint16_t reg_count_{0};
};

}  // namespace supermb
