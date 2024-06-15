#pragma once

#include <cstdint>
#include <vector>
#include "../common/exception_code.hpp"
#include "../common/function_code.hpp"

namespace supermb {

class RtuResponse {
 public:
  RtuResponse(uint8_t slave_id, FunctionCode function_code)
      : slave_id_(slave_id),
        function_code_(function_code) {}

  [[nodiscard]] uint8_t GetSlaveId() const noexcept { return slave_id_; }

  [[nodiscard]] FunctionCode GetFunctionCode() const noexcept {
    return function_code_;
  }
  [[nodiscard]] ExceptionCode GetExceptionCode() const noexcept {
    return exception_code_;
  }
  [[nodiscard]] std::vector<uint8_t> GetData() const { return data_; }

  void SetExceptionCode(ExceptionCode const &exception_code) noexcept {
    exception_code_ = exception_code;
  }

  void SetData(std::vector<uint8_t> const &data) { data_ = data; };

 private:
  const uint8_t slave_id_{};
  const FunctionCode function_code_{};
  ExceptionCode exception_code_{ExceptionCode::kInvalidExceptionCode};
  std::vector<uint8_t> data_{};
};

}  // namespace supermb
