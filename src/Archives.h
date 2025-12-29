#ifndef ARCHIVES_H
#define ARCHIVES_H

#include <QString>
#include <QVarLengthArray>
#include <QMimeType>
#include <QPair>

enum class ArchiveType {
    Empty,              // Not an archive/compression
    Compressed,         // Compression only (gz, xz, bz2)
    Archive,            // Archive without compression (tar, cpio)
    CompressedArchive   // Archive with compression (tar.gz, zip, rar)
};

enum class DetailedArchiveType {
    NotArchive,         // Not recognized as archive/compression
    Compressed,         // Standard compression (gzip, bzip2, xz, zip, rar, 7z)
    Archive,            // Pure archive (tar, cpio, bcpio)
    CompressedArchive,  // Archive with compression (tar.gz, zip, rar)
    CompressedOther,    // Compressed but not entered (jar, LibreOffice)
    ArchiveOther        // Disk images and other archives (iso)
};

// Analyze archive/compression type from MIME type and file path
// Returns: pair of (components, type)
// - components: 1 or 2 strings depending on type
// - type: classification of archive/compression
QPair<QVarLengthArray<QString, 2>, ArchiveType> analyzeArchive(
    const QMimeType& mt,
    const QString& path
);

// Detailed classification of archive/compression types
// Uses analyzeArchive internally and provides more specific categorization
// Returns: pair of (components, detailed type)
QPair<QVarLengthArray<QString, 2>, DetailedArchiveType> classifyArchive(
    const QMimeType& mt,
    const QString& path
);

// Convert DetailedArchiveType to human-readable string
QString archiveTypeToString(DetailedArchiveType type);

// Pack files into 7z archive
// Returns empty string on success, error message on failure
QString pack7z(const QString& archivePath, const QStringList& files,
               bool moveFiles, const QString& volumeSize,
               const QString& solidBlockSize);

// Pack files into zip archive
// Returns empty string on success, error message on failure
QString packZip(const QString& archivePath, const QStringList& files,
                bool moveFiles);

#endif // ARCHIVES_H
