#include "SizeFormat.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ios>

namespace SizeFormat {
std::string formatSize(std::size_t value, bool binaryPrefix) {
  if (value == 0) {
    return "0";
  }

  const std::size_t base = binaryPrefix ? Prefixes::Ki : Prefixes::k;

  const auto &suffixes = binaryPrefix ? BINARY_SUFFIXES : DECIMAL_SUFFIXES;
  std::size_t unitIndex = 0;
  std::size_t divisor = 1;

  // We're looking for the largest divisor (1, 1000, 1000^2, ...) such that
  // divisor <= value and doesn't go beyond the suffix array.
  while (unitIndex + 1 < suffixes.size() && (value / base) >= divisor) {
    divisor *= base;
    ++unitIndex;
  }

  long double amount =
      static_cast<long double>(value) / static_cast<long double>(divisor);

  int precision = 0;
  if (amount < static_cast<long double>(10.0)) {
    precision = 2;
  } else if (amount < static_cast<long double>(100.0)) {
    precision = 1;
  } else {
    precision = 0;
  }

  std::ostringstream oss;

  if (precision == 0) {
    // Round to the nearest integer
    auto rounded = std::llround(amount);
    oss << rounded;
  } else {
    oss << std::fixed << std::setprecision(precision) << amount;
  }

  if (!suffixes[unitIndex].empty()) {
    oss << ' ' << suffixes[unitIndex];
  }

  return oss.str();
}

std::string formatWithSeparators(std::size_t value) {
  std::string digits = std::to_string(value);
  std::string result;
  result.reserve(digits.size() + digits.size() / 3);

  int count = 0;
  for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
    if (count == 3) {
      result.push_back('\'');
      count = 0;
    }
    result.push_back(*it);
    ++count;
  }

  std::reverse(result.begin(), result.end());
  return result;
}
}