#include <gtest/gtest.h>
#include <limits>

#include "../src/SizeFormat.h"

using SizeFormat::formatSize;
using SizeFormat::formatWithSeparators;

TEST(SizeFormatTest, MaxSizeBinaryEi)
{
  const std::size_t maxValue = std::numeric_limits<std::size_t>::max();
  // 2^64 - 1 / 1024^6 = 16.0  -> "16.0 Ei"
  EXPECT_EQ("16.0 Ei", formatSize(maxValue, /*binaryPrefix=*/true));
}

TEST(SizeFormatTest, MaxSizeDecimalE)
{
  const std::size_t maxValue = std::numeric_limits<std::size_t>::max();
  // ~ 1.8446744e19 -> 18.4 * 10^18  -> "18.4 E"
  EXPECT_EQ("18.4 E", formatSize(maxValue, /*binaryPrefix=*/false));
}

TEST(SizeFormatTest, KiloExamples)
{
  // 1020 / 1000 = 1.02  -> "1.02 k"
  EXPECT_EQ("1.02 k", formatSize(1020u, /*binaryPrefix=*/false));

  // 1020 < 1024, so it is left without a binary prefix
  EXPECT_EQ("1020", formatSize(1020u, /*binaryPrefix=*/true));
}

TEST(SizeFormatTest, SmallValuesFormatting)
{
  // Kilka sanity-checków, jeśli implementacja tak działa
  EXPECT_EQ("1",   formatSize(1u,   false));
  EXPECT_EQ("999", formatSize(999u, false));
  EXPECT_EQ("1.02 k", formatSize(1020u, false));
  EXPECT_EQ("1020", formatSize(1020u, true));
}

TEST(SizeFormatTest, SeparatorsZeroAndSmall)
{
  EXPECT_EQ("0", formatWithSeparators(0u));
  EXPECT_EQ("5", formatWithSeparators(5u));
  EXPECT_EQ("123", formatWithSeparators(123u));
}

TEST(SizeFormatTest, SeparatorsLargerNumbers)
{
  EXPECT_EQ("1'234", formatWithSeparators(1234u));
  EXPECT_EQ("12'345'678", formatWithSeparators(12345678u));
  EXPECT_EQ("3'123'456'789", formatWithSeparators(3123456789u));
}
