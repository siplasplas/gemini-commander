#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <botan/hash.h>

#include "fileutils.h"

using fileutils::compute_file_hash;
using fileutils::HashProgressCallback;

TEST(FileHashTest, HashesWithVariousBufferSizes)
{
    // Create temporary file in /dev/shm using makeTempPartPath
    const std::string tmpPath =
         fileutils::makeTempPartPath("/dev/shm", /*pathIsDir=*/true);

    const std::string pattern = "0123456";     // 7 bytes
    const int repeats = 10000;                 // 70'000 bytes total

    {
        // Write file
        std::ofstream out(tmpPath, std::ios::binary);
        ASSERT_TRUE(out.good());

        for (int i = 0; i < repeats; ++i) {
            out.write(pattern.data(), static_cast<std::streamsize>(pattern.size()));
        }
    }

    ASSERT_EQ(std::filesystem::file_size(tmpPath), 70000);

    // Expected digests
    const std::string EXP_SHA256 =
        "d488d2272dea0966b36e4e5e0014eac188a713e9dfd089c30eaf13ddb7b143f8";

    const std::string EXP_SHA3_256 =
        "86f94b587c131d9d83046a660dad97fbc93ca93456531ef39a843b95ca984cbc";

    const std::string EXP_CRC32 =
        "afaadb67";

    const std::vector<std::size_t> BUFS = {
        1234,
        12345,
        70000,
        100000
    };

    for (std::size_t bufSize : BUFS)
    {
        // Empty callback
        HashProgressCallback cb = {};

        // SHA-256
        {
            const std::string hash =
                compute_file_hash(tmpPath, bufSize, "SHA-256", cb);
            EXPECT_EQ(hash, EXP_SHA256) << "bufSize = " << bufSize;
        }

        // SHA3-256
        {
            const std::string hash =
                compute_file_hash(tmpPath, bufSize, "SHA-3(256)", cb);
            EXPECT_EQ(hash, EXP_SHA3_256) << "bufSize = " << bufSize;
        }

        // CRC32
        {
            const std::string hash =
                compute_file_hash(tmpPath, bufSize, "CRC32", cb);
            EXPECT_EQ(hash, EXP_CRC32) << "bufSize = " << bufSize;
        }
    }
}
