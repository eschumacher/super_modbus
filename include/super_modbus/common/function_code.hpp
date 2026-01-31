#pragma once

#include <cstdint>

namespace supermb {

enum class FunctionCode : uint8_t {
  kInvalid = 0,
  kReadCoils = 1,
  kReadDI = 2,
  kReadHR = 3,
  kReadIR = 4,
  kWriteSingleCoil = 5,
  kWriteSingleReg = 6,
  kReadExceptionStatus = 7,
  kDiagnostics = 8,
  kGetComEventCounter = 11,
  kGetComEventLog = 12,
  kWriteMultCoils = 15,
  kWriteMultRegs = 16,
  kReportSlaveID = 17,
  kReadFileRecord = 20,
  kWriteFileRecord = 21,
  kMaskWriteReg = 22,
  kReadWriteMultRegs = 23,
  kReadFIFOQueue = 24
};

/**
 * @brief Check if a function code is a write operation that can be broadcast
 * @param function_code The function code to check
 * @return true if the function code is a write operation that supports broadcast (slave ID 0)
 */
constexpr bool IsBroadcastableWrite(FunctionCode function_code) {
  return function_code == FunctionCode::kWriteSingleCoil ||
         function_code == FunctionCode::kWriteSingleReg ||
         function_code == FunctionCode::kWriteMultCoils ||
         function_code == FunctionCode::kWriteMultRegs ||
         function_code == FunctionCode::kWriteFileRecord;
}

}  // namespace supermb
