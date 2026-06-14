// _GNU_SOURCE must be defined before any system header so that syncfs() is
// declared by <unistd.h> on Linux.
#ifndef _WIN32
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "FileOperations.h"
#include "Config.h"
#include "FileOperationProgressDialog.h"
#include "SortedDirIterator.h"
#include "quitls.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

#include <limits>

namespace FileOperations {

// Flushes copied data to the destination's physical storage.
//
// fsync() works on one file descriptor, but a group of small files have their
// descriptors closed by the time they are done, so they cannot be flushed
// individually after the fact. Instead we sync the whole destination filesystem
// (Linux: syncfs() on a descriptor that lives on it - i.e. just that one volume,
// not the entire machine like sync()). Large files are synced immediately; small
// files are batched and synced once enough bytes have accumulated, plus once more
// at the end of the operation (or when the user interrupts it). On slow removable
// media this is dramatically faster than fsync-after-every-small-file while still
// guaranteeing everything is on disk before we report completion.
class DestSync {
public:
    explicit DestSync(const QString& destPath) {
        m_batchThreshold = static_cast<quint64>(Config::instance().syncBatchThresholdBytes());
        m_maxIntervalMs = Config::instance().syncBatchIntervalMs();
        m_timer.start();
#ifndef _WIN32
        QFileInfo info(destPath);
        QString dir = info.isDir() ? destPath : info.absolutePath();
        m_fd = ::open(dir.toLocal8Bit().constData(), O_RDONLY);
#endif
    }

    ~DestSync() {
        flush();
#ifndef _WIN32
        if (m_fd >= 0)
            ::close(m_fd);
#endif
    }

    // Called after each regular file is copied.
    void afterFileCopied(const QString& dstPath, qint64 fileSize, bool largeFile) {
#ifdef _WIN32
        // No whole-volume flush without admin rights; flush each file as before.
        flushFileWindows(dstPath);
#else
        Q_UNUSED(dstPath);
        if (largeFile || m_batchThreshold == 0) {
            doSync();  // large file (or batching disabled): sync right away
        } else {
            m_pending += static_cast<quint64>(fileSize);
            // Flush when enough bytes piled up, or when too much time has passed
            // since the last sync (so slowly-trickling files don't linger).
            bool byTime = m_maxIntervalMs > 0 && m_timer.elapsed() >= m_maxIntervalMs;
            if (m_pending > 0 && (m_pending >= m_batchThreshold || byTime))
                doSync();
        }
#endif
    }

    // Force a sync of everything still pending (end of operation / on abort).
    void flush() {
#ifndef _WIN32
        if (m_pending > 0)
            doSync();
#endif
    }

private:
#ifdef _WIN32
    void flushFileWindows(const QString& dstPath) {
        HANDLE h = CreateFileW(
            reinterpret_cast<LPCWSTR>(dstPath.utf16()),
            GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(h);
            CloseHandle(h);
        }
    }
#else
    void doSync() {
        if (m_fd >= 0) {
            if (::syncfs(m_fd) != 0)
                ::sync();  // fall back to a global sync if syncfs fails
        } else {
            ::sync();
        }
        m_pending = 0;
        m_timer.restart();
    }
    int m_fd = -1;
#endif
    quint64 m_pending = 0;
    quint64 m_batchThreshold = 10ULL * 1024 * 1024;
    int m_maxIntervalMs = 1000;
    QElapsedTimer m_timer;
};

// Helper: round up size to cluster boundary
static quint64 roundUpToCluster(quint64 size, quint64 clusterSize) {
    if (clusterSize == 0) return size;
    return ((size + clusterSize - 1) / clusterSize) * clusterSize;
}

quint64 getClusterSize(const QString& path) {
    if (path.isEmpty()) return 4096; // fallback

#ifdef _WIN32
    // Get root path (e.g., "C:\")
    QString rootPath = QFileInfo(path).absoluteFilePath();
    if (rootPath.length() >= 2 && rootPath[1] == ':') {
        rootPath = rootPath.left(3); // "C:\"
    }

    DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;
    if (GetDiskFreeSpaceW(reinterpret_cast<LPCWSTR>(rootPath.utf16()),
                          &sectorsPerCluster, &bytesPerSector,
                          &freeClusters, &totalClusters)) {
        return static_cast<quint64>(sectorsPerCluster) * bytesPerSector;
    }
    return 4096; // fallback
#else
    struct statvfs buf;
    QByteArray pathBytes = path.toLocal8Bit();
    if (statvfs(pathBytes.constData(), &buf) == 0) {
        return static_cast<quint64>(buf.f_bsize);
    }
    return 4096; // fallback
#endif
}

// Helper: calculate directory on-disk size for a single directory (not recursive)
static quint64 calculateDirOnDiskSize(const QString& path, [[maybe_unused]] const QFileInfo& fi, quint64 clusterSize) {
#ifdef _WIN32
    // Windows: estimate directory size based on entries
    // Base overhead for empty directory = 1 cluster
    // Each entry: ~64 bytes base + 2 bytes per Unicode char in filename
    QDir dir(path);
    QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

    quint64 dirSize = 64; // base overhead for directory itself
    for (const QString& name : entries) {
        dirSize += 64 + static_cast<quint64>(name.length()) * 2;
    }
    return roundUpToCluster(dirSize, clusterSize);
#else
    // Linux: directory has its own size directly from stat
    quint64 dirSize = static_cast<quint64>(fi.size());
    return roundUpToCluster(dirSize, clusterSize);
#endif
}

void calculateEntrySize(const QString& path, CopyStats& stats, quint64 clusterSize, bool* cancelFlag) {
    if (cancelFlag && *cancelFlag) return;

    QFileInfo fi(path);
    if (!fi.exists()) return;

    // Handle symbolic links first (before isFile/isDir checks)
    if (fi.isSymLink()) {
        stats.symlinks++;
        stats.bytesOnDisk += clusterSize; // symlink = 1 cluster
        return;
    }

    if (fi.isFile()) {
        stats.totalFiles++;
        quint64 size = static_cast<quint64>(fi.size());
        stats.totalBytes += size;
        stats.bytesOnDisk += (size == 0) ? 0 : roundUpToCluster(size, clusterSize);
        return;
    }

    if (fi.isDir()) {
        // Count the root directory
        stats.totalDirs++;
#ifndef _WIN32
        stats.totalBytes += static_cast<quint64>(fi.size());
#endif
        stats.bytesOnDisk += calculateDirOnDiskSize(path, fi, clusterSize);

        // SortedDirIterator iterates recursively through all subdirectories
        SortedDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        int counter = 0;
        while (it.hasNext()) {
            if (cancelFlag && *cancelFlag) return;

            if (++counter % 100 == 0) {
                QCoreApplication::processEvents();
            }

            it.next();
            const QString entryPath = it.filePath();
            const QFileInfo entryInfo = it.fileInfo();

            // Handle symbolic links
            if (entryInfo.isSymLink()) {
                stats.symlinks++;
                stats.bytesOnDisk += clusterSize;
                continue;
            }

            if (entryInfo.isFile()) {
                stats.totalFiles++;
                quint64 size = static_cast<quint64>(entryInfo.size());
                stats.totalBytes += size;
                stats.bytesOnDisk += (size == 0) ? 0 : roundUpToCluster(size, clusterSize);
            } else if (entryInfo.isDir()) {
                // Just count the directory itself, iterator will provide its contents
                stats.totalDirs++;
#ifndef _WIN32
                stats.totalBytes += static_cast<quint64>(entryInfo.size());
#endif
                stats.bytesOnDisk += calculateDirOnDiskSize(entryPath, entryInfo, clusterSize);
            }
        }
    }
}

void calculateEntrySizeAtomic(const QString& path, AtomicStats& stats, quint64 clusterSize, std::atomic<bool>* cancelFlag) {
    if (cancelFlag && cancelFlag->load()) return;

    QFileInfo fi(path);
    if (!fi.exists()) return;

    // Handle symbolic links first
    if (fi.isSymLink()) {
        (*stats.symlinks)++;
        (*stats.bytesOnDisk) += clusterSize;
        return;
    }

    if (fi.isFile()) {
        (*stats.totalFiles)++;
        quint64 size = static_cast<quint64>(fi.size());
        (*stats.totalBytes) += size;
        (*stats.bytesOnDisk) += (size == 0) ? 0 : roundUpToCluster(size, clusterSize);
        return;
    }

    if (fi.isDir()) {
        (*stats.totalDirs)++;
#ifndef _WIN32
        (*stats.totalBytes) += static_cast<quint64>(fi.size());
#endif
        (*stats.bytesOnDisk) += calculateDirOnDiskSize(path, fi, clusterSize);

        SortedDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        while (it.hasNext()) {
            if (cancelFlag && cancelFlag->load()) return;

            it.next();
            const QString entryPath = it.filePath();
            const QFileInfo entryInfo = it.fileInfo();

            if (entryInfo.isSymLink()) {
                (*stats.symlinks)++;
                (*stats.bytesOnDisk) += clusterSize;
                continue;
            }

            if (entryInfo.isFile()) {
                (*stats.totalFiles)++;
                quint64 size = static_cast<quint64>(entryInfo.size());
                (*stats.totalBytes) += size;
                (*stats.bytesOnDisk) += (size == 0) ? 0 : roundUpToCluster(size, clusterSize);
            } else if (entryInfo.isDir()) {
                (*stats.totalDirs)++;
#ifndef _WIN32
                (*stats.totalBytes) += static_cast<quint64>(entryInfo.size());
#endif
                (*stats.bytesOnDisk) += calculateDirOnDiskSize(entryPath, entryInfo, clusterSize);
            }
        }
    }
}

void calculateEntriesSize(const QString& basePath, const QStringList& names, CopyStats& stats, bool* cancelFlag) {
    quint64 clusterSize = getClusterSize(basePath);
    QDir dir(basePath);

    for (const QString& name : names) {
        if (cancelFlag && *cancelFlag) return;

        QString absPath = dir.absoluteFilePath(name);
        calculateEntrySize(absPath, stats, clusterSize, cancelFlag);
    }
}

void countCopyWork(const QString& basePath, const QStringList& names,
                   quint64& outFiles, quint64& outBytes,
                   FileOperationProgressDialog* progress) {
    QDir dir(basePath);
    int counter = 0;
    for (const QString& name : names) {
        if (progress && progress->wasCanceled())
            return;

        const QString absPath = dir.absoluteFilePath(name);
        QFileInfo fi(absPath);

        // Symlinks are copied as links / skipped cross-FS: negligible bytes.
        if (fi.isSymLink())
            continue;

        if (fi.isFile()) {
            outFiles++;
            outBytes += static_cast<quint64>(fi.size());
            continue;
        }

        if (fi.isDir()) {
            // Recurse without following symlinks (same iterator as size info).
            SortedDirIterator it(absPath,
                QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            while (it.hasNext()) {
                if (progress && progress->wasCanceled())
                    return;
                it.next();
                const QFileInfo entryInfo = it.fileInfo();
                if (entryInfo.isSymLink())
                    continue;
                if (entryInfo.isFile()) {
                    outFiles++;
                    outBytes += static_cast<quint64>(entryInfo.size());
                }
                if (progress && (++counter % 200 == 0))
                    progress->updateCounting(outFiles, outBytes);
            }
        }
    }
    if (progress)
        progress->updateCounting(outFiles, outBytes);
}

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

// Copy file in chunks with optional SHA-256 verification and sync per chunk
// Uses temp file + atomic rename for safety.
// Reports per-chunk progress to the dialog (top bar) when provided.
bool copyFileChunked(const QString& srcPath, const QString& dstPath, CopyMode mode,
                     FileOperationProgressDialog* progress) {
    QFile srcFile(srcPath);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to open source file:\n%1").arg(srcPath));
        return false;
    }

    // Create temp file path in destination directory
    QString tempPath = qMakeTempPartPath(dstPath, false);

    // Open destination file to reserve the name (prevents race conditions)
    QFile dstFile(dstPath);
    if (!dstFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to create destination file:\n%1").arg(dstPath));
        return false;
    }

    // Open temp file for actual writing
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to create temp file:\n%1").arg(tempPath));
        dstFile.close();
        dstFile.remove();
        return false;
    }

    const qint64 chunkSize = Config::instance().copyChunkSize();
    QByteArray buffer;
    QCryptographicHash srcHash(QCryptographicHash::Sha256);
    bool useSha = (mode == CopyMode::ChunkedSha);
    bool useSync = (mode == CopyMode::ChunkedSync);
    qint64 written = 0;

    while (!srcFile.atEnd()) {
        buffer = srcFile.read(chunkSize);
        if (buffer.isEmpty() && !srcFile.atEnd()) {
            QMessageBox::warning(nullptr, QObject::tr("Error"),
                                 QObject::tr("Failed to read from source file:\n%1").arg(srcPath));
            tempFile.close();
            tempFile.remove();
            dstFile.close();
            dstFile.remove();
            return false;
        }

        if (useSha)
            srcHash.addData(buffer);

        if (tempFile.write(buffer) != buffer.size()) {
            QMessageBox::warning(nullptr, QObject::tr("Error"),
                                 QObject::tr("Failed to write to temp file:\n%1").arg(tempPath));
            tempFile.close();
            tempFile.remove();
            dstFile.close();
            dstFile.remove();
            return false;
        }

        if (useSync) {
            tempFile.flush();
#ifdef _WIN32
            FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(tempFile.handle())));
#else
            fsync(tempFile.handle());
#endif
        }

        written += buffer.size();
        if (progress) {
            progress->addTransferred(buffer.size());
            progress->addFileBytes(written);
            if (progress->wasCanceled()) {
                tempFile.close();
                tempFile.remove();
                dstFile.close();
                dstFile.remove();
                return false;
            }
        } else {
            QCoreApplication::processEvents();
        }
    }

    srcFile.close();
    tempFile.close();

    // Atomic rename: close and remove empty dest, rename temp to dest
    dstFile.close();
    dstFile.remove();

    if (!QFile::rename(tempPath, dstPath)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to rename temp file to destination:\n%1\n->\n%2")
                             .arg(tempPath, dstPath));
        QFile::remove(tempPath);
        return false;
    }

    // Verify SHA-256 if enabled (verify final file after rename)
    if (useSha) {
        QFile dstCheck(dstPath);
        if (!dstCheck.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(nullptr, QObject::tr("Error"),
                                 QObject::tr("Failed to verify destination file:\n%1").arg(dstPath));
            QFile::remove(dstPath);
            return false;
        }

        QCryptographicHash dstHash(QCryptographicHash::Sha256);
        while (!dstCheck.atEnd()) {
            buffer = dstCheck.read(chunkSize);
            dstHash.addData(buffer);
            QCoreApplication::processEvents();
        }
        dstCheck.close();

        if (srcHash.result() != dstHash.result()) {
            QMessageBox::warning(nullptr, QObject::tr("Error"),
                                 QObject::tr("SHA-256 verification failed!\n"
                                             "Source and destination files do not match:\n%1").arg(dstPath));
            QFile::remove(dstPath);
            return false;
        }
    }

    finalizeCopiedFile(srcPath, dstPath);
    return true;
}

bool copyFile(const QString &srcPath, const QString &dstPath, FileOperationProgressDialog* progress,
              DestSync* sync) {
    const auto& cfg = Config::instance();
    QFileInfo srcInfo(srcPath);
    qint64 fileSize = srcInfo.size();
    bool largeFile = fileSize > cfg.largeFileThreshold();

    bool ok;
    // For files larger than threshold, use configured copy mode
    if (largeFile && cfg.copyMode() != CopyMode::System) {
        ok = copyFileChunked(srcPath, dstPath, cfg.copyMode(), progress);
    } else {
        // Default: use system copy
        ok = QFile::copy(srcPath, dstPath);
        if (ok) {
            finalizeCopiedFile(srcPath, dstPath);
            // System copy gives no chunk callbacks; count it as transferred now.
            if (progress)
                progress->addTransferred(fileSize);
        } else {
            QMessageBox::warning(nullptr, QObject::tr("Error"),
                                 QObject::tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, dstPath));
        }
    }

    if (!ok)
        return false;

    // Flush the written data to physical storage (batched for small files).
    if (sync)
        sync->afterFileCopied(dstPath, fileSize, largeFile);
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
                                         QMessageBox::Button askPolice, QWidget *parent,
                                         FileOperationProgressDialog* progress, DestSync* sync) {
    QFileInfo dstInfo(dstPath);
    QMessageBox::Button result = QMessageBox::Yes;
    if (dstInfo.exists()) {
        // Never offer to overwrite a directory with a same-named file:
        // wiping a whole target tree to drop a single file is too dangerous.
        if (dstInfo.isDir()) {
            if (progress) progress->pauseClock();
            QMessageBox::warning(parent, QObject::tr("Error"),
                QObject::tr("Cannot copy '%1': a directory with the same name "
                            "already exists at the destination.").arg(dstInfo.fileName()));
            if (progress) progress->resumeClock();
            return QMessageBox::No;
        }
        QMessageBox::Button reply;
        switch (askPolice) {
            case QMessageBox::Yes:
            case QMessageBox::No:
                // Exclude the time spent waiting on the prompt from speed/ETA.
                if (progress) progress->pauseClock();
                if (multi)
                    reply = askOverwriteMulti(parent, dstPath);
                else
                    reply = askOverwriteSingle(parent, dstPath);
                if (progress) progress->resumeClock();
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
    if (!copyFile(srcPath, dstPath, progress, sync))
        return QMessageBox::No;
    return result;
}

QMessageBox::Button copyOrMoveFileAskOverwrite(const QString &srcPath, const QString &dstPath, bool move, bool multi,
                                               QMessageBox::Button askPolice, QWidget *parent,
                                               FileOperationProgressDialog* progress, DestSync* sync) {
    auto reply = copyFileAskOverwrite(srcPath, dstPath, multi, askPolice, parent, progress, sync);
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
                                                 FileOperationProgressDialog &progress, DestSync &sync) {
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
        if (progress.wasCanceled())
            return QMessageBox::Abort;

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
                stats.symlinks++;
            }
            continue;
        }

        // Handle regular files
        if (fi.isFile()) {
            progress.beginFile(fi.fileName(), fi.size());
            askPolice = copyOrMoveFileAskOverwrite(srcPath, dstPath, move, true, askPolice, &progress, &progress, &sync);
            progress.endFile();

            if (askPolice == QMessageBox::Abort)
                return QMessageBox::Abort;
        }
        // Handle directories (recursive)
        else if (fi.isDir()) {
            askPolice = copyOrMoveDirectoryRecursive(srcPath, dstPath, move, sameFs, askPolice, stats, progress, sync);

            if (askPolice == QMessageBox::Abort)
                return QMessageBox::Abort;
            // Carry source directory metadata (times, permissions) to the copy.
            finalizeCopiedFile(srcPath, dstPath);
        }
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

    CopyStats stats;
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

    // The destination is a single path, and every source comes from one
    // directory, so source and destination share one filesystem for the whole
    // batch. A same-filesystem move is therefore a fast rename of every item;
    // anything else (copy, or a cross-filesystem move) transfers bytes.
    bool sameFs = areOnSameFilesystem(currentPath, dstPath);
    bool fastMove = move && sameFs;

    FileOperationProgressDialog progressDlg(move ? QObject::tr("Move") : QObject::tr("Copy"), parent);

    if (fastMove) {
        // Fast renames: nothing to weigh by bytes, drive progress by item count.
        progressDlg.setTotals(static_cast<quint64>(names.size()), 0);
    } else {
        // Count the real work (files + bytes) up front so progress is
        // byte-proportional. Uses SortedDirIterator (does not follow symlinks).
        quint64 totalFiles = 0;
        quint64 totalBytes = 0;
        countCopyWork(currentPath, names, totalFiles, totalBytes, &progressDlg);
        if (progressDlg.wasCanceled())
            return {};
        progressDlg.setTotals(totalFiles, totalBytes);
    }

    // Batches flushing copied data to the destination's filesystem (syncfs on
    // Linux). Its destructor performs the final flush even on early return.
    DestSync destSync(dstPath);

    QMessageBox::Button askPolice = QMessageBox::Yes;
    for (const QString &name: names) {
        QString srcPath = srcDir.absoluteFilePath(name);
        QString dstFilePath = destIsDir ? QDir(dstPath).filePath(name) : dstPath;
        QFileInfo srcInfo(srcPath);

        if (isInvalidCopyMoveTarget(srcPath, dstFilePath))
            continue;

        if (progressDlg.wasCanceled())
            break;

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
                stats.symlinks++;
            }
            continue;
        }

        if (fastMove) {
            // Same filesystem move - rename (fast). Advance progress per item.
            progressDlg.beginFile(name, 0);

            // QFile::rename never overwrites: if the target exists we must
            // ask (honoring Yes/No/YesToAll/NoToAll/Abort) and remove it first,
            // otherwise rename fails and the batch stalls on the first conflict.
            if (QFileInfo::exists(dstFilePath)) {
                // Never offer to overwrite a directory with a same-named file:
                // wiping a whole target tree to drop a single file is too dangerous.
                if (srcInfo.isFile() && QFileInfo(dstFilePath).isDir()) {
                    QMessageBox::warning(parent, QObject::tr("Error"),
                        QObject::tr("Cannot move '%1': a directory with the same name "
                                    "already exists at the destination.").arg(name));
                    progressDlg.endFile();
                    continue;
                }
                QMessageBox::Button reply;
                switch (askPolice) {
                    case QMessageBox::Yes:
                    case QMessageBox::No:
                        reply = names.size() > 1 ? askOverwriteMulti(&progressDlg, dstFilePath)
                                                 : askOverwriteSingle(&progressDlg, dstFilePath);
                        break;
                    default:
                        reply = askPolice;
                }
                if (reply == QMessageBox::Abort)
                    break;
                if (reply == QMessageBox::NoToAll) {
                    askPolice = QMessageBox::NoToAll;
                    progressDlg.endFile();
                    continue;
                }
                if (reply == QMessageBox::No) {
                    progressDlg.endFile();
                    continue;
                }
                if (reply == QMessageBox::YesToAll)
                    askPolice = QMessageBox::YesToAll;
                removeExisting(dstFilePath);
            }

            QFile file(srcPath);
            if (!file.rename(dstFilePath)) {
                QMessageBox::warning(parent, QObject::tr("Error"),
                    QObject::tr("Failed to move '%1'").arg(name));
            }
            progressDlg.endFile();
            continue;
        }

        if (srcInfo.isFile()) {
            progressDlg.beginFile(name, srcInfo.size());
            askPolice = copyOrMoveFileAskOverwrite(srcPath, dstFilePath,
                              move, names.size()>1, askPolice, &progressDlg, &progressDlg, &destSync);
            progressDlg.endFile();
        }
        else if (srcInfo.isDir()) {
            askPolice = copyOrMoveDirectoryRecursive(srcPath, dstFilePath, move,
                                             sameFs, askPolice, stats, progressDlg, destSync);
            if (askPolice != QMessageBox::Abort) {
                // Carry source directory metadata (times, permissions) to the copy
                // while the source still exists, then drop the source if moving.
                finalizeCopiedFile(srcPath, dstFilePath);
                if (move && (askPolice == QMessageBox::Yes || askPolice == QMessageBox::YesToAll))
                    QDir(srcPath).removeRecursively();
            }
        }
        if (progressDlg.wasCanceled())
            break;
    }

    // Show info message if symlinks were skipped
    if (stats.symlinks > 0) {
        QMessageBox::information(parent, QObject::tr("Symbolic Links Skipped"),
            QObject::tr("%1 symbolic link(s) were skipped.\n"
                        "Symbolic links cannot be copied/moved across different filesystems.")
            .arg(stats.symlinks));
    }

    if (names.size() == 1)
        return dstPath;
    else
        return {};
}

} // namespace FileOperations
