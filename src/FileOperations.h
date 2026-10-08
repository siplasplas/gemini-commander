#ifndef FILEOPERATIONS_H
#define FILEOPERATIONS_H

#include <QMessageBox>
#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>
#include <optional>

class QWidget;
class QProgressDialog;
class FileOperationProgressDialog;
class SearchIgnore;

namespace FileOperations {

class DestSync;  // defined in FileOperations.cpp

/// Regular files and their bytes to copy, as counted by countCopyWork().
struct CopyTotals {
    quint64 files = 0;
    quint64 bytes = 0;
};

struct Params {
    bool valid = false;
    QString srcPath;
    QStringList names;
    QString destPath;
    bool useGitignore = false;  ///< copy: skip what .gitignore files ignore
    std::optional<CopyTotals> totals;  ///< counted in the dialog already
};

struct CopyStats {
    quint64 totalBytes = 0;
    quint64 bytesOnDisk = 0;
    quint64 totalFiles = 0;
    quint64 totalDirs  = 0;
    quint64 symlinks = 0;
};

enum class EnsureDirResult { Created, Exists, Cancelled, NotADir };

// Get filesystem cluster size for a path
quint64 getClusterSize(const QString& path);

// Calculate size of a single file or directory (with subdirectories)
// Updates stats with file count, dir count, symlinks, totalBytes and bytesOnDisk
void calculateEntrySize(const QString& path, CopyStats& stats, quint64 clusterSize, bool* cancelFlag = nullptr);

// Calculate size of multiple entries (files/directories) from a list of names
// basePath is the directory containing the entries
void calculateEntriesSize(const QString& basePath, const QStringList& names, CopyStats& stats, bool* cancelFlag = nullptr);

// Thread-safe atomic stats for live progress updates
struct AtomicStats {
    std::atomic<quint64>* totalFiles;
    std::atomic<quint64>* totalDirs;
    std::atomic<quint64>* totalBytes;
    std::atomic<quint64>* bytesOnDisk;
    std::atomic<quint64>* symlinks;
};

// Calculate size with atomic stats for thread-safe progress updates
void calculateEntrySizeAtomic(const QString& path, AtomicStats& stats, quint64 clusterSize, std::atomic<bool>* cancelFlag);

// Collect statistics about directory to copy (file count, total size)
void collectCopyStats(const QString& srcPath, CopyStats& stats, bool& ok, bool* cancelFlag = nullptr);

// Count the actual copy work (regular files and their logical bytes) for the
// given entries, used to drive byte-proportional progress. Uses SortedDirIterator
// so it does not follow symbolic links. keepGoing(files, bytes) gets the counts
// so far every 200 entries and once at the end; counting stops when it returns
// false. Thread-safe as long as `ignore` is used by this call only.
// Entries `ignore` skips are not counted (directories not entered).
void countCopyWork(const QString& basePath, const QStringList& names,
                   quint64& outFiles, quint64& outBytes,
                   const std::function<bool(quint64, quint64)>& keepGoing,
                   SearchIgnore* ignore = nullptr);

// Copy directory recursively with byte-proportional progress tracking.
// stats.symlinks is updated if symlinks are skipped (cross-FS).
// Entries `ignore` skips are not copied (directories not entered).
QMessageBox::Button copyOrMoveDirectoryRecursive(const QString& srcRoot, const QString& dstRoot,
                            bool move, bool sameFs,
                            QMessageBox::Button askPolice, CopyStats& stats,
                            FileOperationProgressDialog& progress, DestSync& sync,
                            SearchIgnore* ignore = nullptr);

// Check if target is invalid (same path or subdirectory of source)
bool isInvalidCopyMoveTarget(const QString& srcPath, const QString& dstPath);

// Ensure destination directory exists, prompting user if needed
EnsureDirResult ensureDestDirExists(const QString& dstPath, QWidget* parent);

// Check if destination input indicates a directory
bool isDestinationDirectory(const QString& destInput, const QString& dstPath);

// useGitignore (copy only): skip what .gitignore files ignore, read from
// currentPath down, exactly as Find Files does (SearchIgnore).
// totals: the work counted beforehand (with the same useGitignore), so it is
// not counted again; without it the files are counted first.
QString executeCopyOrMove(const QString& currentPath, const QStringList& names,
                    const QString& destInput, bool move,
                    QWidget* parent, bool useGitignore = false,
                    std::optional<CopyTotals> totals = std::nullopt);

} // namespace FileOperations

#endif // FILEOPERATIONS_H