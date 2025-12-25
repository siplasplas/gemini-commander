#include "Archives.h"
#include <QFileInfo>

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
