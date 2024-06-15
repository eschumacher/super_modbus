#pragma once

#include <cstdint>
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

 private:
  uint8_t id_{1};
};

}  // namespace supermb
