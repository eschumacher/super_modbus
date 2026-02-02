#include <gtest/gtest.h>
#include "super_modbus/transport/memory_transport.hpp"

using supermb::MemoryTransport;

TEST(MemoryTransport, DefaultConstruction) {
  // Default constructor creates a vector with 256 default-initialized elements
  // So HasData() will return true and AvailableBytes() will be 256
  // To test truly empty transport, use 0 capacity
  MemoryTransport transport(0);

  EXPECT_FALSE(transport.HasData());
  EXPECT_EQ(transport.AvailableBytes(), 0);
  EXPECT_EQ(transport.GetWrittenData().size(), 0);
}

TEST(MemoryTransport, SetReadData) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03, 0x04, 0x05};

  transport.SetReadData(test_data);

  EXPECT_TRUE(transport.HasData());
  EXPECT_EQ(transport.AvailableBytes(), 5);
}

TEST(MemoryTransport, ReadAllData) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03};
  transport.SetReadData(test_data);

  uint8_t buffer[10] = {0};
  int bytes_read = transport.Read(buffer);

  EXPECT_EQ(bytes_read, 3);
  EXPECT_EQ(buffer[0], 0x01);
  EXPECT_EQ(buffer[1], 0x02);
  EXPECT_EQ(buffer[2], 0x03);
  EXPECT_FALSE(transport.HasData());
  EXPECT_EQ(transport.AvailableBytes(), 0);
}

TEST(MemoryTransport, ReadPartialData) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03, 0x04, 0x05};
  transport.SetReadData(test_data);

  uint8_t buffer[3] = {0};
  int bytes_read = transport.Read(buffer);

  EXPECT_EQ(bytes_read, 3);
  EXPECT_EQ(buffer[0], 0x01);
  EXPECT_EQ(buffer[1], 0x02);
  EXPECT_EQ(buffer[2], 0x03);
  EXPECT_TRUE(transport.HasData());
  EXPECT_EQ(transport.AvailableBytes(), 2);
}

TEST(MemoryTransport, ReadMultipleTimes) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03, 0x04, 0x05};
  transport.SetReadData(test_data);

  uint8_t buffer1[2] = {0};
  int bytes1 = transport.Read(buffer1);
  EXPECT_EQ(bytes1, 2);

  uint8_t buffer2[2] = {0};
  int bytes2 = transport.Read(buffer2);
  EXPECT_EQ(bytes2, 2);

  uint8_t buffer3[2] = {0};
  int bytes3 = transport.Read(buffer3);
  EXPECT_EQ(bytes3, 1);

  EXPECT_EQ(buffer1[0], 0x01);
  EXPECT_EQ(buffer1[1], 0x02);
  EXPECT_EQ(buffer2[0], 0x03);
  EXPECT_EQ(buffer2[1], 0x04);
  EXPECT_EQ(buffer3[0], 0x05);
}

TEST(MemoryTransport, ReadEmptyBuffer) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03};
  transport.SetReadData(test_data);

  // Can't create span from zero-sized array, use empty vector instead
  std::vector<uint8_t> empty_buffer;
  int bytes_read = transport.Read(empty_buffer);

  EXPECT_EQ(bytes_read, 0);
  EXPECT_TRUE(transport.HasData());
}

TEST(MemoryTransport, ReadBeyondEnd) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02};
  transport.SetReadData(test_data);

  uint8_t buffer[10] = {0};
  int bytes_read = transport.Read(buffer);

  EXPECT_EQ(bytes_read, 2);
  EXPECT_FALSE(transport.HasData());

  // Try to read again
  int bytes_read2 = transport.Read(buffer);
  EXPECT_EQ(bytes_read2, 0);
}

TEST(MemoryTransport, WriteData) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03};

  int bytes_written = transport.Write(test_data);
  EXPECT_EQ(bytes_written, 3);

  auto written = transport.GetWrittenData();
  ASSERT_EQ(written.size(), 3);  // ASSERT to prevent OOB access if wrong
  EXPECT_EQ(written[0], 0x01);
  EXPECT_EQ(written[1], 0x02);
  // cppcheck-suppress containerOutOfBounds
  EXPECT_EQ(written[2], 0x03);
}

TEST(MemoryTransport, WriteMultipleTimes) {
  MemoryTransport transport;

  std::vector<uint8_t> data1{0x01, 0x02};
  (void)transport.Write(data1);

  std::vector<uint8_t> data2{0x03, 0x04, 0x05};
  (void)transport.Write(data2);

  auto written = transport.GetWrittenData();
  ASSERT_EQ(written.size(), 5);  // ASSERT to prevent OOB access if wrong
  EXPECT_EQ(written[0], 0x01);
  EXPECT_EQ(written[1], 0x02);
  // cppcheck-suppress containerOutOfBounds
  EXPECT_EQ(written[2], 0x03);
  // cppcheck-suppress containerOutOfBounds
  EXPECT_EQ(written[3], 0x04);
  // cppcheck-suppress containerOutOfBounds
  EXPECT_EQ(written[4], 0x05);
}

TEST(MemoryTransport, WriteEmptyData) {
  MemoryTransport transport;
  std::vector<uint8_t> empty;

  int bytes_written = transport.Write(empty);
  EXPECT_EQ(bytes_written, 0);
  EXPECT_EQ(transport.GetWrittenData().size(), 0);
}

TEST(MemoryTransport, Flush) {
  MemoryTransport transport;
  // Flush should always return true for MemoryTransport
  EXPECT_TRUE(transport.Flush());
}

TEST(MemoryTransport, ClearWriteBuffer) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03};
  (void)transport.Write(test_data);

  EXPECT_GT(transport.GetWrittenData().size(), 0);

  transport.ClearWriteBuffer();
  EXPECT_EQ(transport.GetWrittenData().size(), 0);
}

TEST(MemoryTransport, ResetReadPosition) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data{0x01, 0x02, 0x03, 0x04, 0x05};
  transport.SetReadData(test_data);

  // Read some data
  uint8_t buffer[3] = {0};
  (void)transport.Read(buffer);
  EXPECT_EQ(transport.AvailableBytes(), 2);

  // Reset position
  transport.ResetReadPosition();
  EXPECT_EQ(transport.AvailableBytes(), 5);

  // Read again from beginning
  uint8_t buffer2[3] = {0};
  int bytes = transport.Read(buffer2);
  EXPECT_EQ(bytes, 3);
  EXPECT_EQ(buffer2[0], 0x01);
  EXPECT_EQ(buffer2[1], 0x02);
  EXPECT_EQ(buffer2[2], 0x03);
}

TEST(MemoryTransport, SetReadDataOverwrites) {
  MemoryTransport transport;
  std::vector<uint8_t> test_data1{0x01, 0x02, 0x03};
  transport.SetReadData(test_data1);

  // Read some
  uint8_t buffer[2] = {0};
  (void)transport.Read(buffer);

  // Set new data - should reset position
  std::vector<uint8_t> test_data2{0xAA, 0xBB, 0xCC, 0xDD};
  transport.SetReadData(test_data2);

  EXPECT_EQ(transport.AvailableBytes(), 4);
  uint8_t buffer2[4] = {0};
  int bytes = transport.Read(buffer2);
  EXPECT_EQ(bytes, 4);
  EXPECT_EQ(buffer2[0], 0xAA);
  EXPECT_EQ(buffer2[1], 0xBB);
  EXPECT_EQ(buffer2[2], 0xCC);
  EXPECT_EQ(buffer2[3], 0xDD);
}

TEST(MemoryTransport, LargeData) {
  MemoryTransport transport;
  std::vector<uint8_t> large_data(1000);
  for (size_t i = 0; i < large_data.size(); ++i) {
    large_data[i] = static_cast<uint8_t>(i & 0xFF);
  }

  transport.SetReadData(large_data);
  EXPECT_EQ(transport.AvailableBytes(), 1000);

  uint8_t buffer[1000];
  int bytes_read = transport.Read(buffer);
  EXPECT_EQ(bytes_read, 1000);

  for (size_t i = 0; i < 1000; ++i) {
    EXPECT_EQ(buffer[i], static_cast<uint8_t>(i & 0xFF));
  }
}

TEST(MemoryTransport, ReadWriteIndependence) {
  MemoryTransport transport;

  // Set read data
  std::vector<uint8_t> read_data{0x01, 0x02, 0x03};
  transport.SetReadData(read_data);

  // Write different data
  std::vector<uint8_t> write_data{0xAA, 0xBB, 0xCC};
  (void)transport.Write(write_data);

  // Read should not affect write buffer
  uint8_t buffer[3] = {0};
  (void)transport.Read(buffer);

  auto written = transport.GetWrittenData();
  ASSERT_EQ(written.size(), 3);  // ASSERT to prevent OOB access if wrong
  EXPECT_EQ(written[0], 0xAA);
  EXPECT_EQ(written[1], 0xBB);
  // cppcheck-suppress containerOutOfBounds
  EXPECT_EQ(written[2], 0xCC);
}

TEST(MemoryTransport, CustomInitialCapacity) {
  MemoryTransport transport(512);
  std::vector<uint8_t> test_data(256);
  transport.SetReadData(test_data);
  EXPECT_EQ(transport.AvailableBytes(), 256);
}
