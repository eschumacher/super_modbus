#include <cstring>
#include <gtest/gtest.h>
#include "super_modbus/common/byte_helpers.hpp"
#include "super_modbus/common/wire_format_options.hpp"

using supermb::ByteOrder;
using supermb::DecodeFloat;
using supermb::DecodeU16;
using supermb::DecodeU32;
using supermb::EncodeFloat;
using supermb::EncodeU16;
using supermb::EncodeU32;
using supermb::WordOrder;

TEST(ByteHelpersCodec, EncodeU16_DecodeU16_BigEndian) {
  uint8_t buf[2];
  EncodeU16(0x1234, ByteOrder::BigEndian, buf);
  EXPECT_EQ(buf[0], 0x12);
  EXPECT_EQ(buf[1], 0x34);
  EXPECT_EQ(DecodeU16(buf[0], buf[1], ByteOrder::BigEndian), 0x1234u);
}

TEST(ByteHelpersCodec, EncodeU16_DecodeU16_LittleEndian) {
  uint8_t buf[2];
  EncodeU16(0x1234, ByteOrder::LittleEndian, buf);
  EXPECT_EQ(buf[0], 0x34);
  EXPECT_EQ(buf[1], 0x12);
  EXPECT_EQ(DecodeU16(buf[0], buf[1], ByteOrder::LittleEndian), 0x1234u);
}

TEST(ByteHelpersCodec, EncodeU32_DecodeU32_BigEndian_HighWordFirst) {
  uint8_t buf[4];
  EncodeU32(0x12345678u, ByteOrder::BigEndian, WordOrder::HighWordFirst, buf);
  EXPECT_EQ(buf[0], 0x12);
  EXPECT_EQ(buf[1], 0x34);
  EXPECT_EQ(buf[2], 0x56);
  EXPECT_EQ(buf[3], 0x78);
  EXPECT_EQ(DecodeU32(buf, ByteOrder::BigEndian, WordOrder::HighWordFirst), 0x12345678u);
}

TEST(ByteHelpersCodec, EncodeU32_DecodeU32_BigEndian_LowWordFirst) {
  uint8_t buf[4];
  EncodeU32(0x12345678u, ByteOrder::BigEndian, WordOrder::LowWordFirst, buf);
  EXPECT_EQ(buf[0], 0x56);
  EXPECT_EQ(buf[1], 0x78);
  EXPECT_EQ(buf[2], 0x12);
  EXPECT_EQ(buf[3], 0x34);
  EXPECT_EQ(DecodeU32(buf, ByteOrder::BigEndian, WordOrder::LowWordFirst), 0x12345678u);
}

TEST(ByteHelpersCodec, EncodeU32_DecodeU32_LittleEndian_HighWordFirst) {
  uint8_t buf[4];
  EncodeU32(0x12345678u, ByteOrder::LittleEndian, WordOrder::HighWordFirst, buf);
  EXPECT_EQ(buf[0], 0x34);
  EXPECT_EQ(buf[1], 0x12);
  EXPECT_EQ(buf[2], 0x78);
  EXPECT_EQ(buf[3], 0x56);
  EXPECT_EQ(DecodeU32(buf, ByteOrder::LittleEndian, WordOrder::HighWordFirst), 0x12345678u);
}

TEST(ByteHelpersCodec, EncodeU32_DecodeU32_LittleEndian_LowWordFirst) {
  uint8_t buf[4];
  EncodeU32(0x12345678u, ByteOrder::LittleEndian, WordOrder::LowWordFirst, buf);
  EXPECT_EQ(buf[0], 0x78);
  EXPECT_EQ(buf[1], 0x56);
  EXPECT_EQ(buf[2], 0x34);
  EXPECT_EQ(buf[3], 0x12);
  EXPECT_EQ(DecodeU32(buf, ByteOrder::LittleEndian, WordOrder::LowWordFirst), 0x12345678u);
}

TEST(ByteHelpersCodec, EncodeFloat_DecodeFloat_RoundTrip) {
  const float values[] = {0.0f, 1.0f, -1.0f, 3.14f, 1e10f, 1e-10f};
  for (float v : values) {
    uint8_t buf[4];
    EncodeFloat(v, ByteOrder::BigEndian, WordOrder::HighWordFirst, buf);
    float decoded = DecodeFloat(buf, ByteOrder::BigEndian, WordOrder::HighWordFirst);
    EXPECT_FLOAT_EQ(decoded, v);
  }
}

TEST(ByteHelpersCodec, EncodeFloat_DecodeFloat_LittleEndian_LowWordFirst) {
  float v = 1.0f;
  uint8_t buf[4];
  EncodeFloat(v, ByteOrder::LittleEndian, WordOrder::LowWordFirst, buf);
  float decoded = DecodeFloat(buf, ByteOrder::LittleEndian, WordOrder::LowWordFirst);
  EXPECT_FLOAT_EQ(decoded, v);
}

TEST(ByteHelpersCodec, EncodeFloat_1_0_BigEndianHighWordFirst) {
  float v = 1.0f;
  uint32_t bits;
  std::memcpy(&bits, &v, sizeof(bits));
  EXPECT_EQ(bits, 0x3F800000u);  // IEEE 754 single
  uint8_t buf[4];
  EncodeFloat(v, ByteOrder::BigEndian, WordOrder::HighWordFirst, buf);
  EXPECT_EQ(buf[0], 0x3F);
  EXPECT_EQ(buf[1], 0x80);
  EXPECT_EQ(buf[2], 0x00);
  EXPECT_EQ(buf[3], 0x00);
}
