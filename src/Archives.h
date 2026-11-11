#ifndef ARCHIVES_H
#define ARCHIVES_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>
#include <QVarLengthArray>
#include <QMimeType>
#include <QPair>

namespace spp {
struct ArchiveItem;
}

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

// Entry in an archive (file or directory)
struct ArchiveEntry {
    QString path;           // Full path inside archive (e.g., "dir1/dir2/file.txt")
    QString name;           // Just the filename
    bool isDirectory = false;
    qint64 size = 0;
    QDateTime modTime;
};

// Contents of an archive
struct ArchiveContents {
    QString archivePath;                    // Path to archive file
    QList<ArchiveEntry> allEntries;         // Flat list of all entries

    // Get entries for a specific directory path inside archive
    // Returns only direct children (not recursive)
    QList<ArchiveEntry> entriesAt(const QString& dirPath) const;

    // Check if path is a directory (has children or marked as dir)
    bool isDirectory(const QString& path) const;

    // Clear all data
    void clear();
};

// The entries the archive module listed, added to `contents`. Archives are
// read by ArchiveOperations::list (through the module, with progress).
void appendArchiveItems(ArchiveContents& contents, const QList<spp::ArchiveItem>& items);

// Check if archive has single root entry (file or directory)
// Returns true if single root, false if multiple roots
bool archiveHasSingleRoot(const ArchiveContents& contents);

// Extracting and packing: ArchiveOperations.

#endif // ARCHIVES_H
