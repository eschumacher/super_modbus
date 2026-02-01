#include <gtest/gtest.h>
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/crc16.hpp"

using supermb::CalculateCrc16;
using supermb::GetHighByte;
using supermb::GetLowByte;
using supermb::VerifyCrc16;

TEST(CRC16, EmptyData) {
  std::vector<uint8_t> empty;
  uint16_t crc = CalculateCrc16(empty);
  // CRC of empty data should be initial value (0xFFFF)
  EXPECT_EQ(crc, 0xFFFF);
}

TEST(CRC16, SingleByte) {
  std::vector<uint8_t> data{0x01};
  uint16_t crc = CalculateCrc16(data);
  // Should not be initial value after processing
  EXPECT_NE(crc, 0xFFFF);
}

TEST(CRC16, KnownTestVector1) {
  // Standard Modbus test vector: 01 03 00 00 00 0A
  std::vector<uint8_t> frame{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  uint16_t crc = CalculateCrc16(frame);

  // Expected CRC for this frame: 0xC5CD (little-endian: CD C5)
  EXPECT_EQ(crc, 0xC5CD);

  // Verify with frame including CRC
  frame.push_back(0xCD);  // Low byte
  frame.push_back(0xC5);  // High byte
  EXPECT_TRUE(VerifyCrc16(frame));
}

TEST(CRC16, KnownTestVector2) {
  // Another test vector: 01 06 00 01 00 17
  std::vector<uint8_t> frame{0x01, 0x06, 0x00, 0x01, 0x00, 0x17};
  uint16_t crc = CalculateCrc16(frame);

  // Expected CRC: 0x9A9B (little-endian: 9B 9A)
  EXPECT_EQ(crc, 0x9A9B);

  frame.push_back(0x9B);  // Low byte
  frame.push_back(0x9A);  // High byte
  EXPECT_TRUE(VerifyCrc16(frame));
}

TEST(CRC16, KnownTestVector3) {
  // Test vector: 11 03 00 6B 00 03
  std::vector<uint8_t> frame{0x11, 0x03, 0x00, 0x6B, 0x00, 0x03};
  uint16_t crc = CalculateCrc16(frame);

  // Expected CRC: 0x7687 (little-endian: 87 76)
  EXPECT_EQ(crc, 0x7687);

  frame.push_back(0x87);  // Low byte
  frame.push_back(0x76);  // High byte
  EXPECT_TRUE(VerifyCrc16(frame));
}

TEST(CRC16, VerifyValidFrame) {
  std::vector<uint8_t> frame{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  uint16_t crc = CalculateCrc16(frame);
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));

  EXPECT_TRUE(VerifyCrc16(frame));
}

TEST(CRC16, VerifyInvalidFrameTooShort) {
  std::vector<uint8_t> frame{0x01};  // Too short (need at least 2 bytes for CRC)
  EXPECT_FALSE(VerifyCrc16(frame));
}

TEST(CRC16, VerifyInvalidFrameSingleByte) {
  std::vector<uint8_t> frame{0x01, 0x03};  // Only 2 bytes, but no data
  EXPECT_FALSE(VerifyCrc16(frame));
}

TEST(CRC16, VerifyInvalidFrameCorruptedData) {
  std::vector<uint8_t> frame{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  uint16_t crc = CalculateCrc16(frame);
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));

  // Corrupt the data
  frame[0] = 0x02;
  EXPECT_FALSE(VerifyCrc16(frame));
}

TEST(CRC16, VerifyInvalidFrameCorruptedCRC) {
  std::vector<uint8_t> frame{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  uint16_t crc = CalculateCrc16(frame);
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));

  // Corrupt the CRC
  frame[frame.size() - 1] = static_cast<uint8_t>(frame[frame.size() - 1] ^ 0xFF);
  EXPECT_FALSE(VerifyCrc16(frame));
}

TEST(CRC16, VerifyInvalidFrameWrongCRC) {
  std::vector<uint8_t> frame{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  frame.push_back(0x00);  // Wrong CRC low byte
  frame.push_back(0x00);  // Wrong CRC high byte
  EXPECT_FALSE(VerifyCrc16(frame));
}

TEST(CRC16, AllOnesData) {
  std::vector<uint8_t> frame(10, 0xFF);
  uint16_t crc = CalculateCrc16(frame);
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));
  EXPECT_TRUE(VerifyCrc16(frame));
}

TEST(CRC16, AllZerosData) {
  std::vector<uint8_t> frame(10, 0x00);
  uint16_t crc = CalculateCrc16(frame);
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));
  EXPECT_TRUE(VerifyCrc16(frame));
}

TEST(CRC16, AlternatingPattern) {
  std::vector<uint8_t> frame{0xAA, 0x55, 0xAA, 0x55};
  uint16_t crc = CalculateCrc16(frame);
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));
  EXPECT_TRUE(VerifyCrc16(frame));
}

TEST(CRC16, LargeFrame) {
  std::vector<uint8_t> frame;
  for (int i = 0; i < 256; ++i) {
    frame.push_back(static_cast<uint8_t>(i));
  }
  uint16_t crc = CalculateCrc16(frame);
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));
  EXPECT_TRUE(VerifyCrc16(frame));
}

TEST(CRC16, IncrementalChanges) {
  std::vector<uint8_t> frame1{0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  std::vector<uint8_t> frame2{0x01, 0x03, 0x00, 0x00, 0x00, 0x0B};

  uint16_t crc1 = CalculateCrc16(frame1);
  uint16_t crc2 = CalculateCrc16(frame2);

  // Changing data should change CRC
  EXPECT_NE(crc1, crc2);
}

TEST(CRC16, OrderMatters) {
  std::vector<uint8_t> frame1{0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> frame2{0x04, 0x03, 0x02, 0x01};

  uint16_t crc1 = CalculateCrc16(frame1);
  uint16_t crc2 = CalculateCrc16(frame2);

  // Different order should produce different CRC
  EXPECT_NE(crc1, crc2);
}
