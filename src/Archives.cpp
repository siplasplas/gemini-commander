#include "Archives.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QStandardPaths>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <archive.h>
#include <archive_entry.h>

namespace {

// Check if string consists only of digits
bool isAllDigits(const QString& str) {
    if (str.isEmpty())
        return false;
    for (const QChar& ch : str) {
        if (!ch.isDigit())
            return false;
    }
    return true;
}

// Find first suffix with double extension (e.g. "tar.gz")
// Ignore as double if one segment is all digits (e.g. "7z.001")
// Returns segments split by dot, or empty list if not found
QVarLengthArray<QString, 2> findDoubleSuffix(const QMimeType& mt) {
    QStringList suffixes = mt.suffixes();

    for (const QString& suffix : suffixes) {
        QStringList segments = suffix.split('.');

        if (segments.size() < 2)
            continue;

        // Check if any segment is all digits
        bool hasDigitSegment = false;
        for (const QString& seg : segments) {
            if (isAllDigits(seg)) {
                hasDigitSegment = true;
                break;
            }
        }

        // If has digit segment, don't count as double suffix
        if (hasDigitSegment)
            continue;

        // This is a real double suffix
        QVarLengthArray<QString, 2> result;
        for (const QString& seg : segments)
            result.append(seg);
        return result;
    }

    return {};
}

// Get extension from path, handling digit segments
// For .7z.001 returns "7z", for .tar.gz returns "gz"
QString getExtensionIgnoringDigits(const QString& path) {
    QFileInfo info(path);
    QString completeSuffix = info.completeSuffix();

    if (completeSuffix.isEmpty())
        return QString();

    QStringList segments = completeSuffix.split('.');

    // Find first non-digit segment
    for (const QString& seg : segments) {
        if (!isAllDigits(seg))
            return seg;
    }

    // All segments are digits? Return last one
    return segments.isEmpty() ? QString() : segments.last();
}

} // anonymous namespace

int isArchiveFormat(const QString& archiveType) {
    if (archiveType == "tar" || archiveType == "cpio" || archiveType == "bcpio")
        return 1;
    if (archiveType == "iso" || archiveType == "archive")
        return 2;
    return 0;
}

QPair<QVarLengthArray<QString, 2>, ArchiveType> analyzeArchive(
    const QMimeType& mt,
    const QString& path)
{
    QVarLengthArray<QString, 2> result;

    // Get MIME name
    QString mimeName = mt.name();

    // Split by slash
    QStringList parts = mimeName.split('/');
    if (parts.size() != 2)
        return {result, ArchiveType::Empty};

    // Check first part
    QString category = parts[0];
    if (category != "application" && category != "image")
        return {result, ArchiveType::Empty};

    // Get name part
    QString name = parts[1];

    // Handle vnd. or x-vnd. prefix
    if (name.contains('.')) {
        if (name.startsWith("vnd.")) {
            name = name.mid(4); // Skip "vnd."
        } else if (name.startsWith("x-vnd.")) {
            name = name.mid(6); // Skip "x-vnd."
        }

        // If still contains dot after removing vnd prefix
        if (name.contains('.')) {
            result.append(name);
            return {result, ArchiveType::Compressed};
        }
    }

    // Split by hyphens
    QStringList nameParts = name.split('-');

    // Remove leading "x"
    if (!nameParts.isEmpty() && nameParts.first() == "x") {
        nameParts.removeFirst();
        if (nameParts.isEmpty())
            return {result, ArchiveType::Empty};
    }

    // Check if any part is "compressed"
    int compressedIndex = nameParts.indexOf("compressed");

    if (compressedIndex != -1) {
        // "compressed" found

        if (nameParts.size() == 1) {
            // Only "compressed"
            result.append("compressed");
            return {result, ArchiveType::Compressed};
        }

        if (compressedIndex == nameParts.size() - 1) {
            // "compressed" at the end

            // Check for double suffix
            auto doubleSuffix = findDoubleSuffix(mt);
            if (!doubleSuffix.isEmpty()) {
                // Has double suffix (e.g. tar.gz)
                return {doubleSuffix, ArchiveType::CompressedArchive};
            } else {
                // No double suffix - join parts before "compressed"
                QStringList beforeCompressed = nameParts.mid(0, compressedIndex);
                QString joined = beforeCompressed.join('-');
                result.append(joined);
                return {result, ArchiveType::Compressed};
            }
        } else {
            // "compressed" not at the end

            // Join parts after "compressed"
            QStringList afterCompressed = nameParts.mid(compressedIndex + 1);
            QString afterJoined = afterCompressed.join('-');

            // Join parts before "compressed"
            QStringList beforeCompressed = nameParts.mid(0, compressedIndex);
            QString beforeJoined = beforeCompressed.join('-');

            result.append(afterJoined);
            result.append(beforeJoined);
            return {result, ArchiveType::CompressedArchive};
        }
    } else {
        // No "compressed" found

        // Check for double suffix
        auto doubleSuffix = findDoubleSuffix(mt);
        if (!doubleSuffix.isEmpty()) {
            // Has double suffix (e.g. tar.Z)
            // Return first and second segments
            return {doubleSuffix, ArchiveType::CompressedArchive};
        } else {
            // No double suffix

            if (nameParts.size() == 1 && isArchiveFormat(nameParts[0])>0) {
                // Single part that is a pure archive format
                result.append(nameParts[0]);
                return {result, ArchiveType::Archive};
            } else {
                // Join all parts
                QString joined = nameParts.join('-');
                result.append(joined);
                return {result, ArchiveType::Compressed};
            }
        }
    }
}

QPair<QVarLengthArray<QString, 2>, DetailedArchiveType> classifyArchive(
    const QMimeType& mt,
    const QString& path)
{
    // Standard compression formats
    static const QStringList standardCompressed = {
        "gz", "gzip", "bz2", "bzip2", "bz3", "bzip3", "lrzip","arc",
        "lz","lzip","zlib","zstd","lz4","lzma","lha","lhz",
        "zip", "arj", "rar", "xz", "xzpdf", "7z", "ms-cab"
    };
    // Call analyzeArchive first
    auto [components, basicType] = analyzeArchive(mt, path);

    QVarLengthArray<QString, 2> result = components;

    // Map based on basic type and component values
    switch (basicType) {
        case ArchiveType::Empty:
            return {result, DetailedArchiveType::NotArchive};

        case ArchiveType::CompressedArchive:
            if (isArchiveFormat(result[0])>0)
                return {result, DetailedArchiveType::CompressedArchive};
            else if (result.size() > 1 && standardCompressed.contains(result[1]))
                return {result, DetailedArchiveType::Compressed};
            else
                return {result, DetailedArchiveType::NotArchive};

        case ArchiveType::Archive: {
            // Check if it's a standard archive format
            if (!components.isEmpty()) {
                QString archiveType = components[0];
                if (isArchiveFormat(archiveType) == 1)
                    return {result, DetailedArchiveType::Archive};
            }
            // Other archive types (like iso)
            return {result, DetailedArchiveType::ArchiveOther};
        }

        case ArchiveType::Compressed: {
            // Check component value to classify
            if (!components.isEmpty()) {
                QString compType = components[0];

                // CompressedOther: jar, LibreOffice documents
                if (compType == "java-archive" ||
                    compType == "epub+zip" ||
                    compType.contains("oasis.opendocument")) {
                    return {result, DetailedArchiveType::CompressedOther};
                }
                if (compType == "efi.iso") {
                    return {result, DetailedArchiveType::ArchiveOther};
                }
                if (standardCompressed.contains(compType)) {
                    return {result, DetailedArchiveType::Compressed};
                }
                if (components.size() > 1) {
                    QString compType = components.back();
                    if (standardCompressed.contains(compType))
                        return {result, DetailedArchiveType::Compressed};
                }
            }

            // Unknown compressed format
            return {result, DetailedArchiveType::NotArchive};
        }
    }

    // Fallback (should not reach here)
    return {result, DetailedArchiveType::NotArchive};
}

QString archiveTypeToString(DetailedArchiveType type)
{
    switch (type) {
        case DetailedArchiveType::NotArchive:
            return "Not Archive";
        case DetailedArchiveType::Compressed:
            return "Compressed";
        case DetailedArchiveType::Archive:
            return "Archive";
        case DetailedArchiveType::CompressedArchive:
            return "Compressed Archive";
        case DetailedArchiveType::CompressedOther:
            return "Compressed Other";
        case DetailedArchiveType::ArchiveOther:
            return "Archive Other";
    }
    return "Unknown";
}

QString pack7z(const QString& archivePath, const QStringList& files,
               bool moveFiles, const QString& volumeSize,
               const QString& solidBlockSize)
{
    // Write file list to temp file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString listFile = QDir(tempDir).absoluteFilePath("gemini_pack_list.txt");

    QFile f(listFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QObject::tr("Failed to create temporary file list.");
    }
    QTextStream out(&f);
    for (const QString& path : files) {
        out << path << "\n";
    }
    f.close();

    // Build command
    QStringList args;
    args << "a";  // add to archive

    if (moveFiles)
        args << "-sdel";  // delete files after adding

    if (!volumeSize.isEmpty())
        args << QString("-v%1").arg(volumeSize);

    if (!solidBlockSize.isEmpty()) {
        if (solidBlockSize == "on") {
            args << "-ms=on";
        } else {
            args << QString("-ms=%1").arg(solidBlockSize);
        }
        args << "-mqs=on";  // sort files by extension for better compression
    }

    args << archivePath;
    args << QString("@%1").arg(listFile);

    QProcess proc;
    proc.start("7z", args);
    proc.waitForFinished(-1);

    QString error;
    if (proc.exitCode() != 0) {
        QString standardError = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        error = QObject::tr("7z failed with exit code %1:\n%2")
                    .arg(proc.exitCode()).arg(standardError);
    }

    QFile::remove(listFile);
    return error;
}

QString packZip(const QString& archivePath, const QStringList& files,
                bool moveFiles)
{
    QStringList args;

    if (moveFiles)
        args << "-m";  // move mode

    args << "-r"; // recurse into directories

    args << archivePath;
    args << "-@";  // read names from stdin

    QProcess proc;
    proc.start("zip", args);
    if (!proc.waitForStarted()) {
        return QObject::tr("Failed to start zip process.");
    }

    // Write file paths to stdin
    for (const QString& path : files) {
        proc.write(path.toUtf8());
        proc.write("\n");
    }
    proc.closeWriteChannel();
    proc.waitForFinished(-1);

    if (proc.exitCode() != 0) {
        QString standardError = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return QObject::tr("zip failed with exit code %1:\n%2")
                   .arg(proc.exitCode()).arg(standardError);
    }

    return {};
}

// ============================================================================
// Archive Reading
// ============================================================================

void ArchiveContents::clear()
{
    archivePath.clear();
    allEntries.clear();
}

QList<ArchiveEntry> ArchiveContents::entriesAt(const QString& dirPath) const
{
    QList<ArchiveEntry> result;
    QSet<QString> addedPaths;  // Track paths we've already added

    QString prefix = dirPath.isEmpty() ? QString() : dirPath + "/";

    for (const ArchiveEntry& entry : allEntries) {
        // Skip entries not under this directory
        if (!prefix.isEmpty() && !entry.path.startsWith(prefix))
            continue;

        // Get the relative path from dirPath
        QString relativePath = prefix.isEmpty() ? entry.path : entry.path.mid(prefix.length());

        // Skip empty paths (the directory itself)
        if (relativePath.isEmpty())
            continue;

        // Check if this is a direct child or deeper
        int slashPos = relativePath.indexOf('/');

        if (slashPos == -1) {
            // Direct child - add it if not already added
            if (!addedPaths.contains(entry.path)) {
                addedPaths.insert(entry.path);
                result.append(entry);
            }
        } else {
            // Deeper entry - add the intermediate directory if not already added
            QString dirName = relativePath.left(slashPos);
            QString fullDirPath = prefix.isEmpty() ? dirName : prefix + dirName;

            if (!addedPaths.contains(fullDirPath)) {
                addedPaths.insert(fullDirPath);

                // Create a synthetic directory entry
                ArchiveEntry dirEntry;
                dirEntry.path = fullDirPath;
                dirEntry.name = dirName;
                dirEntry.isDirectory = true;
                dirEntry.size = 0;
                result.append(dirEntry);
            }
        }
    }

    return result;
}

bool ArchiveContents::isDirectory(const QString& path) const
{
    if (path.isEmpty())
        return true;  // Root is always a directory

    // Check if any entry has this as a directory or has children under it
    QString prefix = path + "/";

    for (const ArchiveEntry& entry : allEntries) {
        if (entry.path == path && entry.isDirectory)
            return true;
        if (entry.path.startsWith(prefix))
            return true;  // Has children, so it's a directory
    }

    return false;
}

namespace {

ArchiveContents readArchiveWithLibarchive(const QString& archivePath)
{
    ArchiveContents result;
    result.archivePath = archivePath;

    struct archive* a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    QByteArray pathBytes = archivePath.toUtf8();
    if (archive_read_open_filename(a, pathBytes.constData(), 10240) == ARCHIVE_OK) {
        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            ArchiveEntry e;
            e.path = QString::fromUtf8(archive_entry_pathname(entry));

            // Remove trailing slash from directories
            while (e.path.endsWith('/'))
                e.path.chop(1);

            e.name = QFileInfo(e.path).fileName();
            e.isDirectory = (archive_entry_filetype(entry) == AE_IFDIR);
            e.size = archive_entry_size(entry);
            e.modTime = QDateTime::fromSecsSinceEpoch(archive_entry_mtime(entry));

            // Skip empty paths
            if (!e.path.isEmpty() && !e.name.isEmpty()) {
                result.allEntries.append(e);
            }

            archive_read_data_skip(a);
        }
    }

    archive_read_free(a);
    return result;
}

ArchiveContents readArchiveWithUnar(const QString& archivePath)
{
    ArchiveContents result;
    result.archivePath = archivePath;

    // Use lsar (list archive) with JSON output
    QProcess proc;
    proc.start("lsar", {"-j", archivePath});
    if (!proc.waitForFinished(30000))
        return result;

    if (proc.exitCode() != 0)
        return result;

    QByteArray output = proc.readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(output);
    if (!doc.isObject())
        return result;

    QJsonObject root = doc.object();
    QJsonArray contents = root["lsarContents"].toArray();

    for (const QJsonValue& val : contents) {
        QJsonObject obj = val.toObject();

        ArchiveEntry e;
        e.path = obj["XADFileName"].toString();

        // Remove trailing slash from directories
        while (e.path.endsWith('/'))
            e.path.chop(1);

        e.name = QFileInfo(e.path).fileName();
        e.isDirectory = obj["XADIsDirectory"].toBool();
        e.size = obj["XADFileSize"].toVariant().toLongLong();

        // Parse date if available
        QString dateStr = obj["XADLastModificationDate"].toString();
        if (!dateStr.isEmpty()) {
            e.modTime = QDateTime::fromString(dateStr, Qt::ISODate);
        }

        if (!e.path.isEmpty() && !e.name.isEmpty()) {
            result.allEntries.append(e);
        }
    }

    return result;
}

} // anonymous namespace

ArchiveContents readArchive(const QString& archivePath)
{
    // Try libarchive first
    ArchiveContents result = readArchiveWithLibarchive(archivePath);

    // If libarchive failed or returned empty, try unar
    if (result.allEntries.isEmpty()) {
        result = readArchiveWithUnar(archivePath);
    }

    return result;
}

bool archiveHasSingleRoot(const ArchiveContents& contents)
{
    if (contents.allEntries.isEmpty())
        return true;

    QSet<QString> rootEntries;
    for (const ArchiveEntry& entry : contents.allEntries) {
        // Get the root component of the path
        int slashPos = entry.path.indexOf('/');
        QString root = (slashPos == -1) ? entry.path : entry.path.left(slashPos);
        if (!root.isEmpty()) {
            rootEntries.insert(root);
        }
    }

    return rootEntries.size() <= 1;
}

QString extractArchive(const QString& archivePath, const QString& destDir)
{
    // Ensure destination directory exists
    QDir().mkpath(destDir);

    // Try 7z first (supports most formats)
    QString exec7z = QStandardPaths::findExecutable("7z");
    if (!exec7z.isEmpty()) {
        QProcess proc;
        QStringList args;
        args << "x" << "-y" << QString("-o%1").arg(destDir) << archivePath;
        proc.start(exec7z, args);
        proc.waitForFinished(-1);

        if (proc.exitCode() == 0) {
            return {};  // Success
        }
        // If 7z failed, try unar
    }

    // Try unar as fallback
    QString execUnar = QStandardPaths::findExecutable("unar");
    if (!execUnar.isEmpty()) {
        QProcess proc;
        QStringList args;
        args << "-f" << "-o" << destDir << archivePath;
        proc.start(execUnar, args);
        proc.waitForFinished(-1);

        if (proc.exitCode() == 0) {
            return {};  // Success
        }
        QString standardError = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return QObject::tr("unar failed with exit code %1:\n%2")
                   .arg(proc.exitCode()).arg(standardError);
    }

    return QObject::tr("No extraction tool found. Install 7z or unar.");
}
