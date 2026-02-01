#include <gtest/gtest.h>
#include "super_modbus/common/address_map.hpp"
#include "super_modbus/common/address_span.hpp"
#include "super_modbus/common/byte_helpers.hpp"

using supermb::AddressMap;
using supermb::AddressSpan;
using supermb::GetHighByte;
using supermb::GetLowByte;
using supermb::MakeInt16;

// AddressMap Tests
TEST(AddressMap, AddAddressSpan) {
  AddressMap<int16_t> map;

  // Add a span
  map.AddAddressSpan({0, 10});

  // Check that addresses 0-9 are accessible
  for (int i = 0; i < 10; ++i) {
    auto value = map[i];
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 0);  // Default initialized
  }

  // Address 10 should not exist
  auto value = map[10];
  EXPECT_FALSE(value.has_value());
}

TEST(AddressMap, AddMultipleSpans) {
  AddressMap<int16_t> map;

  map.AddAddressSpan({0, 5});
  map.AddAddressSpan({10, 5});

  // Check first span
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(map[i].has_value());
  }

  // Check gap
  EXPECT_FALSE(map[5].has_value());
  EXPECT_FALSE(map[9].has_value());

  // Check second span
  for (int i = 10; i < 15; ++i) {
    EXPECT_TRUE(map[i].has_value());
  }
}

TEST(AddressMap, OverlappingSpans) {
  AddressMap<int16_t> map;

  map.AddAddressSpan({0, 10});
  map.AddAddressSpan({5, 10});  // Overlaps

  // All addresses 0-14 should exist
  for (int i = 0; i < 15; ++i) {
    EXPECT_TRUE(map[i].has_value());
  }
}

TEST(AddressMap, RemoveAddressSpan) {
  AddressMap<int16_t> map;

  map.AddAddressSpan({0, 10});

  // Set some values
  map.Set(5, 100);
  map.Set(6, 200);

  // Remove span that includes address 5
  map.RemoveAddressSpan({5, 3});

  // Address 5 and 6 should be gone
  EXPECT_FALSE(map[5].has_value());
  EXPECT_FALSE(map[6].has_value());
  EXPECT_FALSE(map[7].has_value());

  // Address 4 should still exist
  EXPECT_TRUE(map[4].has_value());
}

TEST(AddressMap, SetAndGet) {
  AddressMap<int16_t> map;

  map.AddAddressSpan({0, 10});

  // Set a value
  EXPECT_TRUE(map.Set(5, 1234));
  auto value = map[5];
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 1234);

  // Try to set value at non-existent address
  EXPECT_FALSE(map.Set(20, 5678));
  EXPECT_FALSE(map[20].has_value());
}

TEST(AddressMap, SetMultipleValues) {
  AddressMap<int16_t> map;

  map.AddAddressSpan({0, 10});

  // Set multiple values
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(map.Set(i, static_cast<int16_t>(i * 10)));
  }

  // Verify all values
  for (int i = 0; i < 10; ++i) {
    auto value = map[i];
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, static_cast<int16_t>(i * 10));
  }
}

TEST(AddressMap, AddSpanTwice) {
  AddressMap<int16_t> map;

  map.AddAddressSpan({0, 10});
  map.Set(5, 100);

  // Add same span again - should not overwrite existing values
  map.AddAddressSpan({0, 10});

  auto value = map[5];
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 100);  // Value preserved
}

TEST(AddressMap, BoolType) {
  AddressMap<bool> map;

  map.AddAddressSpan({0, 10});
  EXPECT_TRUE(map.Set(5, true));

  auto value = map[5];
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(*value);
}

TEST(AddressMap, EmptySpan) {
  AddressMap<int16_t> map;

  map.AddAddressSpan({0, 0});  // Empty span

  EXPECT_FALSE(map[0].has_value());
}

// Byte Helpers Tests
TEST(ByteHelpers, GetLowByte) {
  EXPECT_EQ(GetLowByte(0x1234), 0x34);
  EXPECT_EQ(GetLowByte(0x00FF), 0xFF);
  EXPECT_EQ(GetLowByte(0xFF00), 0x00);
  EXPECT_EQ(GetLowByte(0x0000), 0x00);
  EXPECT_EQ(GetLowByte(0xFFFF), 0xFF);
}

TEST(ByteHelpers, GetHighByte) {
  EXPECT_EQ(GetHighByte(0x1234), 0x12);
  EXPECT_EQ(GetHighByte(0x00FF), 0x00);
  EXPECT_EQ(GetHighByte(0xFF00), 0xFF);
  EXPECT_EQ(GetHighByte(0x0000), 0x00);
  EXPECT_EQ(GetHighByte(0xFFFF), 0xFF);
}

TEST(ByteHelpers, MakeInt16) {
  EXPECT_EQ(MakeInt16(0x34, 0x12), 0x1234);
  EXPECT_EQ(MakeInt16(0xFF, 0x00), 0x00FF);
  EXPECT_EQ(MakeInt16(0x00, 0xFF), 0xFF00);
  EXPECT_EQ(MakeInt16(0x00, 0x00), 0x0000);
  EXPECT_EQ(MakeInt16(0xFF, 0xFF), static_cast<int16_t>(0xFFFF));
}

TEST(ByteHelpers, MakeInt16Negative) {
  // Test negative values
  EXPECT_EQ(MakeInt16(0x00, 0x80), static_cast<int16_t>(0x8000));  // -32768
  EXPECT_EQ(MakeInt16(0xFF, 0xFF), static_cast<int16_t>(0xFFFF));  // -1
  EXPECT_EQ(MakeInt16(0xCE, 0xFF), static_cast<int16_t>(0xFFCE));  // -50
}

TEST(ByteHelpers, RoundTrip) {
  // Test that GetLowByte/GetHighByte and MakeInt16 are inverse operations
  constexpr uint16_t test_values[] = {0x0000, 0x0001, 0x00FF, 0x0100, 0x1234, 0xFFFF};

  for (uint16_t value : test_values) {
    uint8_t low = GetLowByte(value);
    uint8_t high = GetHighByte(value);
    int16_t reconstructed = MakeInt16(low, high);
    EXPECT_EQ(static_cast<uint16_t>(reconstructed), value);
  }
}

TEST(ByteHelpers, AllByteCombinations) {
  // Test all possible byte combinations
  for (uint8_t low = 0; low < 256; ++low) {
    for (uint8_t high = 0; high < 256; ++high) {
      int16_t value = MakeInt16(low, high);
      EXPECT_EQ(GetLowByte(static_cast<uint16_t>(value)), low);
      EXPECT_EQ(GetHighByte(static_cast<uint16_t>(value)), high);
    }
  }
}
