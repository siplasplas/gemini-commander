#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>

#include <botan/hash.h>
#include <botan/hex.h>

#if defined(_WIN32)
#  include <process.h>
#  define GETPID() _getpid()
#else
#  include <unistd.h>
#  define GETPID() getpid()
#endif

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
