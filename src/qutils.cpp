#include "quitls.h"
#include "utils.h"
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QApplication>
#include <QPalette>

#include "SizeFormat.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

QString qFormatSize(std::size_t value, bool binaryPrefix) {
    return QString::fromStdString(SizeFormat::formatSize(value, binaryPrefix));
}

bool isDarkTheme() {
    return qApp->palette().color(QPalette::Window).lightness() < 128;
}

QString qEscapePathForShell(const QString& path) {
    return QString::fromStdString(utils::escapePathForShell(path.toStdString()));
}

QPair<QString, QString> splitFileName(const QFileInfo& info)
{
    if (info.isDir()) {
        return {info.fileName(), QString()};
    }

    QString fileName = info.fileName();

    // If name ends with a dot, there's no real extension
    // e.g., "..." should be basename "...", not ".." with empty extension
    if (fileName.endsWith('.')) {
        return {fileName, QString()};
    }

    QString baseName = info.completeBaseName();
    QString ext = info.suffix();

    // Handle hidden files like ".gitignore" where completeBaseName() is empty
    // Qt treats the whole name as extension in this case
    if (baseName.isEmpty() && !ext.isEmpty()) {
        baseName = "." + ext;
        ext.clear();
    }

    return {baseName, ext};
}

bool isTextFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Check first 512 bytes for non-text characters
    QByteArray sample = file.read(512);
    file.close();

    if (sample.isEmpty()) {
        return false;
    }

    // Simple heuristic: if most bytes are printable, it's text
    int printable = 0;
    int total = sample.size();

    for (char c : sample) {
        if ((c >= 0x20 && c <= 0x7E) || c == '\n' || c == '\r' || c == '\t') {
            printable++;
        }
    }

    return (printable * 100 / total) > 85;  // >85% printable = text
}


ExecutableType getExecutableType(const QString& filePath) {
    QFileInfo fileInfo(filePath);

    if (!fileInfo.isExecutable()) {
        return ExecutableType::Unknown;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return ExecutableType::Unknown;
    }

    QByteArray header = file.read(4);
    file.close();

    if (header.size() < 2) {
        return ExecutableType::Unknown;
    }

    // Check for ELF magic bytes
    if (header.size() >= 4 &&
        header[0] == 0x7f &&
        header[1] == 'E' &&
        header[2] == 'L' &&
        header[3] == 'F') {
        return ExecutableType::ELFBinary;
        }

    // Check for shebang
    if (header[0] == '#' && header[1] == '!') {
        return ExecutableType::ScriptWithShebang;
    }

    // If it's text but no shebang, it's probably a shell script
    if (isTextFile(filePath)) {
        return ExecutableType::TextExecutable;
    }

    return ExecutableType::Unknown;
}

void finalizeCopiedFile(const QString& srcPath, const QString& dstPath)
{
#ifdef _WIN32
    // Windows implementation
    HANDLE hSrc = CreateFileW(
        reinterpret_cast<LPCWSTR>(srcPath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hSrc != INVALID_HANDLE_VALUE) {
        FILETIME ftCreate, ftAccess, ftWrite;
        if (GetFileTime(hSrc, &ftCreate, &ftAccess, &ftWrite)) {
            HANDLE hDst = CreateFileW(
                reinterpret_cast<LPCWSTR>(dstPath.utf16()),
                FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );

            if (hDst != INVALID_HANDLE_VALUE) {
                SetFileTime(hDst, &ftCreate, &ftAccess, &ftWrite);
                CloseHandle(hDst);
            }
        }
        CloseHandle(hSrc);
    }

    // Sync file to disk (important for USB drives to prevent data loss)
    HANDLE hFlush = CreateFileW(
        reinterpret_cast<LPCWSTR>(dstPath.utf16()),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFlush != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hFlush);
        CloseHandle(hFlush);
    }
#else
    // Linux/Unix implementation
    struct stat srcStat;
    if (stat(srcPath.toLocal8Bit().constData(), &srcStat) == 0) {
        struct timespec times[2];
        times[0] = srcStat.st_atim;  // access time
        times[1] = srcStat.st_mtim;  // modification time
        utimensat(AT_FDCWD, dstPath.toLocal8Bit().constData(), times, 0);
    }

    // Sync file to disk (important for USB drives to prevent data loss)
    int fd = open(dstPath.toLocal8Bit().constData(), O_RDONLY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
#endif
}