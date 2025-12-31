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
                    const QString& destInput, const QString& baseDirForRelative,
                    QWidget* parent)
{
    if (names.isEmpty())
        return QString();

    // Calculate dstPath from destInput
    QString dstPath;
    if (QDir::isAbsolutePath(destInput)) {
        dstPath = destInput;
    } else {
        QDir baseDir(baseDirForRelative);
        dstPath = baseDir.absoluteFilePath(destInput);
    }

    QFileInfo dstInfo(dstPath);
    bool hasMultiple = names.size() > 1;

    if (hasMultiple) {
        // Multiple files: destination is ALWAYS treated as directory
        auto ensureResult = ensureDestDirExists(dstPath, parent);
        if (ensureResult == EnsureDirResult::Cancelled || ensureResult == EnsureDirResult::NotADir)
            return QString();

        // Copy all files
        QDir srcDir(currentPath);

        // Create progress dialog
        FileOperationProgressDialog progressDlg(QObject::tr("Copy"), static_cast<int>(names.size()), parent);
        progressDlg.show();
        progressDlg.processEvents();  // Ensure dialog is painted before starting

        int currentFile = 0;
        for (const QString& name : names) {
            QString srcPath = srcDir.absoluteFilePath(name);
            QString dstFilePath = QDir(dstPath).filePath(name);
            QFileInfo srcInfo(srcPath);

            // Skip if copying to same location or subdirectory of source
            if (isInvalidCopyMoveTarget(srcPath, dstFilePath))
                continue;

            ++currentFile;
            progressDlg.updateProgress(currentFile, name, srcInfo.size());
            if (progressDlg.wasCancelled())
                break;

            if (srcInfo.isFile()) {
                if (QFileInfo::exists(dstFilePath)) {
                    auto reply = QMessageBox::question(
                        parent, QObject::tr("Overwrite"),
                        QObject::tr("File '%1' already exists.\nOverwrite?").arg(name),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                        QMessageBox::Yes
                    );
                    if (reply == QMessageBox::Cancel)
                        break;
                    if (reply != QMessageBox::Yes)
                        continue;
                    QFile::remove(dstFilePath);
                }

                if (!QFile::copy(srcPath, dstFilePath)) {
                    QMessageBox::warning(parent, QObject::tr("Error"),
                        QObject::tr("Failed to copy '%1'").arg(name));
                } else {
                    finalizeCopiedFile(srcPath, dstFilePath);
                }
            } else if (srcInfo.isDir()) {
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
                bool userAbort = false;
                FilePanel::copyDirectoryRecursive(srcPath, dstFilePath, stats, progress, bytesCopied, userAbort);
                if (userAbort)
                    break;
            }

            if (progressDlg.wasCancelled())
                break;
        }

        return QString();  // Multiple files - no selection
    }

    // Single file copy
    const QString& currentName = names.first();
    QDir srcDir(currentPath);
    QString srcPath = srcDir.absoluteFilePath(currentName);
    QFileInfo srcInfo(srcPath);

    // Determine if destination is directory
    bool destIsDir = isDestinationDirectory(destInput, dstPath);

    QString finalDstPath;
    if (destIsDir) {
        // Copy into directory with original name
        QDir dstDir(dstPath);
        finalDstPath = dstDir.filePath(currentName);
    } else {
        // Destination is new filename
        finalDstPath = dstPath;
    }

    QFileInfo finalDstInfo(finalDstPath);

    // Check if copying to same location or subdirectory of source
    if (isInvalidCopyMoveTarget(srcPath, finalDstPath))
        return QString();

    if (srcInfo.isFile()) {
        if (finalDstInfo.exists()) {
            auto reply = QMessageBox::question(
                parent, QObject::tr("Overwrite"),
                QObject::tr("File '%1' already exists.\nOverwrite?").arg(finalDstInfo.fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );
            if (reply != QMessageBox::Yes)
                return QString();
            QFile::remove(finalDstPath);
        }

        // Ensure parent directory exists
        QDir parentDir = QFileInfo(finalDstPath).absoluteDir();
        if (!parentDir.exists()) {
            parentDir.mkpath(".");
        }

        // Show progress dialog for single file copy
        FileOperationProgressDialog progressDlg(QObject::tr("Copy"), 1, parent);
        progressDlg.show();
        progressDlg.updateProgress(1, srcInfo.fileName(), srcInfo.size());

        if (!QFile::copy(srcPath, finalDstPath)) {
            QMessageBox::warning(parent, QObject::tr("Error"),
                QObject::tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, finalDstPath));
            return QString();
        }
        finalizeCopiedFile(srcPath, finalDstPath);

        return QFileInfo(finalDstPath).fileName();
    }

    if (srcInfo.isDir()) {
        if (!copyDirectoryWithProgress(srcPath, finalDstPath, currentName, false, parent)) {
            return QString();
        }

        // Calculate final entry name for selection (matches helper's dstRoot calculation)
        QString entryName = (finalDstInfo.exists() && finalDstInfo.isDir())
            ? srcInfo.fileName()
            : QFileInfo(finalDstPath).fileName();
        return entryName;
    }

    return QString();
}

QString executeMove(const QString& currentPath, const QStringList& names,
                    const QString& destInput, const QString& baseDirForRelative,
                    QWidget* parent)
{
    if (names.isEmpty())
        return QString();

    // Calculate dstPath from destInput
    QString dstPath;
    if (QDir::isAbsolutePath(destInput)) {
        dstPath = destInput;
    } else {
        QDir baseDir(baseDirForRelative);
        dstPath = baseDir.absoluteFilePath(destInput);
    }

    QFileInfo dstInfo(dstPath);
    bool hasMultiple = names.size() > 1;

    if (hasMultiple) {
        // Multiple files: destination is ALWAYS treated as directory
        auto ensureResult = ensureDestDirExists(dstPath, parent);
        if (ensureResult == EnsureDirResult::Cancelled || ensureResult == EnsureDirResult::NotADir)
            return QString();

        // Move all files
        QDir srcDir(currentPath);

        // Create progress dialog
        FileOperationProgressDialog progressDlg(QObject::tr("Move"), static_cast<int>(names.size()), parent);
        progressDlg.show();
        progressDlg.processEvents();  // Ensure dialog is painted before starting

        int currentFile = 0;
        for (const QString& name : names) {
            QString srcPath = srcDir.absoluteFilePath(name);
            QString dstFilePath = QDir(dstPath).filePath(name);

            // Skip if moving to same location or subdirectory of source
            if (isInvalidCopyMoveTarget(srcPath, dstFilePath))
                continue;

            if (QFileInfo::exists(dstFilePath)) {
                auto reply = QMessageBox::question(
                    parent, QObject::tr("Overwrite"),
                    QObject::tr("'%1' already exists.\nOverwrite?").arg(name),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    QMessageBox::Yes
                );
                if (reply == QMessageBox::Cancel)
                    break;
                if (reply != QMessageBox::Yes)
                    continue;
                // Remove existing
                QFileInfo existingInfo(dstFilePath);
                if (existingInfo.isDir()) {
                    QDir(dstFilePath).removeRecursively();
                } else {
                    QFile::remove(dstFilePath);
                }
            }

            ++currentFile;

            // Check if same filesystem (true move) or different (copy+delete)
            QFileInfo srcFileInfo(srcPath);
            if (areOnSameFilesystem(srcPath, dstFilePath)) {
                // Same filesystem - use rename (fast move)
                // Update progress every 100th file (fast operation)
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
                // Update progress with filename (slower operation)
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
                    bool userAbort = false;
                    if (FilePanel::copyDirectoryRecursive(srcPath, dstFilePath, stats, progress, bytesCopied, userAbort)) {
                        // Copy succeeded, remove source
                        QDir(srcPath).removeRecursively();
                    }
                    if (userAbort)
                        break;
                }
            }

            if (progressDlg.wasCancelled())
                break;
        }

        return QString();  // Multiple files - no selection
    }

    // Single file move
    const QString& currentName = names.first();
    QDir srcDir(currentPath);
    QString srcPath = srcDir.absoluteFilePath(currentName);
    QFileInfo srcInfo(srcPath);

    // Determine if destination is directory
    bool destIsDir = isDestinationDirectory(destInput, dstPath);

    QString finalDstPath;
    if (destIsDir) {
        QDir dstDir(dstPath);
        finalDstPath = dstDir.filePath(currentName);
    } else {
        finalDstPath = dstPath;
    }

    QFileInfo finalDstInfo(finalDstPath);

    // Check if moving to same location or subdirectory of source
    if (isInvalidCopyMoveTarget(srcPath, finalDstPath))
        return QString();

    if (finalDstInfo.exists()) {
        auto reply = QMessageBox::question(
            parent, QObject::tr("Overwrite"),
            QObject::tr("'%1' already exists.\nOverwrite?").arg(finalDstInfo.fileName()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );
        if (reply != QMessageBox::Yes)
            return QString();
        if (finalDstInfo.isDir()) {
            QDir(finalDstPath).removeRecursively();
        } else {
            QFile::remove(finalDstPath);
        }
    }

    // Ensure parent directory exists
    QDir parentDir = QFileInfo(finalDstPath).absoluteDir();
    if (!parentDir.exists()) {
        parentDir.mkpath(".");
    }

    // Check if same filesystem (true move) or different (copy+delete)
    if (areOnSameFilesystem(srcPath, finalDstPath)) {
        // Same filesystem - use rename (fast move)
        // Show progress dialog for single file move
        FileOperationProgressDialog progressDlg(QObject::tr("Move"), 1, parent);
        progressDlg.show();
        progressDlg.updateMoveProgress(1, 1);  // Single file, show immediately

        QFile file(srcPath);
        if (!file.rename(finalDstPath)) {
            QMessageBox::warning(parent, QObject::tr("Error"),
                QObject::tr("Failed to move:\n%1\nto\n%2").arg(srcPath, finalDstPath));
            return QString();
        }
    } else {
        // Different filesystem - copy then delete
        if (srcInfo.isFile()) {
            // Show progress dialog for single file move (cross-partition)
            FileOperationProgressDialog progressDlg(QObject::tr("Move"), 1, parent);
            progressDlg.show();
            progressDlg.updateProgress(1, srcInfo.fileName(), srcInfo.size());

            if (!QFile::copy(srcPath, finalDstPath)) {
                QMessageBox::warning(parent, QObject::tr("Error"),
                    QObject::tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, finalDstPath));
                return QString();
            }
            finalizeCopiedFile(srcPath, finalDstPath);
            QFile::remove(srcPath);
        } else if (srcInfo.isDir()) {
            // Directory: recursive copy then delete
            FilePanel::CopyStats stats;
            bool statsOk = false;
            FilePanel::collectCopyStats(srcPath, stats, statsOk);

            QProgressDialog progress(
                QObject::tr("Moving %1...").arg(currentName), QObject::tr("Cancel"), 0,
                static_cast<int>(qMin<quint64>(stats.totalBytes, std::numeric_limits<int>::max())),
                parent
            );
            progress.setWindowModality(Qt::ApplicationModal);
            progress.setMinimumDuration(0);
            progress.show();

            quint64 bytesCopied = 0;
            bool userAbort = false;
            if (FilePanel::copyDirectoryRecursive(srcPath, finalDstPath, stats, progress, bytesCopied, userAbort)) {
                // Copy succeeded, remove source
                QDir(srcPath).removeRecursively();
            }
            if (userAbort)
                return QString();
        }
    }

    return QFileInfo(finalDstPath).fileName();
}

} // namespace FileOperations