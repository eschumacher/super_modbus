#pragma once

#include <cstdint>
#include "../common/address_map.hpp"
#include "../common/address_span.hpp"
#include "rtu_request.hpp"
#include "rtu_response.hpp"

namespace supermb {

class RtuSlave {
 public:
  explicit RtuSlave(uint8_t slave_id)
      : id_(slave_id) {}

  [[nodiscard]] uint8_t GetId() const noexcept { return id_; }
  void SetId(uint8_t slave_id) noexcept { id_ = slave_id; }

  RtuResponse Process(RtuRequest const &request);

  void AddHoldingRegisters(AddressSpan span);
  void AddInputRegisters(AddressSpan span);

 private:
  static void ProcessReadRegisters(AddressMap<int16_t> const &address_map, RtuRequest const &request,
                                   RtuResponse &response);
  static void ProcessWriteSingleRegister(AddressMap<int16_t> &address_map, RtuRequest const &request,
                                         RtuResponse &response);

  uint8_t id_{1};
  AddressMap<int16_t> holding_registers_{};
  AddressMap<int16_t> input_registers_{};
};

}  // namespace supermb
