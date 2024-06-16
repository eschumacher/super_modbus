#pragma once

#include <cstdint>

namespace supermb {

struct AddressSpan {
  uint16_t start_address{0};
  uint16_t reg_count{0};
};

}  // namespace supermb
