#ifndef ARCHIVEOPERATIONS_H
#define ARCHIVEOPERATIONS_H

#include <QHash>
#include <QString>
#include <QStringList>

#include <memory>

class QWidget;
struct ArchiveContents;

// Archive operations through the archive module (spp-archive from
// siplasplas-plugins), with the progress dialog of copying. The module
// itself goes over to lsar, 7z and unar for what libarchive cannot read;
// without the module archives are not opened.
namespace ArchiveOperations {

enum class Result { Done, Cancelled, Failed };

// Lists the archive (Enter, Alt+F9) with a progress dialog that can cancel
// it: listing tar.gz and the like decompresses all of it. When only part of
// it could be read, Done with that part and a warning in `error`.
Result list(QWidget* parent, const QString& archivePath, ArchiveContents& contents, QString& error);

// Extracts the archive listed in `contents` into `destDir`, existing files
// settled first with one question as copying does, passwords asked when
// needed. `error` is set when it failed.
Result extract(QWidget* parent, const ArchiveContents& contents, const QString& destDir, QString& error);

// F5 inside an archive: copies `entries` (paths inside the archive, below
// its directory `archiveDir`, "" for the top) out into `destDir`, with their
// paths relative to `archiveDir`, as copying does: existing files are
// settled first with one question (from the listing, before extracting).
// The entries are extracted with a progress dialog into a hidden temporary
// directory inside `destDir` (the same filesystem) and moved from there.
Result copyOut(QWidget* parent, const ArchiveContents& contents, const QString& archiveDir,
               const QStringList& entries, const QString& destDir, QString& error);

struct PackRequest {
    QString format;          // "zip", "7z", "tar", "tar.gz", "tar.xz", "tar.zst", "tar.bz2"
    QString volumeSize;      // empty, or e.g. "100m" (k, m, g: KiB, MiB, GiB)
    QString solidBlockSize;  // 7z: empty (one block), or e.g. "1g"
    int level = -1;          // compression level, -1 the format's default
    QString password;        // zip (AES-256) or 7z (names encrypted too); empty: none
    bool moveFiles = false;  // remove the files once packed
};

// Packs `files` (relative to `baseDir`) into `archivePath` through the
// module, which writes a 7z with a password or a solid block size by the 7z
// program. An existing archive is replaced after asking.
Result pack(QWidget* parent, const QString& baseDir, const QStringList& files, const QString& archivePath,
            const PackRequest& request, QString& error);

// Previews (F3) of the files of one archive: the first kBytes of each, in a
// temporary directory removed with the cache. Only that much of a file is
// decompressed, also of a huge entry. In an archive compressed as one
// stream (tar.gz and the like, solid 7z) everything before a file is
// decompressed to reach it, so the files passed on the way are kept too
// (up to kLimit together), and reading goes on past the file as far again
// as it went to reach it (at least kReadAhead): a file near the start costs
// little, one in the middle reads the whole archive, and the files around
// it open at once later.
class PreviewCache {
public:
    static constexpr qint64 kBytes = 1024 * 1024;
    static constexpr qint64 kLimit = 512 * 1024 * 1024;
    static constexpr qint64 kReadAhead = 16 * 1024 * 1024;

    // `contents`: the archive's entries in its order, with their sizes.
    explicit PreviewCache(const ArchiveContents& contents);
    ~PreviewCache();

    // The preview of the file `entry` (of `entrySize` bytes), extracted
    // with a progress dialog unless kept already. A larger file's name says
    // so before its extension (kept for syntax colouring), e.g.
    // "app (first 1.00 MiB).log". Empty with `error` set when it failed
    // (empty `error` when cancelled).
    QString file(QWidget* parent, const QString& entry, qint64 entrySize, QString& error);

private:
    QString targetPath(const QString& entry, qint64 entrySize) const;
    // In a stream archive: where to stop reading for `entry` (see above).
    QString readUntil(const QString& entry) const;

    QString m_archivePath;
    QStringList m_order;               // files in the archive's order
    QList<qint64> m_ends;              // bytes decompressed up to the end of each
    bool m_stream;                     // decompressed as one stream: keep the files on the way
    struct Dir;
    std::unique_ptr<Dir> m_dir;        // the temporary directory
    QHash<QString, QString> m_files;   // entry path -> preview file
    qint64 m_bytes = 0;
};

} // namespace ArchiveOperations

#endif // ARCHIVEOPERATIONS_H
