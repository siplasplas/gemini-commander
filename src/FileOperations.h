#ifndef FILEOPERATIONS_H
#define FILEOPERATIONS_H

#include <QString>
#include <QStringList>

class QWidget;
class FilePanel;

namespace FileOperations {

enum class EnsureDirResult { Created, Exists, Cancelled, NotADir };

// Check if target is invalid (same path or subdirectory of source)
bool isInvalidCopyMoveTarget(const QString& srcPath, const QString& dstPath);

// Ensure destination directory exists, prompting user if needed
EnsureDirResult ensureDestDirExists(const QString& dstPath, QWidget* parent);

// Check if destination input indicates a directory
bool isDestinationDirectory(const QString& destInput, const QString& dstPath);

// Copy a single file with progress dialog
bool copySingleFileWithProgress(const QString& srcPath, const QString& dstPath, QWidget* parent);

// Copy a directory recursively with progress dialog
// Returns true on success, false on failure or user abort
// If deleteSourceAfter is true, removes source after successful copy
bool copyDirectoryWithProgress(const QString& srcPath, const QString& dstPath,
                               const QString& displayName, bool deleteSourceAfter, QWidget* parent);

// Execute copy operation after destination path is confirmed
// srcPanel: source panel (for path and marked files)
// dstPanel: destination panel (may be null for in-place copy)
// dstPath: resolved absolute destination path
// destInput: original user input (for directory detection)
// currentName: name of single file (empty if multiple files marked)
// markedNames: list of marked file names (empty if single file)
void executeCopy(FilePanel* srcPanel, FilePanel* dstPanel,
                 const QString& dstPath, const QString& destInput,
                 const QString& currentName, const QStringList& markedNames,
                 QWidget* parent);

// Execute move operation after destination path is confirmed
void executeMove(FilePanel* srcPanel, FilePanel* dstPanel,
                 const QString& dstPath, const QString& destInput,
                 const QString& currentName, const QStringList& markedNames,
                 QWidget* parent);

} // namespace FileOperations

#endif // FILEOPERATIONS_H