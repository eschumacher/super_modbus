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

}  // namespace supermb
