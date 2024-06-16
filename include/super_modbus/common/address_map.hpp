#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include "address_span.hpp"

namespace supermb {

template <typename DataType>
class AddressMap {
 public:
  void AddAddressSpan(AddressSpan span) {
    for (uint16_t i = span.start_address; i < span.start_address + span.reg_count; ++i) {
      if (!data_.contains(i)) {
        data_[i] = DataType{};
      }
    }
  }

  void RemoveAddressSpan(AddressSpan span) {
    for (uint16_t i = span.start_address; i < span.start_address + span.reg_count; ++i) {
      data_.erase(i);
    }
  }

  std::optional<DataType> operator[](int address) {
    if (data_.contains(address)) {
      return data_[address];
    }

    return {};
  }

 private:
  std::unordered_map<int, DataType> data_{};
};

}  // namespace supermb
