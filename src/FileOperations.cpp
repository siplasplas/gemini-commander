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
                                                 QMessageBox::Button askPolice, const CopyStats &stats,
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

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);

    const QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &fi: files) {
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

        askPolice = copyOrMoveFileAskOverwrite(srcPath, dstPath, move, true, askPolice, &progress);

        if (askPolice == QMessageBox::Abort)
            return QMessageBox::Abort;
        if (askPolice == QMessageBox::Yes || askPolice == QMessageBox::YesToAll) {
            bytesCopied += static_cast<quint64>(fi.size());
            if (stats.totalBytes > 0) {
                const int value = static_cast<int>(qMin<quint64>(bytesCopied, stats.totalBytes));
                //progress.setValue(value);
            }
        }
        qApp->processEvents();
    }

    for (const QFileInfo &fi: dirs) {
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
        askPolice = copyOrMoveDirectoryRecursive(srcPath, dstPath, move, askPolice, stats, progress, bytesCopied);

        if (askPolice == QMessageBox::Abort)
            return QMessageBox::Abort;
        if (askPolice == QMessageBox::Yes || askPolice == QMessageBox::YesToAll) {
            bytesCopied += static_cast<quint64>(fi.size());
            if (stats.totalBytes > 0) {
                const int value = static_cast<int>(qMin<quint64>(bytesCopied, stats.totalBytes));
                //progress.setValue(value);
            }
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
    progressDlg.processEvents();

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

        if (move && areOnSameFilesystem(srcPath, dstFilePath)) {
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
                                             askPolice, stats,
                                             progressDlg, bytesCopied);
        }
        if (progressDlg.wasCanceled())
            break;
    }
    if (names.size() == 1)
        return dstPath;
    else
        return {};
}

} // namespace FileOperations
