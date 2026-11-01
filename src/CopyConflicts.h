#ifndef COPYCONFLICTS_H
#define COPYCONFLICTS_H

#include <QFileInfo>
#include <QStringList>
#include <functional>

// Targets that exist already when copying or moving (F5, F6): what counts as
// a conflict, the quick scan that finds them before copying, and whether a
// policy overwrites one. Only QtCore, no dialogs (see FileOperations).
namespace FileOperations {

/// What happens to a file whose target exists already; chosen once, before
/// copying or moving (see executeCopyOrMove).
enum class ConflictPolicy {
    Overwrite,       ///< always
    Skip,            ///< never; a moved file stays in the source
    OverwriteOlder,  ///< only when the source is newer than the target
};

/// Modification times closer than this count as the same: FAT keeps them to
/// 2 seconds, so a file copied there and back is not "newer".
constexpr qint64 kSameTimeMs = 2000;

/// A path taken at the destination, also by a broken symbolic link.
bool targetExists(const QFileInfo& info);

/// A real directory, not a symbolic link to one.
bool isRealDir(const QFileInfo& info);

/// 1 when the source is newer than the target, -1 when older, 0 the same.
int compareTimes(const QFileInfo& src, const QFileInfo& dst);

bool overwriteAllowed(ConflictPolicy policy, const QFileInfo& src, const QFileInfo& dst);

/// The entries a copy or move goes through in a directory, symbolic links and
/// hidden files included, by name.
QFileInfoList dirEntries(const QString& path);

/// Files (and symbolic links) whose target exists already.
struct ConflictScan {
    quint64 entries = 0;    ///< checked, for the progress
    quint64 conflicts = 0;
    quint64 newer = 0;      ///< of them, the source newer than the target
    QStringList examples;   ///< the first few target paths
};

/// Goes through `src` against its target `dst`; only directories that exist
/// at the destination are entered, the rest cannot conflict. Mismatches (a
/// file over a directory, a directory over a file) are reported when copying
/// and are not conflicts; nor are symbolic links across filesystems (not
/// copied). Entries `skip` accepts are left out. keepGoing(scan) is called
/// every 200 entries; false stops the scan and it returns false.
bool scanConflicts(const QFileInfo& src, const QString& dst, bool sameFs,
                   const std::function<bool(const QFileInfo&)>& skip, ConflictScan& scan,
                   const std::function<bool(const ConflictScan&)>& keepGoing);

} // namespace FileOperations

#endif // COPYCONFLICTS_H
