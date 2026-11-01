#include "ArchiveOperations.h"

#include "Archives.h"
#include "CopyConflicts.h"
#include "FileOperationProgressDialog.h"
#include "FileOperations.h"
#include "Plugins.h"
#include "keys/KeyRouter.h"
#include "siplasplas/plugin/ArchivePlugin.h"

#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>

namespace ArchiveOperations {

namespace {

// Asked by the module for the password of an encrypted archive or entry.
spp::ArchivePassword passwordAsker(QWidget* parent)
{
    return [parent](const QString& archivePath) {
        bool ok = false;
        const QString password = QInputDialog::getText(
            parent, QObject::tr("Password"),
            QObject::tr("Password for '%1':").arg(QFileInfo(archivePath).fileName()),
            QLineEdit::Password, QString(), &ok);
        return ok ? password : QString();
    };
}

// The size of the archive's files (volumes .001, .002, ... together).
qint64 archiveBytes(const QString& archivePath)
{
    if (!archivePath.endsWith(QLatin1String(".001")))
        return QFileInfo(archivePath).size();
    const QString base = archivePath.chopped(4);
    qint64 size = 0;
    for (int i = 1;; ++i) {
        const QFileInfo volume(QStringLiteral("%1.%2").arg(base).arg(i, 3, 10, QLatin1Char('0')));
        if (!volume.exists())
            return size;
        size += volume.size();
    }
}

} // namespace

Result extract(QWidget* parent, const ArchiveContents& contents, const QString& destDir, QString& error)
{
    spp::ArchivePlugin* plugin = Plugins::instance().archive();
    if (!plugin) {
        error = QObject::tr("The archive module is not loaded.");
        return Result::Failed;
    }

    // Existing files, settled before extracting (as copying does).
    using namespace FileOperations;
    ConflictScan scan;
    for (const ArchiveEntry& entry : contents.allEntries) {
        if (entry.isDirectory)
            continue;
        const QString target = QDir(destDir).filePath(entry.path);
        const QFileInfo targetInfo(target);
        if (!targetExists(targetInfo) || isRealDir(targetInfo))
            continue;
        ++scan.conflicts;
        if (entry.modTime.isValid()
            && entry.modTime.toMSecsSinceEpoch() - targetInfo.lastModified().toMSecsSinceEpoch() >= kSameTimeMs)
            ++scan.newer;
        if (scan.examples.size() < 5)
            scan.examples << target;
    }
    ConflictPolicy policy = ConflictPolicy::Overwrite;
    if (scan.conflicts > 0) {
        const auto chosen = askConflictPolicy(parent, scan, false);
        if (!chosen)
            return Result::Cancelled;
        policy = *chosen;
    }
    const spp::ExtractConflictHandler conflict = [policy](const spp::ArchiveItem& item, const QString& target) {
        const QFileInfo targetInfo(target);
        switch (policy) {
            case ConflictPolicy::Overwrite:
                return spp::ExtractConflict::Overwrite;
            case ConflictPolicy::Skip:
                return spp::ExtractConflict::Skip;
            case ConflictPolicy::OverwriteOlder:
                return item.modTime.isValid()
                               && item.modTime.toMSecsSinceEpoch() - targetInfo.lastModified().toMSecsSinceEpoch()
                                      >= kSameTimeMs
                           ? spp::ExtractConflict::Overwrite
                           : spp::ExtractConflict::Skip;
        }
        return spp::ExtractConflict::Skip;
    };

    // Progress by the bytes of the archive read.
    const QString archivePath = contents.archivePath;
    FileOperationProgressDialog progress(QObject::tr("Extract"), parent);
    const qint64 total = archiveBytes(archivePath);
    progress.setTotals(1, quint64(total));
    progress.beginFile(QFileInfo(archivePath).fileName(), total);
    KeyRouter::instance().setOperationInProgress(true);
    qint64 reported = 0;
    const bool ok = plugin->extract(
        archivePath, destDir, {}, error,
        [&progress, &reported](qint64 done, qint64, const QString&) {
            progress.addTransferred(done - reported);  // for the speed
            reported = done;
            progress.addFileBytes(done);
            return !progress.wasCanceled();
        },
        passwordAsker(&progress), conflict);
    KeyRouter::instance().setOperationInProgress(false);
    progress.endFile();
    if (ok)
        return Result::Done;
    return progress.wasCanceled() ? Result::Cancelled : Result::Failed;
}

} // namespace ArchiveOperations
