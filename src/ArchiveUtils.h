#ifndef ARCHIVEUTILS_H
#define ARCHIVEUTILS_H

#include <QList>
#include <QString>
#include <QStringList>

// The rules of archive operations that need no archive and no window
// (ArchiveOperations uses them; tests check them).
namespace ArchiveUtils {

// "100m" -> bytes (k, m, g: KiB, MiB, GiB, as 7z reads them); 0 when empty.
qint64 sizeInBytes(const QString& size);

// Compressed as one stream (or solid): reaching a file decompresses
// everything before it. Zip, RAR, ISO and the like are read file by file.
bool isStreamArchive(const QString& archivePath);

// Where to stop reading for a preview of `entry`: `order` are the files in
// the archive's order and `ends` the bytes decompressed up to the end of
// each. In a stream archive reading goes on past the file as far again as
// reaching it took, at least `readAhead` more; else (or for a file not in
// `order`) it stops at the file.
QString readUntil(const QStringList& order, const QList<qint64>& ends, const QString& entry, bool stream,
                  qint64 readAhead);

// The preview file of `entry` below `dir`: the entry's path, with a note
// before the extension when only `maxBytes` of its `entrySize` are kept,
// e.g. "app (first 1.00 MiB).log". Empty when the path leaves `dir` ("..").
QString previewPath(const QString& dir, const QString& entry, qint64 entrySize, qint64 maxBytes);

} // namespace ArchiveUtils

#endif // ARCHIVEUTILS_H
