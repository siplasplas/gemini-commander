#include "fileutils.h"

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <fstream>

#include <botan/hash.h>
#include <botan/hex.h>

#if defined(_WIN32)
#  include <process.h>
#  define GETPID() _getpid()
#else
#  include <unistd.h>
#  define GETPID() getpid()
#endif

namespace fileutils {

namespace {
void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
    std::size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}
} // anonymous namespace

std::string escapePathForShell(const std::string& path)
{
    bool hasSpace = path.find(' ') != std::string::npos;
    bool hasBackslash = path.find('\\') != std::string::npos;
    bool hasDoubleQuote = path.find('"') != std::string::npos;
    bool hasSingleQuote = path.find('\'') != std::string::npos;

    // No special chars - return unchanged
    if (!hasSpace && !hasBackslash && !hasDoubleQuote && !hasSingleQuote) {
        return path;
    }

    // Has ' but no " - use double quotes (simpler)
    if (hasSingleQuote && !hasDoubleQuote) {
        std::string escaped = path;
        replaceAll(escaped, "\\", "\\\\");
        replaceAll(escaped, "$", "\\$");
        replaceAll(escaped, "`", "\\`");
        return "\"" + escaped + "\"";
    }

    // Otherwise: use single quotes, escape ' as '\''
    std::string escaped = path;
    replaceAll(escaped, "'", "'\\''");
    return "'" + escaped + "'";
}
std::string makeTempPartPath(const std::string& path, bool pathIsDir)
{
    static std::atomic<uint32_t> g_seq{0};
    namespace fs = std::filesystem;

    fs::path abs = fs::absolute(path);
    fs::path dir = pathIsDir ? abs : abs.parent_path();
    fs::path hashedPath = abs;

    std::string utf8 = hashedPath.u8string();
    auto crc = Botan::HashFunction::create("CRC32");
    crc->update(reinterpret_cast<const uint8_t*>(utf8.data()), utf8.size());
    auto digest = crc->final();

    std::string crcHex = Botan::hex_encode(digest);
    if (crcHex.size() > 8)
        crcHex = crcHex.substr(0, 8);

    int pid = GETPID();

    using namespace std::chrono;

    auto now  = system_clock::now();
    auto tt   = system_clock::to_time_t(now);
    auto us   = duration_cast<microseconds>(now.time_since_epoch()) % 1000000; // 0–999999
    auto ms   = duration_cast<milliseconds>(us) % 1000;                        // mmm
    auto microsOnly = us - duration_cast<microseconds>(ms);                    // uuu

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    std::ostringstream tbuf;
    tbuf << std::setw(2) << std::setfill('0') << tm.tm_min
         << std::setw(2) << tm.tm_sec
         << std::setw(3) << std::setfill('0') << ms.count()
         << std::setw(3) << std::setfill('0') << microsOnly.count();

    std::string timeStr = tbuf.str();

    constexpr uint32_t SEQ_MOD = 10'000;
    uint32_t seq = g_seq.fetch_add(1, std::memory_order_relaxed) % SEQ_MOD;

    std::ostringstream name;
    name << crcHex << pid << timeStr << seq << ".part";

    return (dir / name.str()).string();
}


std::string compute_file_hash(const std::filesystem::path& file_path,
                              std::size_t buffer_size,
                              std::string_view algorithm,
                              HashProgressCallback progress_cb)
{
    if (buffer_size == 0) {
        // Zero buffer size is a logic error in the caller.
        throw std::logic_error("compute_file_hash: buffer_size must be > 0");
    }

    // Obtain file size for progress reporting.
    std::uintmax_t total_size = 0;
    try {
        total_size = std::filesystem::file_size(file_path);
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error(
            std::string("compute_file_hash: unable to get file size: ") + e.what());
    }

    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("compute_file_hash: unable to open file for reading");
    }

    // Create hash function via Botan using the requested algorithm.
    auto hash = Botan::HashFunction::create(std::string(algorithm));
    if (!hash) {
        throw std::runtime_error(
            std::string("compute_file_hash: unsupported algorithm: ") +
            std::string(algorithm));
    }

    std::vector<std::uint8_t> buffer(buffer_size);
    std::uintmax_t processed = 0;

    // Initial progress notification.
    if (progress_cb) {
        progress_cb(total_size, processed);
    }

    while (in) {
        in.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = in.gcount();
        if (got <= 0) {
            break;
        }

        hash->update(buffer.data(), static_cast<std::size_t>(got));
        processed += static_cast<std::uintmax_t>(got);

        if (progress_cb) {
            progress_cb(total_size, processed);
        }
    }

    // Finalize the digest and return lower-case hex.
    const auto digest = hash->final();
    const std::string hex = Botan::hex_encode(digest, false /*lowercase*/);
    return hex;
}

std::string trimLeft(const std::string &str) {
    const auto strBegin = str.find_first_not_of(" \t");
    return str.substr(strBegin, str.length() - strBegin);
}


std::string trimRight(const std::string &str) {
    const auto strEnd = str.find_last_not_of(" \t\r");
    return str.substr(0, strEnd + 1);
}

std::string trim(const std::string &str) {
    return trimLeft(trimRight(str));
}

}