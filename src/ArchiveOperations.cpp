#include "ArchiveOperations.h"

#include "ArchiveUtils.h"
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
#include <QMessageBox>
#include <QTemporaryDir>

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

// The archive module, or null with `error` set.
spp::ArchivePlugin* module(QString& error)
{
    spp::ArchivePlugin* plugin = Plugins::instance().archive();
    if (!plugin)
        error = QObject::tr("Archives need the archive module (spp-archive), which is not loaded.");
    return plugin;
}

// The module's progress (bytes of the archive, or the programs' coarser
// steps) on the progress dialog of copying.
spp::ArchiveProgress progressOn(FileOperationProgressDialog& progress, const QString& archivePath,
                                bool countSpeed = false)
{
    auto started = std::make_shared<bool>(false);
    auto reported = std::make_shared<qint64>(0);
    return [&progress, archivePath, countSpeed, started, reported](qint64 done, qint64 total, const QString&) {
        if (!*started) {
            progress.setTotals(1, quint64(qMax<qint64>(total, 0)));
            progress.beginFile(QFileInfo(archivePath).fileName(), total);
            *started = true;
        }
        if (countSpeed && done > *reported) {
            progress.addTransferred(done - *reported);  // for the speed
            *reported = done;
        }
        progress.addFileBytes(done);
        return !progress.wasCanceled();
    };
}

// The volume files name.001, name.002, ... that exist.
QStringList volumeFiles(const QString& path)
{
    QStringList result;
    for (int i = 1;; ++i) {
        const QString volume = QStringLiteral("%1.%2").arg(path).arg(i, 3, 10, QLatin1Char('0'));
        if (!QFileInfo::exists(volume))
            return result;
        result << volume;
    }
}

// The files of the archive whose targets (`targetOf`; empty: not copied)
// exist: one question for all, as copying asks. Overwrite when there are
// none; nullopt when cancelled.
std::optional<FileOperations::ConflictPolicy> settleConflicts(
    QWidget* parent, const ArchiveContents& contents, const std::function<QString(const ArchiveEntry&)>& targetOf)
{
    using namespace FileOperations;
    ConflictScan scan;
    for (const ArchiveEntry& entry : contents.allEntries) {
        if (entry.isDirectory)
            continue;
        const QString target = targetOf(entry);
        if (target.isEmpty())
            continue;
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
    if (scan.conflicts == 0)
        return ConflictPolicy::Overwrite;
    return askConflictPolicy(parent, scan, false);
}

// Extracts `entries` (all when empty) through the module, with the progress
// dialog and passwords.
Result extractWithModule(QWidget* parent, spp::ArchivePlugin* plugin, const QString& archivePath,
                         const QString& destDir, const QStringList& entries,
                         const spp::ExtractConflictHandler& conflict, QString& error)
{
    FileOperationProgressDialog progress(QObject::tr("Extract"), parent);
    KeyRouter::instance().setOperationInProgress(true);
    const bool ok = plugin->extract(archivePath, destDir, entries, error, progressOn(progress, archivePath, true),
                                    passwordAsker(&progress), conflict);
    KeyRouter::instance().setOperationInProgress(false);
    progress.endFile();
    if (ok)
        return Result::Done;
    return progress.wasCanceled() ? Result::Cancelled : Result::Failed;
}

// Removes the packed files (Pack with "move"); a link, not what it points to.
void removeSources(const QString& baseDir, const QStringList& files)
{
    for (const QString& file : files) {
        const QString path = QDir(baseDir).absoluteFilePath(file);
        const QFileInfo info(path);
        if (info.isDir() && !info.isSymLink())
            QDir(path).removeRecursively();
        else
            QFile::remove(path);
    }
}

} // namespace

Result list(QWidget* parent, const QString& archivePath, ArchiveContents& contents, QString& error)
{
    contents.clear();
    contents.archivePath = archivePath;
    spp::ArchivePlugin* plugin = module(error);
    if (!plugin)
        return Result::Failed;
    FileOperationProgressDialog progress(QObject::tr("Open Archive"), parent);
    QList<spp::ArchiveItem> items;
    KeyRouter::instance().setOperationInProgress(true);
    const bool ok = plugin->list(archivePath, items, error, progressOn(progress, archivePath),
                                 passwordAsker(&progress));
    KeyRouter::instance().setOperationInProgress(false);
    if (progress.wasCanceled())
        return Result::Cancelled;
    appendArchiveItems(contents, items);
    if (ok && !contents.allEntries.isEmpty()) {
        error.clear();
        return Result::Done;
    }
    if (!contents.allEntries.isEmpty()) {
        // Better part of it than nothing; Done with a warning in `error`.
        error = QObject::tr("Only part of '%1' could be read:\n%2").arg(QFileInfo(archivePath).fileName(), error);
        return Result::Done;
    }
    const QString reason = error;
    error = QObject::tr("'%1' cannot be read as an archive.").arg(QFileInfo(archivePath).fileName());
    if (!reason.isEmpty())
        error += QLatin1Char('\n') + reason;
    return Result::Failed;
}

Result extract(QWidget* parent, const ArchiveContents& contents, const QString& destDir, QString& error)
{
    spp::ArchivePlugin* plugin = module(error);
    if (!plugin)
        return Result::Failed;

    // Existing files, settled before extracting (as copying does).
    using namespace FileOperations;
    const auto settled = settleConflicts(parent, contents, [&destDir](const ArchiveEntry& entry) {
        return QDir(destDir).filePath(entry.path);
    });
    if (!settled)
        return Result::Cancelled;
    const ConflictPolicy policy = *settled;
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
    QDir().mkpath(destDir);
    return extractWithModule(parent, plugin, contents.archivePath, destDir, {}, conflict, error);
}

Result copyOut(QWidget* parent, const ArchiveContents& contents, const QString& archiveDir,
               const QStringList& entries, const QString& destDir, QString& error)
{
    spp::ArchivePlugin* plugin = module(error);
    if (!plugin)
        return Result::Failed;
    if (entries.isEmpty())
        return Result::Done;
    using namespace FileOperations;
    const EnsureDirResult dest = ensureDestDirExists(destDir, parent);
    if (dest != EnsureDirResult::Exists && dest != EnsureDirResult::Created)
        return Result::Cancelled;

    // Paths below the archive's directory shown in the panel.
    const QString prefix = archiveDir.isEmpty() ? QString() : archiveDir + QLatin1Char('/');
    const auto selected = [&entries](const QString& path) {
        for (const QString& entry : entries) {
            if (path == entry || path.startsWith(entry + QLatin1Char('/')))
                return true;
        }
        return false;
    };
    const auto settled = settleConflicts(parent, contents, [&](const ArchiveEntry& entry) {
        return selected(entry.path) ? QDir(destDir).filePath(entry.path.mid(prefix.size())) : QString();
    });
    if (!settled)
        return Result::Cancelled;

    // Extracted next to the target, then moved in by renaming (the same
    // filesystem), the conflicts as settled above.
    QTemporaryDir staging(QDir(destDir).filePath(QStringLiteral(".gemini-extract-XXXXXX")));
    if (!staging.isValid()) {
        error = QObject::tr("Cannot write to '%1': %2").arg(destDir, staging.errorString());
        return Result::Failed;
    }
    const Result result = extractWithModule(parent, plugin, contents.archivePath, staging.path(), entries, {}, error);
    if (result != Result::Done)
        return result;
    QStringList names;
    for (const QString& entry : entries)
        names << entry.mid(prefix.size());
    executeCopyOrMove(QDir(staging.path()).filePath(archiveDir), names, destDir + QLatin1Char('/'), true, parent,
                      false, std::nullopt, *settled);
    return Result::Done;
}

Result pack(QWidget* parent, const QString& baseDir, const QStringList& files, const QString& archivePath,
            const PackRequest& request, QString& error)
{
    spp::ArchivePlugin* plugin = module(error);
    if (!plugin)
        return Result::Failed;
    const qint64 volumeBytes = ArchiveUtils::sizeInBytes(request.volumeSize);
    // An existing archive (or its volumes) is replaced, not added to.
    QStringList existing = volumeBytes > 0 ? volumeFiles(archivePath) : QStringList();
    if (QFileInfo::exists(archivePath))
        existing.prepend(archivePath);
    if (!existing.isEmpty()) {
        const auto reply = QMessageBox::question(
            parent, QObject::tr("Pack"),
            QObject::tr("'%1' already exists.\nReplace it?").arg(existing.first()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (reply != QMessageBox::Yes)
            return Result::Cancelled;
        for (const QString& file : existing)
            QFile::remove(file);
    }

    // A 7z with a password or a solid block size the module writes by the
    // 7z program; a cancelled or failed packing leaves no files behind.
    spp::PackOptions options;
    options.format = request.format;
    options.volumeSize = volumeBytes;
    options.level = request.level;
    options.password = request.password;
    options.solidBlockSize = ArchiveUtils::sizeInBytes(request.solidBlockSize);
    FileOperationProgressDialog progress(QObject::tr("Pack"), parent);
    KeyRouter::instance().setOperationInProgress(true);
    const bool ok = plugin->pack(archivePath, baseDir, files, options, error, progressOn(progress, archivePath, true));
    KeyRouter::instance().setOperationInProgress(false);
    if (!ok)
        return progress.wasCanceled() ? Result::Cancelled : Result::Failed;
    if (request.moveFiles)
        removeSources(baseDir, files);
    return Result::Done;
}

struct PreviewCache::Dir {
    QTemporaryDir dir{QDir::tempPath() + QStringLiteral("/gemini-commander-preview-XXXXXX")};
};

PreviewCache::PreviewCache(const ArchiveContents& contents)
    : m_archivePath(contents.archivePath)
    , m_stream(ArchiveUtils::isStreamArchive(contents.archivePath))
    , m_dir(std::make_unique<Dir>())
{
    qint64 end = 0;
    for (const ArchiveEntry& entry : contents.allEntries) {
        if (entry.isDirectory)
            continue;
        end += entry.size;
        m_order << entry.path;
        m_ends << end;
    }
}

QString PreviewCache::readUntil(const QString& entry) const
{
    return ArchiveUtils::readUntil(m_order, m_ends, entry, m_stream, kReadAhead);
}

PreviewCache::~PreviewCache() = default;

QString PreviewCache::targetPath(const QString& entry, qint64 entrySize) const
{
    return ArchiveUtils::previewPath(m_dir->dir.path(), entry, entrySize, kBytes);
}

QString PreviewCache::file(QWidget* parent, const QString& entry, qint64 entrySize, QString& error)
{
    if (const auto kept = m_files.constFind(entry); kept != m_files.constEnd())
        return *kept;
    if (!m_dir->dir.isValid()) {
        error = m_dir->dir.errorString();
        return {};
    }
    if (targetPath(entry, entrySize).isEmpty()) {
        error = QObject::tr("'%1' is not a file to view.").arg(entry);
        return {};
    }
    spp::ArchivePlugin* plugin = module(error);
    if (!plugin)
        return {};

    // The file, and in a stream archive those on the way that fit.
    const auto wanted = [this, &entry](const spp::ArchiveItem& item) {
        return item.path == entry || (m_stream && m_bytes < kLimit && !m_files.contains(item.path));
    };
    QString writeError;
    const auto start = [&](const spp::ArchiveItem& item, const QByteArray& data) {
        const QString path = targetPath(item.path, item.size);
        if (path.isEmpty())
            return;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile out(path);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
            writeError = QObject::tr("Cannot write '%1': %2").arg(path, out.errorString());
            return;
        }
        m_files.insert(item.path, path);
        m_bytes += data.size();
    };

    FileOperationProgressDialog progress(QObject::tr("View"), parent);
    KeyRouter::instance().setOperationInProgress(true);
    const spp::ArchivePassword password = passwordAsker(&progress);
    QString until = readUntil(entry);
    bool ok = plugin->readEntries(m_archivePath, until, kBytes, wanted, start, error,
                                  progressOn(progress, m_archivePath), password);
    // Read by a program, the module gives only the file stopped at: when
    // reading ahead went past this one, it is asked for by itself.
    if (ok && !m_files.contains(entry) && until != entry && !progress.wasCanceled())
        ok = plugin->readEntries(m_archivePath, entry, kBytes, wanted, start, error,
                                 progressOn(progress, m_archivePath), password);
    KeyRouter::instance().setOperationInProgress(false);
    // Cancelled while reading ahead: the file itself is there.
    if (!ok && m_files.contains(entry)) {
        error.clear();
        return m_files.value(entry);
    }
    if (!ok) {
        if (progress.wasCanceled())
            error.clear();  // nothing to tell
        return {};
    }
    if (!m_files.contains(entry)) {
        error = writeError.isEmpty() ? QObject::tr("'%1' could not be read.").arg(entry) : writeError;
        return {};
    }
    return m_files.value(entry);
}

} // namespace ArchiveOperations
