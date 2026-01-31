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
 * @brief Exception function code mask - sets MSB to indicate exception response
 * In Modbus RTU, exception responses have the function code with bit 7 (0x80) set
 */
static constexpr uint8_t kExceptionFunctionCodeMask = 0x80;

/**
 * @brief Function code mask - clears MSB to get base function code
 * Used to extract the base function code from an exception response
 */
static constexpr uint8_t kFunctionCodeMask = 0x7F;

/**
 * @brief Coil ON value in Modbus RTU
 * When writing a single coil, 0xFF00 indicates ON (true), 0x0000 indicates OFF (false)
 */
static constexpr uint16_t kCoilOnValue = 0xFF00;

/**
 * @brief Number of coils per byte in Modbus RTU
 * Each byte can hold 8 coils (bits)
 */
static constexpr uint8_t kCoilsPerByte = 8;

/**
 * @brief Rounding offset for coil byte count calculation
 * Used in formula: (count + kCoilByteCountRoundingOffset) / kCoilsPerByte to round up
 */
static constexpr uint8_t kCoilByteCountRoundingOffset = 7;

/**
 * @brief File record reference type
 * Modbus file record operations use reference type 0x06
 */
static constexpr uint8_t kFileRecordReferenceType = 0x06;

/**
 * @brief Bytes per file record header
 * Each file record header: file_number(2) + record_number(2) + record_length(2) = 6 bytes
 */
static constexpr uint8_t kFileRecordBytesPerRecord = 6;

/**
 * @brief Check if a function code is a write operation that can be broadcast
 * @param function_code The function code to check
 * @return true if the function code is a write operation that supports broadcast (slave ID 0)
 */
constexpr bool IsBroadcastableWrite(FunctionCode function_code) {
  return function_code == FunctionCode::kWriteSingleCoil || function_code == FunctionCode::kWriteSingleReg ||
         function_code == FunctionCode::kWriteMultCoils || function_code == FunctionCode::kWriteMultRegs ||
         function_code == FunctionCode::kWriteFileRecord;
}

}  // namespace supermb
