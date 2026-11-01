#ifndef ARCHIVEOPERATIONS_H
#define ARCHIVEOPERATIONS_H

#include <QString>
#include <QStringList>

class QWidget;
struct ArchiveContents;

// Archive operations with the progress dialog of copying: through the
// archive module (siplasplas-plugins), and by external programs (7z, unar,
// zip) for what the module cannot do, also with a progress window.
namespace ArchiveOperations {

enum class Result { Done, Cancelled, Failed };

// Extracts the archive listed in `contents` into `destDir`: through the
// module when it listed it (ArchiveContents::readByPlugin), existing files
// settled first with one question as copying does, passwords asked when
// needed; else by 7z or unar. `error` is set when it failed.
Result extract(QWidget* parent, const ArchiveContents& contents, const QString& destDir, QString& error);

struct PackRequest {
    QString format;          // "zip" or "7z"
    QString volumeSize;      // empty, or e.g. "100m" (k, m, g: KiB, MiB, GiB)
    QString solidBlockSize;  // 7z: empty, or e.g. "1g"; given, 7z packs (libarchive cannot)
    bool moveFiles = false;  // remove the files once packed
};

// Packs `files` (relative to `baseDir`) into `archivePath`: through the
// module, or by 7z for a solid block size and without the module (zip then).
// An existing archive is replaced after asking.
Result pack(QWidget* parent, const QString& baseDir, const QStringList& files, const QString& archivePath,
            const PackRequest& request, QString& error);

} // namespace ArchiveOperations

#endif // ARCHIVEOPERATIONS_H
