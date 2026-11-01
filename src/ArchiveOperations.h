#ifndef ARCHIVEOPERATIONS_H
#define ARCHIVEOPERATIONS_H

#include <QString>

class QWidget;
struct ArchiveContents;

// Archive operations through the archive module (siplasplas-plugins), with
// the progress dialog of copying.
namespace ArchiveOperations {

enum class Result { Done, Cancelled, Failed };

// Extracts the archive listed in `contents` (by the module, see
// ArchiveContents::readByPlugin) into `destDir`. Existing files are settled
// first with one question, as copying does; passwords are asked when
// needed. `error` is set when it failed.
Result extract(QWidget* parent, const ArchiveContents& contents, const QString& destDir, QString& error);

} // namespace ArchiveOperations

#endif // ARCHIVEOPERATIONS_H
