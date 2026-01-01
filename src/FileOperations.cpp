#include "FileOperations.h"
#include "FileOperationProgressDialog.h"
#include "SortedDirIterator.h"
#include "quitls.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>

#include <limits>

namespace FileOperations {

QMessageBox::StandardButton askOverwriteMulti(QWidget *parent, const QString &name) {
    auto reply = QMessageBox::question(
            parent, QObject::tr("Overwrite"), QObject::tr("'%1' already exists.\nOverwrite?").arg(name),
            QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Abort,
            QMessageBox::Yes);
    return reply;
}

QMessageBox::StandardButton askOverwriteSingle(QWidget *parent, const QString &name) {
    auto reply = QMessageBox::question(parent, QObject::tr("Overwrite"),
                                       QObject::tr("'%1' already exists.\nOverwrite?").arg(name),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    return reply;
}

bool copyFile(const QString &srcPath, const QString &dstPath) {
    if (!QFile::copy(srcPath, dstPath)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, dstPath));
        return false;
    }
    finalizeCopiedFile(srcPath, dstPath);
    return true;
}

// Copy a symbolic link (creates new link with same target, doesn't follow it)
bool copySymlink(const QString &srcPath, const QString &dstPath) {
    QString target = QFile::symLinkTarget(srcPath);
    if (target.isEmpty()) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to read symlink target:\n%1").arg(srcPath));
        return false;
    }
    // Remove existing file/link at destination if exists
    if (QFileInfo::exists(dstPath)) {
        QFile::remove(dstPath);
    }
    if (!QFile::link(target, dstPath)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to create symlink:\n%1\n->\n%2").arg(dstPath, target));
        return false;
    }
    return true;
}

QMessageBox::Button copyFileAskOverwrite(const QString &srcPath, const QString &dstPath, bool multi,
                                         QMessageBox::Button askPolice, QWidget *parent) {
    QFileInfo dstInfo(dstPath);
    QMessageBox::Button result = QMessageBox::Yes;
    if (dstInfo.exists()) {
        QMessageBox::Button reply;
        switch (askPolice) {
            case QMessageBox::Yes:
            case QMessageBox::No:
                if (multi)
                    reply = askOverwriteMulti(parent, dstPath);
                else
                    reply = askOverwriteSingle(parent, dstPath);
                break;
            default:
                reply = askPolice;
        }
        if (reply == QMessageBox::Abort)
            return QMessageBox::Abort;
        if (reply == QMessageBox::No)
            return QMessageBox::No;
        if (reply == QMessageBox::NoToAll)
            return QMessageBox::NoToAll;
        if (reply == QMessageBox::YesToAll)
            result = QMessageBox::YesToAll;
        QFile::remove(dstPath);
    }
    if (!copyFile(srcPath, dstPath))
        return QMessageBox::No;
    return result;
}

QMessageBox::Button copyOrMoveFileAskOverwrite(const QString &srcPath, const QString &dstPath, bool move, bool multi,
                                               QMessageBox::Button askPolice, QWidget *parent) {
    auto reply = copyFileAskOverwrite(srcPath, dstPath, multi, askPolice, parent);
    if (move)
        if (reply == QMessageBox::Yes || reply == QMessageBox::YesToAll)
            QFile::remove(srcPath);
    return reply;
}

void collectCopyStats(const QString &srcPath, CopyStats &stats, bool &ok, bool *cancelFlag) {
    ok = true;

    QFileInfo rootInfo(srcPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        ok = false;
        return;
    }

    // Count root directory too
    stats.totalDirs += 1;

    SortedDirIterator it(srcPath, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

    int counter = 0;
    while (it.hasNext()) {
        // Check for cancellation every 100 iterations
        if (cancelFlag && *cancelFlag) {
            ok = false;
            return;
        }

        // Process events every 100 iterations to allow UI to respond
        if (++counter % 100 == 0) {
            QCoreApplication::processEvents();
        }

        it.next();
        const QFileInfo fi = it.fileInfo();

        if (fi.isDir()) {
            stats.totalDirs += 1;
        } else if (fi.isFile()) {
            stats.totalFiles += 1;
            stats.totalBytes += static_cast<quint64>(fi.size());
        }
    }
}

QMessageBox::Button copyOrMoveDirectoryRecursive(const QString &srcRoot, const QString &dstRoot, bool move,
                                                 bool sameFs, QMessageBox::Button askPolice, CopyStats &stats,
                                                 FileOperationProgressDialog &progress, quint64 &bytesCopied) {
    if (askPolice == QMessageBox::Abort)
        return QMessageBox::Abort;

    QFileInfo srcInfo(srcRoot);
    if (!srcInfo.exists() || !srcInfo.isDir())
        return askPolice;

    QDir dstDir;
    if (!dstDir.mkpath(dstRoot)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to create directory:\n%1").arg(dstRoot));
        return askPolice;
    }

    QDir dir(srcRoot);

    // Get all entries including symlinks (System flag includes symlinks on Unix)
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System,
        QDir::Name);

    for (const QFileInfo &fi: entries) {
        // Handle Cancel
        if (progress.wasCanceled()) {
            auto reply = QMessageBox::question(nullptr, QObject::tr("Cancel copy"),
                                               QObject::tr("Do you really want to cancel the copy operation?"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (reply == QMessageBox::Yes)
                return QMessageBox::Abort;
        }

        const QString srcPath = fi.absoluteFilePath();
        const QString dstPath = QDir(dstRoot).filePath(fi.fileName());

        // Handle symbolic links
        if (fi.isSymLink()) {
            if (sameFs) {
                // Same FS: copy the symlink itself (don't follow)
                copySymlink(srcPath, dstPath);
                if (move) {
                    QFile::remove(srcPath);
                }
            } else {
                // Cross-FS: skip symlinks
                stats.skippedSymlinks++;
            }
            continue;
        }

        // Handle regular files
        if (fi.isFile()) {
            askPolice = copyOrMoveFileAskOverwrite(srcPath, dstPath, move, true, askPolice, &progress);

            if (askPolice == QMessageBox::Abort)
                return QMessageBox::Abort;
            if (askPolice == QMessageBox::Yes || askPolice == QMessageBox::YesToAll) {
                bytesCopied += static_cast<quint64>(fi.size());
            }
        }
        // Handle directories (recursive)
        else if (fi.isDir()) {
            askPolice = copyOrMoveDirectoryRecursive(srcPath, dstPath, move, sameFs, askPolice, stats, progress, bytesCopied);

            if (askPolice == QMessageBox::Abort)
                return QMessageBox::Abort;
        }

        qApp->processEvents();
    }
    return askPolice;
}

bool isInvalidCopyMoveTarget(const QString &srcPath, const QString &dstPath) {
    QFileInfo srcInfo(srcPath);
    QFileInfo dstInfo(dstPath);

    QString srcCanonical = srcInfo.canonicalFilePath();
    // For destination that may not exist yet, get canonical path of existing parent
    QString dstCanonical =
            dstInfo.exists() ? dstInfo.canonicalFilePath() : QFileInfo(dstInfo.absolutePath()).canonicalFilePath();

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

EnsureDirResult ensureDestDirExists(const QString &dstPath, QWidget *parent) {
    QFileInfo dstInfo(dstPath);
    if (!dstInfo.exists()) {
        auto reply = QMessageBox::question(parent, QObject::tr("Create Directory"),
                                           QObject::tr("Directory '%1' does not exist.\nCreate it?").arg(dstPath),
                                           QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
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

bool isDestinationDirectory(const QString &destInput, const QString &dstPath) {
    QFileInfo dstInfo(dstPath);
    return destInput.endsWith('/') || destInput == "." || destInput == ".." || destInput.endsWith("/.") ||
           destInput.endsWith("/..") || (dstInfo.exists() && dstInfo.isDir());
}

// Calculate absolute destination path from user input
QString resolveDstPath(const QString &currentPath, const QString &destInput) {
    if (QDir::isAbsolutePath(destInput))
        return destInput;
    return QDir(currentPath).absoluteFilePath(destInput);
}

// Calculate final target path for single file operation
QString resolveTargetPath(const QString &srcName, const QString &destInput, const QString &dstPath) {
    if (isDestinationDirectory(destInput, dstPath))
        return QDir(dstPath).filePath(srcName);
    return dstPath;
}

// Ensure parent directory exists, creating if needed
void ensureParentDirExists(const QString &filePath) {
    QDir parentDir = QFileInfo(filePath).absoluteDir();
    if (!parentDir.exists())
        parentDir.mkpath(".");
}

// Remove existing file or directory
void removeExisting(const QString &path) {
    QFileInfo info(path);
    if (info.isDir())
        QDir(path).removeRecursively();
    else
        QFile::remove(path);
}

QString executeCopyOrMove(const QString &currentPath, const QStringList &names, const QString &destInput,
                            bool move, QWidget *parent) {
    if (names.isEmpty())
        return {};

    quint64 bytesCopied = 0;
    CopyStats stats;
    bool statsOk = false;
    collectCopyStats(currentPath, stats, statsOk);
    QString dstPath = resolveDstPath(currentPath, destInput);

    // Treat destination as directory when: ends with '/' OR multiple files
    bool destIsDir = destInput.endsWith('/') || names.size() > 1;
    if (destIsDir) {
        auto ensureResult = ensureDestDirExists(dstPath, parent);
        if (ensureResult == EnsureDirResult::Cancelled || ensureResult == EnsureDirResult::NotADir)
            return {};
    } else {
        ensureParentDirExists(dstPath);
    }

    QDir srcDir(currentPath);
    FileOperationProgressDialog progressDlg(QObject::tr("Copy"), static_cast<int>(names.size()), parent);
    progressDlg.show();
    progressDlg.activateWindow();
    qApp->processEvents();

    int currentFile = 0;
    QMessageBox::Button askPolice = QMessageBox::Yes;
    for (const QString &name: names) {
        QString srcPath = srcDir.absoluteFilePath(name);
        QString dstFilePath = destIsDir ? QDir(dstPath).filePath(name) : dstPath;
        QFileInfo srcInfo(srcPath);

        if (isInvalidCopyMoveTarget(srcPath, dstFilePath))
            continue;

        ++currentFile;
        progressDlg.updateProgress(currentFile, name, srcInfo.size());
        if (progressDlg.wasCanceled())
            break;

        bool sameFs = areOnSameFilesystem(srcPath, dstFilePath);

        // Handle symbolic links
        if (srcInfo.isSymLink()) {
            if (sameFs) {
                if (move) {
                    // Same FS move: rename works for symlinks
                    QFile file(srcPath);
                    if (!file.rename(dstFilePath)) {
                        QMessageBox::warning(parent, QObject::tr("Error"),
                            QObject::tr("Failed to move symlink '%1'").arg(name));
                    }
                } else {
                    // Same FS copy: copy the symlink itself
                    copySymlink(srcPath, dstFilePath);
                }
            } else {
                // Cross-FS: skip symlinks
                stats.skippedSymlinks++;
            }
            continue;
        }

        if (move && sameFs) {
            // Same filesystem - use rename (fast move)
            progressDlg.updateMoveProgress(currentFile, 100);
            if (progressDlg.wasCanceled())
                break;

            QFile file(srcPath);
            if (!file.rename(dstFilePath)) {
                QMessageBox::warning(parent, QObject::tr("Error"),
                    QObject::tr("Failed to move '%1'").arg(name));
            }

            continue;
        }

        if (srcInfo.isFile()) {
            askPolice = copyOrMoveFileAskOverwrite(srcPath, dstFilePath,
                              move, names.size()>1, askPolice, &progressDlg);
        }
        else if (srcInfo.isDir()) {
            askPolice = copyOrMoveDirectoryRecursive(srcPath, dstFilePath, move,
                                             sameFs, askPolice, stats,
                                             progressDlg, bytesCopied);
            // After copying directory, delete source if move
            if (move && (askPolice == QMessageBox::Yes || askPolice == QMessageBox::YesToAll)) {
                QDir(srcPath).removeRecursively();
            }
        }
        if (progressDlg.wasCanceled())
            break;
    }

    // Show info message if symlinks were skipped
    if (stats.skippedSymlinks > 0) {
        QMessageBox::information(parent, QObject::tr("Symbolic Links Skipped"),
            QObject::tr("%1 symbolic link(s) were skipped.\n"
                        "Symbolic links cannot be copied/moved across different filesystems.")
            .arg(stats.skippedSymlinks));
    }

    if (names.size() == 1)
        return dstPath;
    else
        return {};
}

} // namespace FileOperations
