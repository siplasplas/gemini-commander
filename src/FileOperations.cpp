#include "FileOperations.h"
#include "FilePanel.h"
#include "FileOperationProgressDialog.h"
#include "quitls.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>

#include <limits>

namespace FileOperations {

bool isInvalidCopyMoveTarget(const QString& srcPath, const QString& dstPath)
{
    QFileInfo srcInfo(srcPath);
    QFileInfo dstInfo(dstPath);

    QString srcCanonical = srcInfo.canonicalFilePath();
    // For destination that may not exist yet, get canonical path of existing parent
    QString dstCanonical = dstInfo.exists()
        ? dstInfo.canonicalFilePath()
        : QFileInfo(dstInfo.absolutePath()).canonicalFilePath();

    if (srcCanonical.isEmpty() || dstCanonical.isEmpty())
        return false;

    // Same path
    if (srcCanonical == dstCanonical)
        return true;

    // Destination is subdirectory of source (only when source is directory)
    if (srcInfo.isDir()) {
        QString srcWithSlash = srcCanonical;
        if (!srcWithSlash.endsWith('/'))
            srcWithSlash += '/';
        if (dstCanonical.startsWith(srcWithSlash))
            return true;
    }

    return false;
}

EnsureDirResult ensureDestDirExists(const QString& dstPath, QWidget* parent)
{
    QFileInfo dstInfo(dstPath);
    if (!dstInfo.exists()) {
        auto reply = QMessageBox::question(
            parent,
            QObject::tr("Create Directory"),
            QObject::tr("Directory '%1' does not exist.\nCreate it?").arg(dstPath),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes
        );
        if (reply == QMessageBox::Cancel)
            return EnsureDirResult::Cancelled;
        if (reply == QMessageBox::Yes) {
            QDir().mkpath(dstPath);
            return EnsureDirResult::Created;
        }
        return EnsureDirResult::Cancelled;
    }
    if (!dstInfo.isDir()) {
        QMessageBox::warning(parent, QObject::tr("Error"),
            QObject::tr("'%1' exists but is not a directory.").arg(dstPath));
        return EnsureDirResult::NotADir;
    }
    return EnsureDirResult::Exists;
}

bool isDestinationDirectory(const QString& destInput, const QString& dstPath)
{
    QFileInfo dstInfo(dstPath);
    return destInput.endsWith('/')
           || destInput == "." || destInput == ".."
           || destInput.endsWith("/.")  || destInput.endsWith("/..")
           || (dstInfo.exists() && dstInfo.isDir());
}

// Calculate absolute destination path from user input
QString resolveDstPath(const QString& currentPath, const QString& destInput)
{
    if (QDir::isAbsolutePath(destInput))
        return destInput;
    return QDir(currentPath).absoluteFilePath(destInput);
}

// Calculate final target path for single file operation
QString resolveTargetPath(const QString& srcName, const QString& destInput, const QString& dstPath)
{
    if (isDestinationDirectory(destInput, dstPath))
        return QDir(dstPath).filePath(srcName);
    return dstPath;
}

// Ensure parent directory exists, creating if needed
void ensureParentDirExists(const QString& filePath)
{
    QDir parentDir = QFileInfo(filePath).absoluteDir();
    if (!parentDir.exists())
        parentDir.mkpath(".");
}

// Remove existing file or directory
void removeExisting(const QString& path)
{
    QFileInfo info(path);
    if (info.isDir())
        QDir(path).removeRecursively();
    else
        QFile::remove(path);
}

// Ask user about overwriting - returns Yes/No/Cancel for multi-file, Yes/No for single
enum class OverwriteAnswer { Yes, No, Cancel };

OverwriteAnswer askOverwriteMulti(QWidget* parent, const QString& name)
{
    auto reply = QMessageBox::question(
        parent, QObject::tr("Overwrite"),
        QObject::tr("'%1' already exists.\nOverwrite?").arg(name),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes
    );
    if (reply == QMessageBox::Cancel) return OverwriteAnswer::Cancel;
    if (reply == QMessageBox::Yes) return OverwriteAnswer::Yes;
    return OverwriteAnswer::No;
}

bool askOverwriteSingle(QWidget* parent, const QString& name)
{
    auto reply = QMessageBox::question(
        parent, QObject::tr("Overwrite"),
        QObject::tr("'%1' already exists.\nOverwrite?").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );
    return reply == QMessageBox::Yes;
}

// Copy directory with progress dialog (used in multi-file loops)
// Returns: true to continue, false to break loop
bool copyDirInLoop(const QString& srcPath, const QString& dstPath, const QString& name, QWidget* parent, bool& userAbort)
{
    FilePanel::CopyStats stats;
    bool statsOk = false;
    FilePanel::collectCopyStats(srcPath, stats, statsOk);

    QProgressDialog progress(
        QObject::tr("Copying %1...").arg(name), QObject::tr("Cancel"), 0,
        static_cast<int>(qMin<quint64>(stats.totalBytes, std::numeric_limits<int>::max())),
        parent
    );
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.show();

    quint64 bytesCopied = 0;
    FilePanel::copyDirectoryRecursive(srcPath, dstPath, stats, progress, bytesCopied, userAbort);
    return !userAbort;
}

// Move directory across filesystems (copy + delete)
bool moveDirCrossFs(const QString& srcPath, const QString& dstPath, const QString& name, QWidget* parent, bool& userAbort)
{
    FilePanel::CopyStats stats;
    bool statsOk = false;
    FilePanel::collectCopyStats(srcPath, stats, statsOk);

    QProgressDialog progress(
        QObject::tr("Moving %1...").arg(name), QObject::tr("Cancel"), 0,
        static_cast<int>(qMin<quint64>(stats.totalBytes, std::numeric_limits<int>::max())),
        parent
    );
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.show();

    quint64 bytesCopied = 0;
    if (FilePanel::copyDirectoryRecursive(srcPath, dstPath, stats, progress, bytesCopied, userAbort)) {
        QDir(srcPath).removeRecursively();
        return true;
    }
    return false;
}

bool copySingleFileWithProgress(const QString& srcPath, const QString& dstPath, QWidget* parent)
{
    QFileInfo srcInfo(srcPath);

    // Ensure parent directory exists
    QDir parentDir = QFileInfo(dstPath).absoluteDir();
    if (!parentDir.exists()) {
        parentDir.mkpath(".");
    }

    // Show progress dialog
    FileOperationProgressDialog progressDlg(QObject::tr("Copy"), 1, parent);
    progressDlg.show();
    progressDlg.updateProgress(1, srcInfo.fileName(), srcInfo.size());

    if (!QFile::copy(srcPath, dstPath)) {
        QMessageBox::warning(parent, QObject::tr("Error"),
            QObject::tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, dstPath));
        return false;
    }
    finalizeCopiedFile(srcPath, dstPath);
    return true;
}

bool copyDirectoryWithProgress(const QString& srcPath, const QString& dstPath,
                               const QString& displayName, bool deleteSourceAfter, QWidget* parent)
{
    QFileInfo srcInfo(srcPath);
    QFileInfo dstInfo(dstPath);

    QString dstRoot = dstPath;
    if (dstInfo.exists() && dstInfo.isDir()) {
        QDir base(dstPath);
        dstRoot = base.filePath(srcInfo.fileName());
    }

    QDir checkDir(dstRoot);
    if (checkDir.exists()) {
        const QStringList entries = checkDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        if (!entries.isEmpty()) {
            QMessageBox::warning(parent, deleteSourceAfter ? QObject::tr("Move") : QObject::tr("Copy"),
                QObject::tr("Destination directory '%1' already exists and is not empty.\n"
                   "Recursive copy into non-empty directories is not supported yet.")
                    .arg(dstRoot));
            return false;
        }
    }

    FilePanel::CopyStats stats;
    bool statsOk = false;
    FilePanel::collectCopyStats(srcPath, stats, statsOk);
    if (!statsOk || stats.totalFiles == 0) {
        QMessageBox::warning(parent, deleteSourceAfter ? QObject::tr("Move") : QObject::tr("Copy"),
            QObject::tr("No files to copy in '%1'.").arg(srcPath));
        return false;
    }

    QString progressTitle = deleteSourceAfter
        ? QObject::tr("Moving %1...").arg(displayName)
        : QObject::tr("Copying %1 files (%2 bytes)...").arg(stats.totalFiles).arg(static_cast<qulonglong>(stats.totalBytes));

    QProgressDialog progress(
        progressTitle, QObject::tr("Cancel"), 0,
        static_cast<int>(qMin<quint64>(stats.totalBytes, std::numeric_limits<int>::max())),
        parent
    );
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.show();

    quint64 bytesCopied = 0;
    bool userAbort = false;

    bool okCopy = FilePanel::copyDirectoryRecursive(srcPath, dstRoot, stats, progress, bytesCopied, userAbort);

    if (okCopy && deleteSourceAfter) {
        QDir(srcPath).removeRecursively();
    }

    return okCopy && !userAbort;
}

QString executeCopy(const QString& currentPath, const QStringList& names,
                    const QString& destInput,
                    QWidget* parent)
{
    if (names.isEmpty())
        return {};

    QString dstPath = resolveDstPath(currentPath, destInput);

    if (names.size() > 1) {
        // Multiple files: destination is ALWAYS treated as directory
        auto ensureResult = ensureDestDirExists(dstPath, parent);
        if (ensureResult == EnsureDirResult::Cancelled || ensureResult == EnsureDirResult::NotADir)
            return {};

        QDir srcDir(currentPath);
        FileOperationProgressDialog progressDlg(QObject::tr("Copy"), static_cast<int>(names.size()), parent);
        progressDlg.show();
        progressDlg.processEvents();

        int currentFile = 0;
        for (const QString& name : names) {
            QString srcPath = srcDir.absoluteFilePath(name);
            QString dstFilePath = QDir(dstPath).filePath(name);
            QFileInfo srcInfo(srcPath);

            if (isInvalidCopyMoveTarget(srcPath, dstFilePath))
                continue;

            ++currentFile;
            progressDlg.updateProgress(currentFile, name, srcInfo.size());
            if (progressDlg.wasCancelled())
                break;

            if (srcInfo.isFile()) {
                if (QFileInfo::exists(dstFilePath)) {
                    auto answer = askOverwriteMulti(parent, name);
                    if (answer == OverwriteAnswer::Cancel) break;
                    if (answer == OverwriteAnswer::No) continue;
                    QFile::remove(dstFilePath);
                }

                if (!QFile::copy(srcPath, dstFilePath)) {
                    QMessageBox::warning(parent, QObject::tr("Error"),
                        QObject::tr("Failed to copy '%1'").arg(name));
                } else {
                    finalizeCopiedFile(srcPath, dstFilePath);
                }
            } else if (srcInfo.isDir()) {
                bool userAbort = false;
                if (!copyDirInLoop(srcPath, dstFilePath, name, parent, userAbort))
                    break;
            }

            if (progressDlg.wasCancelled())
                break;
        }
        return {};
    }

    // Single file copy
    const QString& currentName = names.first();
    QString srcPath = QDir(currentPath).absoluteFilePath(currentName);
    QString finalDstPath = resolveTargetPath(currentName, destInput, dstPath);
    QFileInfo srcInfo(srcPath);
    QFileInfo finalDstInfo(finalDstPath);

    if (isInvalidCopyMoveTarget(srcPath, finalDstPath))
        return {};

    if (srcInfo.isFile()) {
        if (finalDstInfo.exists()) {
            if (!askOverwriteSingle(parent, finalDstInfo.fileName()))
                return {};
            QFile::remove(finalDstPath);
        }

        ensureParentDirExists(finalDstPath);

        FileOperationProgressDialog progressDlg(QObject::tr("Copy"), 1, parent);
        progressDlg.show();
        progressDlg.updateProgress(1, srcInfo.fileName(), srcInfo.size());

        if (!QFile::copy(srcPath, finalDstPath)) {
            QMessageBox::warning(parent, QObject::tr("Error"),
                QObject::tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, finalDstPath));
            return {};
        }
        finalizeCopiedFile(srcPath, finalDstPath);
        return finalDstPath;
    }

    if (srcInfo.isDir()) {
        if (!copyDirectoryWithProgress(srcPath, finalDstPath, currentName, false, parent))
            return {};
        return finalDstPath;
    }

    return {};
}

QString executeMove(const QString& currentPath, const QStringList& names,
                    const QString& destInput,
                    QWidget* parent)
{
    if (names.isEmpty())
        return {};

    QString dstPath = resolveDstPath(currentPath, destInput);

    if (names.size() > 1) {
        // Multiple files: destination is ALWAYS treated as directory
        auto ensureResult = ensureDestDirExists(dstPath, parent);
        if (ensureResult == EnsureDirResult::Cancelled || ensureResult == EnsureDirResult::NotADir)
            return {};

        QDir srcDir(currentPath);
        FileOperationProgressDialog progressDlg(QObject::tr("Move"), static_cast<int>(names.size()), parent);
        progressDlg.show();
        progressDlg.processEvents();

        int currentFile = 0;
        for (const QString& name : names) {
            QString srcPath = srcDir.absoluteFilePath(name);
            QString dstFilePath = QDir(dstPath).filePath(name);

            if (isInvalidCopyMoveTarget(srcPath, dstFilePath))
                continue;

            if (QFileInfo::exists(dstFilePath)) {
                auto answer = askOverwriteMulti(parent, name);
                if (answer == OverwriteAnswer::Cancel) break;
                if (answer == OverwriteAnswer::No) continue;
                removeExisting(dstFilePath);
            }

            ++currentFile;
            QFileInfo srcFileInfo(srcPath);

            if (areOnSameFilesystem(srcPath, dstFilePath)) {
                // Same filesystem - use rename (fast move)
                progressDlg.updateMoveProgress(currentFile, 100);
                if (progressDlg.wasCancelled())
                    break;

                QFile file(srcPath);
                if (!file.rename(dstFilePath)) {
                    QMessageBox::warning(parent, QObject::tr("Error"),
                        QObject::tr("Failed to move '%1'").arg(name));
                }
            } else {
                // Different filesystem - copy then delete
                progressDlg.updateProgress(currentFile, name, srcFileInfo.size());
                if (progressDlg.wasCancelled())
                    break;

                if (srcFileInfo.isFile()) {
                    if (!QFile::copy(srcPath, dstFilePath)) {
                        QMessageBox::warning(parent, QObject::tr("Error"),
                            QObject::tr("Failed to copy '%1'").arg(name));
                        continue;
                    }
                    finalizeCopiedFile(srcPath, dstFilePath);
                    QFile::remove(srcPath);
                } else if (srcFileInfo.isDir()) {
                    bool userAbort = false;
                    moveDirCrossFs(srcPath, dstFilePath, name, parent, userAbort);
                    if (userAbort)
                        break;
                }
            }

            if (progressDlg.wasCancelled())
                break;
        }
        return {};
    }

    // Single file move
    const QString& currentName = names.first();
    QString srcPath = QDir(currentPath).absoluteFilePath(currentName);
    QString finalDstPath = resolveTargetPath(currentName, destInput, dstPath);
    QFileInfo srcInfo(srcPath);
    QFileInfo finalDstInfo(finalDstPath);

    if (isInvalidCopyMoveTarget(srcPath, finalDstPath))
        return {};

    if (finalDstInfo.exists()) {
        if (!askOverwriteSingle(parent, finalDstInfo.fileName()))
            return {};
        removeExisting(finalDstPath);
    }

    ensureParentDirExists(finalDstPath);

    if (areOnSameFilesystem(srcPath, finalDstPath)) {
        // Same filesystem - use rename (fast move)
        FileOperationProgressDialog progressDlg(QObject::tr("Move"), 1, parent);
        progressDlg.show();
        progressDlg.updateMoveProgress(1, 1);

        QFile file(srcPath);
        if (!file.rename(finalDstPath)) {
            QMessageBox::warning(parent, QObject::tr("Error"),
                QObject::tr("Failed to move:\n%1\nto\n%2").arg(srcPath, finalDstPath));
            return {};
        }
    } else {
        // Different filesystem - copy then delete
        if (srcInfo.isFile()) {
            FileOperationProgressDialog progressDlg(QObject::tr("Move"), 1, parent);
            progressDlg.show();
            progressDlg.updateProgress(1, srcInfo.fileName(), srcInfo.size());

            if (!QFile::copy(srcPath, finalDstPath)) {
                QMessageBox::warning(parent, QObject::tr("Error"),
                    QObject::tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, finalDstPath));
                return {};
            }
            finalizeCopiedFile(srcPath, finalDstPath);
            QFile::remove(srcPath);
        } else if (srcInfo.isDir()) {
            bool userAbort = false;
            moveDirCrossFs(srcPath, finalDstPath, currentName, parent, userAbort);
            if (userAbort)
                return {};
        }
    }

    return finalDstPath;
}

} // namespace FileOperations