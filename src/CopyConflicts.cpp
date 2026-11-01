#include "CopyConflicts.h"

#include <QDateTime>
#include <QDir>

namespace FileOperations {

bool targetExists(const QFileInfo &info) {
    return info.exists() || info.isSymLink();
}

bool isRealDir(const QFileInfo &info) {
    return info.isDir() && !info.isSymLink();
}

int compareTimes(const QFileInfo &src, const QFileInfo &dst) {
    const qint64 diff = src.lastModified().toMSecsSinceEpoch() - dst.lastModified().toMSecsSinceEpoch();
    if (diff >= kSameTimeMs)
        return 1;
    if (diff <= -kSameTimeMs)
        return -1;
    return 0;
}

bool overwriteAllowed(ConflictPolicy policy, const QFileInfo &src, const QFileInfo &dst) {
    switch (policy) {
        case ConflictPolicy::Overwrite:
            return true;
        case ConflictPolicy::Skip:
            return false;
        case ConflictPolicy::OverwriteOlder:
            return compareTimes(src, dst) > 0;
    }
    return false;
}

QFileInfoList dirEntries(const QString &path) {
    return QDir(path).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System, QDir::Name);
}

bool scanConflicts(const QFileInfo &src, const QString &dst, bool sameFs,
                   const std::function<bool(const QFileInfo&)> &skip, ConflictScan &scan,
                   const std::function<bool(const ConflictScan&)> &keepGoing) {
    if (skip && skip(src))
        return true;
    if (++scan.entries % 200 == 0 && keepGoing && !keepGoing(scan))
        return false;
    const QFileInfo dstInfo(dst);
    if (!targetExists(dstInfo))
        return true;
    if (src.isSymLink() && !sameFs)
        return true;  // not copied across filesystems
    if (isRealDir(src)) {
        if (!isRealDir(dstInfo))
            return true;
        for (const QFileInfo &child : dirEntries(src.absoluteFilePath())) {
            if (!scanConflicts(child, QDir(dst).filePath(child.fileName()), sameFs, skip, scan, keepGoing))
                return false;
        }
        return true;
    }
    if (isRealDir(dstInfo))
        return true;
    scan.conflicts++;
    if (compareTimes(src, dstInfo) > 0)
        scan.newer++;
    if (scan.examples.size() < 5)
        scan.examples << dst;
    return true;
}

} // namespace FileOperations
