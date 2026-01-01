#ifndef FILEOPERATIONS_H
#define FILEOPERATIONS_H

#include <QMessageBox>
#include <QString>
#include <QStringList>

class QWidget;
class QProgressDialog;
class FileOperationProgressDialog;

namespace FileOperations {

struct Params {
    bool valid = false;
    QString srcPath;
    QStringList names;
    QString destPath;
};

struct CopyStats {
    quint64 totalBytes = 0;
    quint64 totalFiles = 0;
    quint64 totalDirs  = 0;
    quint64 skippedSymlinks = 0;  // Symlinks skipped (cross-FS operations)
};

enum class EnsureDirResult { Created, Exists, Cancelled, NotADir };

// Collect statistics about directory to copy (file count, total size)
void collectCopyStats(const QString& srcPath, CopyStats& stats, bool& ok, bool* cancelFlag = nullptr);

// Copy directory recursively with progress tracking
// stats.skippedSymlinks is updated if symlinks are skipped (cross-FS)
QMessageBox::Button copyOrMoveDirectoryRecursive(const QString& srcRoot, const QString& dstRoot,
                            bool move, bool sameFs,
                            QMessageBox::Button askPolice, CopyStats& stats,
                            FileOperationProgressDialog& progress, quint64& bytesCopied);

// Check if target is invalid (same path or subdirectory of source)
bool isInvalidCopyMoveTarget(const QString& srcPath, const QString& dstPath);

// Ensure destination directory exists, prompting user if needed
EnsureDirResult ensureDestDirExists(const QString& dstPath, QWidget* parent);

// Check if destination input indicates a directory
bool isDestinationDirectory(const QString& destInput, const QString& dstPath);

// Copy a directory recursively with progress dialog
// Returns true on success, false on failure or user abort
// If deleteSourceAfter is true, removes source after successful copy
bool copyOrMoveDirectoryWithProgress(const QString& srcPath, const QString& dstPath,
                               const QString& displayName, bool deleteSourceAfter, QWidget* parent);

QString executeCopyOrMove(const QString& currentPath, const QStringList& names,
                    const QString& destInput, bool move,
                    QWidget* parent);

} // namespace FileOperations

#endif // FILEOPERATIONS_H