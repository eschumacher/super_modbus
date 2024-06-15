#pragma once

#include <cstdint>

namespace supermb {

enum class ExceptionCode : uint8_t {
  kInvalidExceptionCode = 0x00,
  kIllegalFunction = 0x01,
  kIllegalDataAddress = 0x02,
  kIllegalDataValue = 0x03,
  kServerDeviceFailure = 0x04,
  kAcknowledge = 0x05,
  kServerDeviceBusy = 0x06,
  kMemoryParityError = 0x08,
  kGatewayPathUnavailable = 0x0A,
  kGatewayTargetDeviceFailedToRespond = 0x0B
};

}  // namespace supermb
