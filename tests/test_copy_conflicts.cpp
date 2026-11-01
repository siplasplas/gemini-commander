// Conflicts of Copy (F5) and Move (F6): the quick scan before copying and
// which targets a policy overwrites (src/CopyConflicts).
#include <gtest/gtest.h>

#include "CopyConflicts.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace FileOperations;

namespace {

// Writes `path` (parent directories too), modified `ageSec` seconds ago.
void put(const QString& path, qint64 ageSec)
{
    ASSERT_TRUE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.flush();  // or the write when closing sets the time again
    ASSERT_TRUE(file.setFileTime(QDateTime::currentDateTime().addSecs(-ageSec),
                                 QFileDevice::FileModificationTime));
}

ConflictScan scan(const QString& src, const QString& dst, bool sameFs = true,
                  const std::function<bool(const QFileInfo&)>& skip = {})
{
    ConflictScan result;
    EXPECT_TRUE(scanConflicts(QFileInfo(src), dst, sameFs, skip, result, {}));
    return result;
}

} // namespace

TEST(CopyConflictsTest, TimesWithinTwoSecondsAreTheSame)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString base = dir.path();
    put(base + "/now", 0);
    put(base + "/second", 1);
    put(base + "/hour", 3600);
    const QFileInfo now(base + "/now"), second(base + "/second"), hour(base + "/hour");

    EXPECT_EQ(compareTimes(now, second), 0);
    EXPECT_EQ(compareTimes(now, hour), 1);
    EXPECT_EQ(compareTimes(hour, now), -1);

    EXPECT_TRUE(overwriteAllowed(ConflictPolicy::Overwrite, hour, now));
    EXPECT_FALSE(overwriteAllowed(ConflictPolicy::Skip, now, hour));
    EXPECT_TRUE(overwriteAllowed(ConflictPolicy::OverwriteOlder, now, hour));
    EXPECT_FALSE(overwriteAllowed(ConflictPolicy::OverwriteOlder, hour, now));
    EXPECT_FALSE(overwriteAllowed(ConflictPolicy::OverwriteOlder, now, second));
}

TEST(CopyConflictsTest, ScanFindsExistingTargetsOnly)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString src = dir.path() + "/src/d", dst = dir.path() + "/dst/d";
    put(src + "/a", 0);        put(dst + "/a", 3600);       // newer source
    put(src + "/b", 3600);     put(dst + "/b", 0);          // older source
    put(src + "/sub/c", 0);    put(dst + "/sub/c", 0);      // same time, one level down
    put(src + "/e", 0);                                     // not at the destination
    put(src + "/new/f", 0);                                 // directory not at the destination
    put(src + "/x/g", 0);      put(dst + "/x", 0);          // directory over a file: not a conflict
    put(src + "/y", 0);        QDir().mkpath(dst + "/y/z"); // file over a directory: not a conflict

    const ConflictScan result = scan(src, dst);
    EXPECT_EQ(result.conflicts, 3u);
    EXPECT_EQ(result.newer, 1u);
    EXPECT_EQ(result.examples, QStringList({dst + "/a", dst + "/b", dst + "/sub/c"}));
    // Not entered where the destination has no directory: new/f, x/g.
    EXPECT_EQ(result.entries, 9u);  // d, a, b, e, new, sub, sub/c, x, y

    EXPECT_EQ(scan(src + "/e", dst + "/e").conflicts, 0u);
    EXPECT_EQ(scan(src + "/a", dst + "/a").conflicts, 1u);
}

TEST(CopyConflictsTest, SkippedEntriesAreNotConflicts)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString src = dir.path() + "/src", dst = dir.path() + "/dst";
    put(src + "/a", 0);       put(dst + "/a", 0);
    put(src + "/sub/b", 0);   put(dst + "/sub/b", 0);

    EXPECT_EQ(scan(src, dst).conflicts, 2u);
    const auto skipSub = [](const QFileInfo& fi) { return fi.fileName() == "sub"; };
    EXPECT_EQ(scan(src, dst, true, skipSub).conflicts, 1u);
}

TEST(CopyConflictsTest, SymlinksConflictOnTheSameFilesystemOnly)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString src = dir.path() + "/src", dst = dir.path() + "/dst";
    ASSERT_TRUE(QDir().mkpath(src));
    ASSERT_TRUE(QDir().mkpath(dst));
    ASSERT_TRUE(QFile::link("missing", src + "/link"));
    // A broken link takes the name at the destination too.
    ASSERT_TRUE(QFile::link("missing-too", dst + "/link"));
    EXPECT_TRUE(targetExists(QFileInfo(dst + "/link")));

    EXPECT_EQ(scan(src, dst, true).conflicts, 1u);
    EXPECT_EQ(scan(src, dst, false).conflicts, 0u);  // not copied across filesystems
}

TEST(CopyConflictsTest, KeepGoingStopsTheScan)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString src = dir.path() + "/src", dst = dir.path() + "/dst";
    for (int i = 0; i < 250; ++i) {
        put(src + QString("/f%1").arg(i), 0);
        put(dst + QString("/f%1").arg(i), 0);
    }
    int calls = 0;
    ConflictScan result;
    const bool finished = scanConflicts(QFileInfo(src), dst, true, {}, result,
                                        [&calls](const ConflictScan&) { ++calls; return false; });
    EXPECT_FALSE(finished);
    EXPECT_EQ(calls, 1);
    EXPECT_LT(result.conflicts, 250u);
}
