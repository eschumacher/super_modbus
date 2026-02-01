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

  bool Set(int address, DataType value) {
    const auto value_iter = data_.find(address);
    if (value_iter != data_.end()) {
      value_iter->second = value;
      return true;
    }

    return false;
  }

  [[nodiscard]] std::optional<DataType> operator[](int address) const {
    const auto value_iter = data_.find(address);
    if (value_iter != data_.end()) {
      return value_iter->second;
    }

    return {};
  }

 private:
  std::unordered_map<int, DataType> data_{};
};

}  // namespace supermb
