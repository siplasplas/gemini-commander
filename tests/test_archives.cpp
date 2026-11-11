// Archives without the archive module or windows: the rules of previews
// (ArchiveUtils), the module's items as entries, the directories of an archive.
#include <gtest/gtest.h>

#include "ArchiveUtils.h"
#include "Archives.h"
#include "siplasplas/plugin/ArchivePlugin.h"

#include <QLocale>
#include <QTimeZone>

using namespace ArchiveUtils;

namespace {

constexpr qint64 MiB = 1024 * 1024;

// Files of the given sizes (MiB), named f0, f1, ...: their order and ends.
void files(const QList<qint64>& sizes, QStringList& order, QList<qint64>& ends)
{
    qint64 end = 0;
    for (int i = 0; i < sizes.size(); ++i) {
        end += sizes[i] * MiB;
        order << QStringLiteral("f%1").arg(i);
        ends << end;
    }
}

ArchiveEntry entry(const QString& path, bool isDirectory = false)
{
    ArchiveEntry e;
    e.path = path;
    e.name = path.section(QLatin1Char('/'), -1);
    e.isDirectory = isDirectory;
    return e;
}

QStringList names(const QList<ArchiveEntry>& entries)
{
    QStringList result;
    for (const ArchiveEntry& e : entries)
        result << e.name;
    result.sort();
    return result;
}

} // namespace

TEST(ArchivesTest, SizeInBytes)
{
    EXPECT_EQ(sizeInBytes(""), 0);
    EXPECT_EQ(sizeInBytes("  "), 0);
    EXPECT_EQ(sizeInBytes("1000"), 1000);
    EXPECT_EQ(sizeInBytes("512k"), 512 * 1024);
    EXPECT_EQ(sizeInBytes("100m"), 100 * MiB);
    EXPECT_EQ(sizeInBytes("100M"), 100 * MiB);
    EXPECT_EQ(sizeInBytes("1.5g"), 1536 * MiB);
}

TEST(ArchivesTest, StreamArchives)
{
    for (const char* name : {"a.tar.gz", "a.tar.zst", "A.TGZ", "a.tbz2", "a.txz", "a.7z", "a.7z.001"})
        EXPECT_TRUE(isStreamArchive(name)) << name;
    for (const char* name : {"a.zip", "a.rar", "a.part1.rar", "a.iso", "a.tar", "tar.gz.txt"})
        EXPECT_FALSE(isStreamArchive(name)) << name;
}

TEST(ArchivesTest, ReadsAheadAsFarAgain)
{
    QStringList order;
    QList<qint64> ends;
    files({50, 50, 50, 50, 50, 50, 50, 50, 50, 50}, order, ends);  // 10 files of 50 MiB
    const qint64 ahead = 16 * MiB;

    // f0 reached at 50 MiB: on to 100 MiB, f1.
    EXPECT_EQ(readUntil(order, ends, "f0", true, ahead), "f1");
    // f2 reached at 150 MiB: on to 300 MiB, f5.
    EXPECT_EQ(readUntil(order, ends, "f2", true, ahead), "f5");
    // f4 in the middle (250 MiB): to the end.
    EXPECT_EQ(readUntil(order, ends, "f4", true, ahead), "f9");
    EXPECT_EQ(readUntil(order, ends, "f9", true, ahead), "f9");

    // Small files at the start: at least `ahead` further.
    QStringList smallOrder;
    QList<qint64> smallEnds;
    files({1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, smallOrder, smallEnds);
    EXPECT_EQ(readUntil(smallOrder, smallEnds, "f0", true, ahead), "f16");  // 1 + 16 MiB

    // Read file by file (zip), or not listed: just the file.
    EXPECT_EQ(readUntil(order, ends, "f2", false, ahead), "f2");
    EXPECT_EQ(readUntil(order, ends, "missing", true, ahead), "missing");
}

TEST(ArchivesTest, PreviewPaths)
{
    QLocale::setDefault(QLocale::c());  // "1.00 MiB"
    const QString dir = "/tmp/preview";

    EXPECT_EQ(previewPath(dir, "a.txt", 10, MiB), "/tmp/preview/a.txt");
    EXPECT_EQ(previewPath(dir, "d/sub/a.txt", MiB, MiB), "/tmp/preview/d/sub/a.txt");  // all of it kept
    EXPECT_EQ(previewPath(dir, "logs/app.log", 5 * MiB, MiB), "/tmp/preview/logs/app (first 1.00 MiB).log");
    // Without an extension, and a dot in a directory's name only.
    EXPECT_EQ(previewPath(dir, "README", 5 * MiB, MiB), "/tmp/preview/README (first 1.00 MiB)");
    EXPECT_EQ(previewPath(dir, "v1.2/README", 5 * MiB, MiB), "/tmp/preview/v1.2/README (first 1.00 MiB)");
    EXPECT_EQ(previewPath(dir, ".bashrc", 5 * MiB, MiB), "/tmp/preview/.bashrc (first 1.00 MiB)");
    // Not out of the directory.
    EXPECT_EQ(previewPath(dir, "../evil", 1, MiB), "");
    EXPECT_EQ(previewPath(dir, "d/../../evil", 1, MiB), "");
    EXPECT_EQ(previewPath(dir, "d/../ok", 1, MiB), "/tmp/preview/ok");
}

TEST(ArchivesTest, TakesTheModulesItems)
{
    // The module's items (by libarchive or, for lsar's formats, parsed by
    // the module itself) as the panel's entries.
    QList<spp::ArchiveItem> items(3);
    items[0].path = "Fen";
    items[0].isDirectory = true;
    items[1].path = "Fen/a.fen";
    items[1].size = 44;
    items[1].modTime = QDateTime(QDate(1999, 8, 13), QTime(15, 12, 26), QTimeZone::UTC);
    // items[2]: no path, left out
    ArchiveContents contents;
    appendArchiveItems(contents, items);
    ASSERT_EQ(contents.allEntries.size(), 2);
    EXPECT_EQ(contents.allEntries[0].name, "Fen");
    EXPECT_TRUE(contents.allEntries[0].isDirectory);
    EXPECT_EQ(contents.allEntries[1].name, "a.fen");
    EXPECT_EQ(contents.allEntries[1].size, 44);
    EXPECT_EQ(contents.allEntries[1].modTime.toUTC().toString(Qt::ISODate), "1999-08-13T15:12:26Z");
    EXPECT_EQ(contents.entriesAt("Fen").size(), 1);
}

TEST(ArchivesTest, DirectoriesOfAnArchive)
{
    ArchiveContents contents;
    // "src" has no entry of its own, as in many zips.
    contents.allEntries = {entry("README"), entry("docs", true), entry("docs/a.md"),
                           entry("src/main.cpp"), entry("src/util/x.cpp")};

    EXPECT_EQ(names(contents.entriesAt("")), QStringList({"README", "docs", "src"}));
    EXPECT_EQ(names(contents.entriesAt("src")), QStringList({"main.cpp", "util"}));
    EXPECT_EQ(names(contents.entriesAt("src/util")), QStringList({"x.cpp"}));
    EXPECT_TRUE(contents.isDirectory("src"));
    EXPECT_TRUE(contents.isDirectory("docs"));
    EXPECT_FALSE(contents.isDirectory("README"));
    EXPECT_FALSE(contents.isDirectory("sr"));

    EXPECT_FALSE(archiveHasSingleRoot(contents));
    ArchiveContents single;
    single.allEntries = {entry("project", true), entry("project/a"), entry("project/b/c")};
    EXPECT_TRUE(archiveHasSingleRoot(single));
}
