#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace fileutils {
std::string makeTempPartPath(const std::string& path, bool pathIsDir);

using HashProgressCallback = std::function<void(std::uintmax_t total_bytes,
                                                std::uintmax_t processed_bytes)>;
// Generic file hash using Botan-2.
//
// Parameters:
//   file_path   - path to the file to hash
//   buffer_size - temporary read buffer size; if 0, std::logic_error is thrown
//   algorithm   - hash algorithm name understood by Botan (e.g. "SHA-256",
//                 "SHA-512", "SHA-1", "BLAKE2b", ...)
//   progress_cb - optional progress callback
//
// Throws:
//   std::logic_error   - if buffer_size == 0 (programming error)
//   std::runtime_error - if file operations fail or algorithm is unsupported
//
// Returns:
//   Lower-case hexadecimal string with the digest.
//
std::string compute_file_hash(const std::filesystem::path& file_path,
                              std::size_t buffer_size,
                              std::string_view algorithm,
                              HashProgressCallback progress_cb = {});

// Convenience wrapper for SHA-256 using the generic hash function above.
inline std::string compute_file_sha256(const std::filesystem::path& file_path,
                                       std::size_t buffer_size,
                                       HashProgressCallback progress_cb = {})
{
    return compute_file_hash(file_path, buffer_size, "SHA-256", std::move(progress_cb));
}
}