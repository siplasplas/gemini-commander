#ifndef FILEOPERATIONS_H
#define FILEOPERATIONS_H

#include <QString>
#include <QStringList>

class QWidget;
class QProgressDialog;

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
};

enum class EnsureDirResult { Created, Exists, Cancelled, NotADir };

// Collect statistics about directory to copy (file count, total size)
void collectCopyStats(const QString& srcPath, CopyStats& stats, bool& ok, bool* cancelFlag = nullptr);

// Copy directory recursively with progress tracking
// Returns true on success, false on failure or user abort
bool copyDirectoryRecursive(const QString& srcRoot, const QString& dstRoot, const CopyStats& stats,
                            QProgressDialog& progress, quint64& bytesCopied, bool& userAbort);

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

// Execute copy operation after destination path is confirmed
// currentPath: source directory path (will be prepended to names)
// names: list of file/directory names to copy
// destInput: user input for destination (may be relative or absolute)
// baseDirForRelative: base directory for resolving relative destInput
// Returns: name of copied file for selection (empty if multiple files)
QString executeCopy(const QString& currentPath, const QStringList& names,
                    const QString& destInput,
                    QWidget* parent);

// Execute move operation after destination path is confirmed
// Returns: name of moved file for selection (empty if multiple files)
QString executeMove(const QString& currentPath, const QStringList& names,
                    const QString& destInput,
                    QWidget* parent);

} // namespace FileOperations

#endif // FILEOPERATIONS_H