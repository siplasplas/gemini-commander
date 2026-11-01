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
#include "SearchIgnore.h"
#include "SortedDirIterator.h"
#include "quitls.h"

#include <qce/spell/SpellChecker.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif
#ifdef Q_OS_LINUX
#include <sys/sendfile.h>
#endif

#include <limits>
#include <optional>

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
                   const std::function<bool(quint64, quint64)>& keepGoing, SearchIgnore* ignore) {
    QDir dir(basePath);
    int counter = 0;
    // Reports every 200 entries; false stops counting.
    const auto step = [&] { return ++counter % 200 != 0 || keepGoing(outFiles, outBytes); };
    for (const QString& name : names) {
        if (!step())
            return;

        const QString absPath = dir.absoluteFilePath(name);
        QFileInfo fi(absPath);

        // Symlinks are copied as links / skipped cross-FS: negligible bytes.
        if (fi.isSymLink() || (ignore && ignore->isIgnored(fi)))
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
            if (ignore)
                it.setEntryFilter([ignore](const QFileInfo& entry) { return !ignore->isIgnored(entry); });
            while (it.hasNext()) {
                it.next();
                const QFileInfo entryInfo = it.fileInfo();
                if (!entryInfo.isSymLink() && entryInfo.isFile()) {
                    outFiles++;
                    outBytes += static_cast<quint64>(entryInfo.size());
                }
                if (!step())
                    return;
            }
        }
    }
    keepGoing(outFiles, outBytes);
}

// A path taken at the destination, also by a broken symbolic link.
static bool targetExists(const QFileInfo &info) {
    return info.exists() || info.isSymLink();
}

// A real directory, not a symbolic link to one.
static bool isRealDir(const QFileInfo &info) {
    return info.isDir() && !info.isSymLink();
}

// Modification times closer than this count as the same: FAT keeps them to
// 2 seconds, so a file copied there and back is not "newer".
constexpr qint64 kSameTimeMs = 2000;

// 1 when the source is newer than the target, -1 when older, 0 the same.
static int compareTimes(const QFileInfo &src, const QFileInfo &dst) {
    const qint64 diff = src.lastModified().toMSecsSinceEpoch() - dst.lastModified().toMSecsSinceEpoch();
    if (diff >= kSameTimeMs)
        return 1;
    if (diff <= -kSameTimeMs)
        return -1;
    return 0;
}

static bool overwriteAllowed(ConflictPolicy policy, const QFileInfo &src, const QFileInfo &dst) {
    switch (policy) {
        case ConflictPolicy::Overwrite:
            return true;
        case ConflictPolicy::Skip:
            return false;
        case ConflictPolicy::OverwriteOlder:
            return compareTimes(src, dst) > 0;
    }
    return false;
}

// The entries a copy or move goes through in a directory (as
// copyOrMoveDirectoryRecursive does).
static QFileInfoList dirEntries(const QString &path) {
    return QDir(path).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System, QDir::Name);
}

namespace {

// Files (and symbolic links) whose target exists already, found before
// copying or moving.
struct ConflictScan {
    quint64 entries = 0;    // checked, for the progress
    quint64 conflicts = 0;
    quint64 newer = 0;      // of them, the source newer than the target
    QStringList examples;   // the first few target paths
};

} // namespace

// Goes through `src` against its target `dst`; only directories that exist
// at the destination are entered, the rest cannot conflict. Mismatches (a
// file over a directory, a directory over a file) are reported when copying
// and are not conflicts. False when keepGoing() stops it.
static bool scanConflicts(const QFileInfo &src, const QString &dst, bool sameFs, SearchIgnore *ignore,
                          ConflictScan &scan, const std::function<bool(const ConflictScan&)> &keepGoing) {
    if (ignore && ignore->isIgnored(src))
        return true;
    if (++scan.entries % 200 == 0 && !keepGoing(scan))
        return false;
    const QFileInfo dstInfo(dst);
    if (!targetExists(dstInfo))
        return true;
    if (src.isSymLink() && !sameFs)
        return true;  // not copied across filesystems
    if (isRealDir(src)) {
        if (!isRealDir(dstInfo))
            return true;
        for (const QFileInfo &child : dirEntries(src.absoluteFilePath())) {
            if (!scanConflicts(child, QDir(dst).filePath(child.fileName()), sameFs, ignore, scan, keepGoing))
                return false;
        }
        return true;
    }
    if (isRealDir(dstInfo))
        return true;
    scan.conflicts++;
    if (compareTimes(src, dstInfo) > 0)
        scan.newer++;
    if (scan.examples.size() < 5)
        scan.examples << dst;
    return true;
}

static QString describeFile(const QFileInfo &info) {
    return QObject::tr("%1, %2")
        .arg(QLocale().formattedDataSize(info.size()),
             info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

// One file whose target exists: Overwrite or Cancel, saying whether the file
// copied over it is newer, older or of the same time.
static bool askOverwriteOneFile(QWidget *parent, const QFileInfo &src, const QFileInfo &dst) {
    const int cmp = compareTimes(src, dst);
    const QString question = cmp > 0 ? QObject::tr("Overwrite with a newer file?")
                           : cmp < 0 ? QObject::tr("Overwrite with an older file?")
                                     : QObject::tr("Overwrite with a file of the same time?");
    QMessageBox box(QMessageBox::Question, QObject::tr("Overwrite"),
                    QObject::tr("'%1' already exists.\n%2").arg(dst.absoluteFilePath(), question),
                    QMessageBox::NoButton, parent);
    box.setInformativeText(QObject::tr("Source: %1\nTarget: %2").arg(describeFile(src), describeFile(dst)));
    QPushButton *overwrite = box.addButton(QObject::tr("&Overwrite"), QMessageBox::AcceptRole);
    QPushButton *cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(cmp > 0 ? overwrite : cancel);
    box.setEscapeButton(cancel);
    box.exec();
    return box.clickedButton() == overwrite;
}

// Several conflicts: one choice for all of them; nullopt cancels the command.
static std::optional<ConflictPolicy> askConflictPolicy(QWidget *parent, const ConflictScan &scan, bool move) {
    QString text = QObject::tr("%n conflict(s): already at the destination", nullptr, int(scan.conflicts));
    text += QLatin1Char('\n');
    for (const QString &path : scan.examples)
        text += QLatin1Char('\n') + path;
    if (scan.conflicts > quint64(scan.examples.size()))
        text += QStringLiteral("\n…");
    QMessageBox box(QMessageBox::Question, QObject::tr("Conflicts"), text, QMessageBox::NoButton, parent);
    QString info = QObject::tr("%n of them newer than the target.", nullptr, int(scan.newer));
    if (move)
        info += QLatin1Char('\n') + QObject::tr("Files not overwritten stay in the source.");
    box.setInformativeText(info);
    QPushButton *skip = box.addButton(QObject::tr("&Skip"), QMessageBox::AcceptRole);
    QPushButton *all = box.addButton(QObject::tr("Overwrite &All"), QMessageBox::AcceptRole);
    QPushButton *newer = box.addButton(QObject::tr("Overwrite with &Newer"), QMessageBox::AcceptRole);
    QPushButton *cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(skip);
    box.setEscapeButton(cancel);
    box.exec();
    if (box.clickedButton() == skip)
        return ConflictPolicy::Skip;
    if (box.clickedButton() == all)
        return ConflictPolicy::Overwrite;
    if (box.clickedButton() == newer)
        return ConflictPolicy::OverwriteOlder;
    return std::nullopt;
}

#ifdef Q_OS_LINUX
// Copies up to `len` bytes from the current offset of `in` to the current offset
// of `out`, advancing both. The data stays in the kernel: copy_file_range() (which
// may also share blocks on btrfs/XFS), or sendfile() when that is not possible -
// copy_file_range() refuses to copy between different filesystems (EXDEV), which
// is the usual case when copying to a USB drive. Returns the number of bytes
// copied, 0 at the end of the source, or -1 on error.
static qint64 copyInKernel(int in, int out, qint64 len, bool& tryCopyFileRange) {
    if (tryCopyFileRange) {
        ssize_t n = ::copy_file_range(in, nullptr, out, nullptr, static_cast<size_t>(len), 0);
        if (n >= 0)
            return n;
        if (errno != EXDEV && errno != EINVAL && errno != ENOSYS && errno != EOPNOTSUPP)
            return -1;
        tryCopyFileRange = false;  // not supported here: sendfile() from now on
    }
    return ::sendfile(out, in, nullptr, static_cast<size_t>(len));
}
#endif

// Copies the next chunk (at most `len` bytes) of an open source file to the temp
// file. Returns the number of bytes copied, 0 at the end of the source, or -1.
static qint64 copyChunk(QFile& src, QFile& dst, qint64 len, [[maybe_unused]] bool& tryCopyFileRange) {
#ifdef Q_OS_LINUX
    return copyInKernel(src.handle(), dst.handle(), len, tryCopyFileRange);
#else
    QByteArray buffer = src.read(len);
    if (buffer.isEmpty())
        return src.atEnd() ? 0 : -1;
    return dst.write(buffer) == buffer.size() ? buffer.size() : -1;
#endif
}

// SHA-256 of a whole file, or an empty result when it cannot be read.
static QByteArray fileSha256(const QString& path, qint64 chunkSize) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(chunkSize));
        QCoreApplication::processEvents();
    }
    return hash.result();
}

// Copy file in chunks with optional SHA-256 verification and sync per chunk
// Uses temp file + atomic rename for safety.
// Reports per-chunk progress to the dialog (top bar) when provided.
bool copyFileChunked(const QString& srcPath, const QString& dstPath, CopyMode mode,
                     FileOperationProgressDialog* progress) {
    // Unbuffered: on Linux the data is copied through the descriptors, not QFile.
    QFile srcFile(srcPath);
    if (!srcFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
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
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to create temp file:\n%1").arg(tempPath));
        dstFile.close();
        dstFile.remove();
        return false;
    }

    const qint64 chunkSize = Config::instance().copyChunkSize();
    bool useSha = (mode == CopyMode::ChunkedSha);
    bool useSync = (mode == CopyMode::ChunkedSync);
    bool tryCopyFileRange = true;
    qint64 written = 0;

    for (;;) {
        const qint64 copied = copyChunk(srcFile, tempFile, chunkSize, tryCopyFileRange);
        if (copied == 0)
            break;
        if (copied < 0) {
            QMessageBox::warning(nullptr, QObject::tr("Error"),
                                 QObject::tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, tempPath));
            tempFile.close();
            tempFile.remove();
            dstFile.close();
            dstFile.remove();
            return false;
        }

        if (useSync) {
#ifdef _WIN32
            FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(tempFile.handle())));
#else
            fsync(tempFile.handle());
#endif
        }

        written += copied;
        if (progress) {
            progress->addTransferred(copied);
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

    // Verify SHA-256 if enabled: the copy never passes through this process, so
    // both the source and the final file (after rename) are read again.
    if (useSha) {
        const QByteArray dstHash = fileSha256(dstPath, chunkSize);
        if (dstHash.isEmpty()) {
            QMessageBox::warning(nullptr, QObject::tr("Error"),
                                 QObject::tr("Failed to verify destination file:\n%1").arg(dstPath));
            QFile::remove(dstPath);
            return false;
        }

        if (fileSha256(srcPath, chunkSize) != dstHash) {
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

namespace {
enum class FileResult { Done, Skipped, Failed };
}

// Copies (or moves, by copying and removing the source) one file; an existing
// target is overwritten as `policy` says.
static FileResult copyOrMoveFile(const QString &srcPath, const QString &dstPath, bool move,
                                 ConflictPolicy policy, QWidget *parent,
                                 FileOperationProgressDialog* progress, DestSync* sync) {
    const QFileInfo dstInfo(dstPath);
    if (targetExists(dstInfo)) {
        // Never overwrite a directory with a same-named file: wiping a whole
        // target tree to drop a single file is too dangerous.
        if (isRealDir(dstInfo)) {
            if (progress) progress->pauseClock();
            QMessageBox::warning(parent, QObject::tr("Error"),
                QObject::tr("Cannot copy '%1': a directory with the same name "
                            "already exists at the destination.").arg(dstInfo.fileName()));
            if (progress) progress->resumeClock();
            return FileResult::Failed;
        }
        if (!overwriteAllowed(policy, QFileInfo(srcPath), dstInfo))
            return FileResult::Skipped;
        QFile::remove(dstPath);
    }
    if (!copyFile(srcPath, dstPath, progress, sync))
        return FileResult::Failed;
    if (move)
        QFile::remove(srcPath);
    return FileResult::Done;
}

// Copies a symbolic link itself (same filesystem); an existing target is
// replaced as `policy` says. True when copied.
static bool copySymlinkWithPolicy(const QFileInfo &src, const QString &dstPath, ConflictPolicy policy) {
    const QFileInfo dstInfo(dstPath);
    if (targetExists(dstInfo) && (isRealDir(dstInfo) || !overwriteAllowed(policy, src, dstInfo)))
        return false;
    return copySymlink(src.absoluteFilePath(), dstPath);
}

void collectCopyStats(const QString &srcPath, CopyStats &stats, bool &ok, bool *cancelFlag,
                      const std::function<bool(const QFileInfo&)> &keep) {
    ok = true;

    QFileInfo rootInfo(srcPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        ok = false;
        return;
    }

    // Count root directory too
    stats.totalDirs += 1;

    SortedDirIterator it(srcPath, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    if (keep)
        it.setEntryFilter(keep);

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

bool copyOrMoveDirectoryRecursive(const QString &srcRoot, const QString &dstRoot, bool move,
                                  bool sameFs, ConflictPolicy policy, CopyStats &stats,
                                  FileOperationProgressDialog &progress, DestSync &sync,
                                  SearchIgnore *ignore) {
    QFileInfo srcInfo(srcRoot);
    if (!srcInfo.exists() || !srcInfo.isDir())
        return true;

    QDir dstDir;
    if (!dstDir.mkpath(dstRoot)) {
        QMessageBox::warning(nullptr, QObject::tr("Error"),
                             QObject::tr("Failed to create directory:\n%1").arg(dstRoot));
        return true;
    }

    // All entries including symlinks (System flag includes symlinks on Unix)
    for (const QFileInfo &fi: dirEntries(srcRoot)) {
        if (progress.wasCanceled())
            return false;
        if (ignore && ignore->isIgnored(fi))
            continue;

        const QString srcPath = fi.absoluteFilePath();
        const QString dstPath = QDir(dstRoot).filePath(fi.fileName());

        // Handle symbolic links
        if (fi.isSymLink()) {
            if (sameFs) {
                // Same FS: copy the symlink itself (don't follow)
                if (copySymlinkWithPolicy(fi, dstPath, policy) && move)
                    QFile::remove(srcPath);
            } else {
                // Cross-FS: skip symlinks
                stats.symlinks++;
            }
            continue;
        }

        // Handle regular files
        if (fi.isFile()) {
            progress.beginFile(fi.fileName(), fi.size());
            copyOrMoveFile(srcPath, dstPath, move, policy, &progress, &progress, &sync);
            progress.endFile();
        }
        // Handle directories (recursive)
        else if (fi.isDir()) {
            if (!copyOrMoveDirectoryRecursive(srcPath, dstPath, move, sameFs, policy, stats, progress, sync,
                                              ignore))
                return false;
            // Carry source directory metadata (times, permissions) to the copy.
            finalizeCopiedFile(srcPath, dstPath);
            // Moved: gone unless something stayed (skipped, failed).
            if (move)
                QDir().rmdir(srcPath);
        }
    }
    return true;
}

// Same-filesystem move of `src` into the existing directory `dst`: entries
// missing there are renamed into it, directories on both sides are merged
// the same way, and files on both sides follow `policy`. The source
// directories left empty are removed.
static void moveMergeByRename(const QString &src, const QString &dst, ConflictPolicy policy, QWidget *parent) {
    for (const QFileInfo &fi : dirEntries(src)) {
        const QString srcPath = fi.absoluteFilePath();
        const QString dstPath = QDir(dst).filePath(fi.fileName());
        const QFileInfo dstInfo(dstPath);
        if (targetExists(dstInfo)) {
            if (isRealDir(fi) && isRealDir(dstInfo)) {
                moveMergeByRename(srcPath, dstPath, policy, parent);
                continue;
            }
            if (isRealDir(fi) || isRealDir(dstInfo)) {
                QMessageBox::warning(parent, QObject::tr("Error"),
                    QObject::tr("Cannot move '%1': a file and a directory have the same name.")
                        .arg(srcPath));
                continue;
            }
            if (!overwriteAllowed(policy, fi, dstInfo))
                continue;
            QFile::remove(dstPath);
            qce::spell::removeSpellingData(dstPath);
        }
        if (QFile::rename(srcPath, dstPath))
            qce::spell::moveSpellingData(srcPath, dstPath);
        else
            QMessageBox::warning(parent, QObject::tr("Error"),
                                 QObject::tr("Failed to move '%1'").arg(srcPath));
    }
    QDir().rmdir(src);
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

// The words ignored by the editor's spell check in the file, or in the files
// under the directory, follow it to its copy or new place. An overwritten
// file's own words are dropped first; a directory copied into an existing one
// keeps the words of the files already there.
static void carrySpellingData(const QString &srcPath, const QString &dstPath, bool move, bool isFile) {
    if (isFile)
        qce::spell::removeSpellingData(dstPath);
    if (move)
        qce::spell::moveSpellingData(srcPath, dstPath);
    else
        qce::spell::copySpellingData(srcPath, dstPath);
}

QString executeCopyOrMove(const QString &currentPath, const QStringList &names, const QString &destInput,
                            bool move, QWidget *parent, bool useGitignore,
                            std::optional<CopyTotals> totals) {
    if (names.isEmpty())
        return {};

    // Skipping is for copies only: a move would leave the skipped entries
    // behind and then remove the source directory together with them.
    std::optional<SearchIgnore> ignoreStorage;
    if (useGitignore && !move)
        ignoreStorage.emplace(currentPath, QStringList(), QStringList(), true,
                              Config::instance().gitignoreOptions());
    SearchIgnore* ignore = ignoreStorage ? &*ignoreStorage : nullptr;

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

    // Existing targets are settled before anything is copied, with one
    // question: for a single file whether to overwrite it, otherwise one
    // choice for all conflicts found by a quick scan.
    ConflictPolicy policy = ConflictPolicy::Overwrite;
    const QFileInfo firstInfo(srcDir.absoluteFilePath(names.first()));
    if (names.size() == 1 && !isRealDir(firstInfo)) {
        const QString dstFilePath = destIsDir ? QDir(dstPath).filePath(names.first()) : dstPath;
        const QFileInfo dstInfo(dstFilePath);
        if (targetExists(dstInfo) && !isRealDir(dstInfo)
            && !isInvalidCopyMoveTarget(firstInfo.absoluteFilePath(), dstFilePath)
            && (!firstInfo.isSymLink() || sameFs)
            && !askOverwriteOneFile(parent, firstInfo, dstInfo))
            return {};
    } else {
        ConflictScan scan;
        const auto keepGoing = [&progressDlg](const ConflictScan& s) {
            progressDlg.updateScanning(s.entries, s.conflicts);
            return !progressDlg.wasCanceled();
        };
        for (const QString &name : names) {
            const QString srcPath = srcDir.absoluteFilePath(name);
            const QString dstFilePath = destIsDir ? QDir(dstPath).filePath(name) : dstPath;
            if (isInvalidCopyMoveTarget(srcPath, dstFilePath))
                continue;
            if (!scanConflicts(QFileInfo(srcPath), dstFilePath, sameFs, ignore, scan, keepGoing))
                return {};
        }
        if (scan.conflicts > 0) {
            const auto chosen = askConflictPolicy(parent, scan, move);
            if (!chosen)
                return {};
            policy = *chosen;
        }
    }

    if (fastMove) {
        // Fast renames: nothing to weigh by bytes, drive progress by item count.
        progressDlg.setTotals(static_cast<quint64>(names.size()), 0);
    } else if (totals) {
        // Counted already (the copy dialog's "Count").
        progressDlg.setTotals(totals->files, totals->bytes);
    } else {
        // Count the real work (files + bytes) up front so progress is
        // byte-proportional. Uses SortedDirIterator (does not follow symlinks).
        quint64 totalFiles = 0;
        quint64 totalBytes = 0;
        countCopyWork(currentPath, names, totalFiles, totalBytes,
                      [&progressDlg](quint64 files, quint64 bytes) {
                          progressDlg.updateCounting(files, bytes);
                          return !progressDlg.wasCanceled();
                      }, ignore);
        if (progressDlg.wasCanceled())
            return {};
        progressDlg.setTotals(totalFiles, totalBytes);
    }

    // Batches flushing copied data to the destination's filesystem (syncfs on
    // Linux). Its destructor performs the final flush even on early return.
    DestSync destSync(dstPath);

    for (const QString &name: names) {
        QString srcPath = srcDir.absoluteFilePath(name);
        QString dstFilePath = destIsDir ? QDir(dstPath).filePath(name) : dstPath;
        QFileInfo srcInfo(srcPath);

        if (isInvalidCopyMoveTarget(srcPath, dstFilePath))
            continue;
        if (ignore && ignore->isIgnored(srcInfo))
            continue;

        if (progressDlg.wasCanceled())
            break;

        // Handle symbolic links
        if (srcInfo.isSymLink()) {
            if (sameFs) {
                const QFileInfo dstInfo(dstFilePath);
                if (move) {
                    // Same FS move: rename works for symlinks
                    if (targetExists(dstInfo)) {
                        if (isRealDir(dstInfo) || !overwriteAllowed(policy, srcInfo, dstInfo))
                            continue;
                        QFile::remove(dstFilePath);
                    }
                    QFile file(srcPath);
                    if (!file.rename(dstFilePath)) {
                        QMessageBox::warning(parent, QObject::tr("Error"),
                            QObject::tr("Failed to move symlink '%1'").arg(name));
                    }
                } else {
                    // Same FS copy: copy the symlink itself
                    copySymlinkWithPolicy(srcInfo, dstFilePath, policy);
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

            // QFile::rename never overwrites: an existing target is removed
            // first when the policy says so. A directory is never removed: a
            // directory moved onto one is merged into it, a file is not put
            // in place of a directory, nor a directory in place of a file.
            const QFileInfo dstInfo(dstFilePath);
            if (targetExists(dstInfo)) {
                if (isRealDir(srcInfo) && isRealDir(dstInfo)) {
                    moveMergeByRename(srcPath, dstFilePath, policy, parent);
                    progressDlg.endFile();
                    continue;
                }
                if (isRealDir(srcInfo) || isRealDir(dstInfo)) {
                    QMessageBox::warning(parent, QObject::tr("Error"),
                        QObject::tr("Cannot move '%1': a file and a directory have the same name.")
                            .arg(name));
                    progressDlg.endFile();
                    continue;
                }
                if (!overwriteAllowed(policy, srcInfo, dstInfo)) {
                    progressDlg.endFile();
                    continue;
                }
                QFile::remove(dstFilePath);
                qce::spell::removeSpellingData(dstFilePath);
            }

            QFile file(srcPath);
            if (file.rename(dstFilePath)) {
                // One update for a whole directory: the paths under it move too.
                qce::spell::moveSpellingData(srcPath, dstFilePath);
            } else {
                QMessageBox::warning(parent, QObject::tr("Error"),
                    QObject::tr("Failed to move '%1'").arg(name));
            }
            progressDlg.endFile();
            continue;
        }

        if (srcInfo.isFile()) {
            progressDlg.beginFile(name, srcInfo.size());
            const FileResult result = copyOrMoveFile(srcPath, dstFilePath, move, policy,
                                                     &progressDlg, &progressDlg, &destSync);
            progressDlg.endFile();
            if (result == FileResult::Done)
                carrySpellingData(srcPath, dstFilePath, move, true);
        }
        else if (srcInfo.isDir()) {
            if (copyOrMoveDirectoryRecursive(srcPath, dstFilePath, move,
                                             sameFs, policy, stats, progressDlg, destSync, ignore)) {
                // Carry source directory metadata (times, permissions) to the copy
                // while the source still exists, then drop the source if moving:
                // only what was moved, the files not overwritten stay.
                finalizeCopiedFile(srcPath, dstFilePath);
                if (move)
                    QDir().rmdir(srcPath);
            }
            // Files left behind (skipped or cancelled) keep their words, the
            // copies get them too.
            carrySpellingData(srcPath, dstFilePath, move && !QFileInfo::exists(srcPath), false);
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

    // The path the single entry was copied/moved to (inside dstPath when that
    // is a directory), so that the panels can select it.
    if (names.size() == 1)
        return destIsDir ? QDir(dstPath).filePath(names.first()) : dstPath;
    else
        return {};
}

} // namespace FileOperations
