#pragma once

#include "../common/exception_code.hpp"
#include "../common/function_code.hpp"
#include <cstdint>
#include <vector>

namespace supermb {

class TcpResponse {
 public:
  TcpResponse(uint16_t transaction_id, uint8_t unit_id, FunctionCode function_code)
      : transaction_id_(transaction_id),
        unit_id_(unit_id),
        function_code_(function_code) {}

  [[nodiscard]] uint16_t GetTransactionId() const noexcept { return transaction_id_; }
  [[nodiscard]] uint8_t GetUnitId() const noexcept { return unit_id_; }

  [[nodiscard]] FunctionCode GetFunctionCode() const noexcept { return function_code_; }
  [[nodiscard]] ExceptionCode GetExceptionCode() const noexcept { return exception_code_; }
  [[nodiscard]] std::vector<uint8_t> GetData() const { return data_; }

  void SetExceptionCode(const ExceptionCode &exception_code) noexcept { exception_code_ = exception_code; }

  void SetData(const std::vector<uint8_t> &data) { data_ = data; };
  void EmplaceBack(uint8_t data) { data_.emplace_back(data); }

 private:
  const uint16_t transaction_id_{};
  const uint8_t unit_id_{};
  const FunctionCode function_code_{};
  ExceptionCode exception_code_{ExceptionCode::kInvalidExceptionCode};
  std::vector<uint8_t> data_{};
};

}  // namespace supermb
