// SizeFormat.h
#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace Prefixes {
// Decimal (SI)
constexpr std::size_t k  = 1000ULL;
constexpr std::size_t M  = k * k;        // 1'000'000
constexpr std::size_t G  = M * k;
constexpr std::size_t T  = G * k;
constexpr std::size_t P  = T * k;
constexpr std::size_t E  = P * k;

// Binary (IEC)
constexpr std::size_t Ki = 1024ULL;
constexpr std::size_t Mi = Ki * Ki;
constexpr std::size_t Gi = Mi * Ki;
constexpr std::size_t Ti = Gi * Ki;
constexpr std::size_t Pi = Ti * Ki;
constexpr std::size_t Ei = Pi * Ki;
}

namespace SizeFormat {
enum SizeKind {Precise, Decimal, Binary};

std::string formatSize(std::size_t value, SizeKind format);

// Samples:
//   formatSize( std::numeric_limits<size_t>::max(), true )
//      -> "16.0 Ei" (zaokrąglanie)
//   formatSize( std::numeric_limits<size_t>::max(), false )
//      -> "18.4 E"
//   formatSize(1020, false) -> "1.02 k"
//   formatSize(1020, true)  -> "1020"
std::string formatWithPrefix(std::size_t value, bool binaryPrefix);

// Sample
//   3123456789 -> "3'123'456'789"
std::string formatWithSeparators(std::size_t value);


static const std::array<std::string, 7> DECIMAL_SUFFIXES = {
  "", // B
  "k", "M", "G", "T", "P", "E",
};

static const std::array<std::string, 7> BINARY_SUFFIXES = {
  "", // B
  "Ki", "Mi", "Gi", "Ti", "Pi", "Ei",
};
} // namespace SizeFormat