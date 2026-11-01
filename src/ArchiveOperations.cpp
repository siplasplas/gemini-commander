#include "ArchiveOperations.h"

#include "ArchiveUtils.h"
#include "Archives.h"
#include "CopyConflicts.h"
#include "FileOperationProgressDialog.h"
#include "FileOperations.h"
#include "Plugins.h"
#include "keys/KeyRouter.h"
#include "siplasplas/plugin/ArchivePlugin.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
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

// Runs an external program with the progress dialog: percentages it prints
// (7z with -bsp1) move the bar; without them the dialog shows that it works.
// Cancelling kills the program.
Result runWithProgress(QWidget* parent, const QString& title, const QString& program, const QStringList& args,
                       const QString& workDir, QString& error, const QByteArray& input = {},
                       QByteArray* output = nullptr)
{
    if (QStandardPaths::findExecutable(program).isEmpty()) {
        error = QObject::tr("%1 not found.").arg(program);
        return Result::Failed;
    }
    QProcess process;
    process.setWorkingDirectory(workDir);
    process.start(program, args);
    if (!process.waitForStarted()) {
        error = QObject::tr("Cannot start %1: %2").arg(program, process.errorString());
        return Result::Failed;
    }
    if (!input.isEmpty())
        process.write(input);
    process.closeWriteChannel();

    FileOperationProgressDialog progress(title, parent);
    progress.setTotals(1, 100);
    progress.beginFile(program, 100);
    KeyRouter::instance().setOperationInProgress(true);
    static const QRegularExpression percent(QStringLiteral("(\\d{1,3})%"));
    QByteArray errors;
    bool cancelled = false;
    while (!process.waitForFinished(50)) {
        const QByteArray bytes = process.readAllStandardOutput();
        if (output)
            output->append(bytes);
        const QString out = QString::fromLocal8Bit(bytes);
        auto matches = percent.globalMatch(out);
        int last = -1;
        while (matches.hasNext())
            last = matches.next().captured(1).toInt();
        if (last >= 0)
            progress.addFileBytes(last);
        errors += process.readAllStandardError();
        progress.processEvents();
        if (progress.wasCanceled()) {
            process.kill();
            process.waitForFinished();
            cancelled = true;
            break;
        }
    }
    KeyRouter::instance().setOperationInProgress(false);
    if (cancelled)
        return Result::Cancelled;
    progress.endFile();
    if (output)
        output->append(process.readAllStandardOutput());
    errors += process.readAllStandardError();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        error = QObject::tr("%1 failed with exit code %2:\n%3")
                    .arg(program).arg(process.exitCode()).arg(QString::fromLocal8Bit(errors).trimmed());
        return Result::Failed;
    }
    return Result::Done;
}

// Without the module, or a format it cannot read: 7z, else unar.
Result extractWithProcess(QWidget* parent, const QString& archivePath, const QString& destDir, QString& error)
{
    QDir().mkpath(destDir);
    if (!QStandardPaths::findExecutable(QStringLiteral("7z")).isEmpty()) {
        const Result result = runWithProgress(
            parent, QObject::tr("Extract"), QStringLiteral("7z"),
            {"x", "-y", "-bsp1", QStringLiteral("-o%1").arg(destDir), archivePath}, destDir, error);
        if (result != Result::Failed)
            return result;
        // 7z cannot read it: unar may.
    }
    if (!QStandardPaths::findExecutable(QStringLiteral("unar")).isEmpty()) {
        error.clear();
        return runWithProgress(parent, QObject::tr("Extract"), QStringLiteral("unar"),
                               {"-f", "-o", destDir, archivePath}, destDir, error);
    }
    if (error.isEmpty())
        error = QObject::tr("No extraction tool found. Install 7z or unar.");
    return Result::Failed;
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
// dialog (bytes of the archive read) and passwords.
Result extractWithModule(QWidget* parent, spp::ArchivePlugin* plugin, const QString& archivePath,
                         const QString& destDir, const QStringList& entries,
                         const spp::ExtractConflictHandler& conflict, QString& error)
{
    FileOperationProgressDialog progress(QObject::tr("Extract"), parent);
    KeyRouter::instance().setOperationInProgress(true);
    bool started = false;
    qint64 reported = 0;
    const bool ok = plugin->extract(
        archivePath, destDir, entries, error,
        [&](qint64 done, qint64 total, const QString&) {
            // The total from the module: all volumes of the archive.
            if (!started) {
                progress.setTotals(1, quint64(total));
                progress.beginFile(QFileInfo(archivePath).fileName(), total);
                started = true;
            }
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
    // What the module read before it broke off (e.g. solid RAR 1.5), used
    // only when lsar cannot read the archive either.
    QList<spp::ArchiveItem> partial;
    if (spp::ArchivePlugin* plugin = Plugins::instance().archive()) {
        FileOperationProgressDialog progress(QObject::tr("Open Archive"), parent);
        bool started = false;
        QList<spp::ArchiveItem> items;
        KeyRouter::instance().setOperationInProgress(true);
        const bool ok = plugin->list(
            archivePath, items, error,
            [&](qint64 done, qint64 total, const QString&) {
                if (!started) {
                    progress.setTotals(1, quint64(total));
                    progress.beginFile(QFileInfo(archivePath).fileName(), total);
                    started = true;
                }
                progress.addFileBytes(done);
                return !progress.wasCanceled();
            },
            passwordAsker(&progress));
        KeyRouter::instance().setOperationInProgress(false);
        if (progress.wasCanceled())
            return Result::Cancelled;
        if (ok && !items.isEmpty()) {
            appendArchiveItems(contents, items);
            contents.readByPlugin = true;
            return Result::Done;
        }
        partial = items;
    }
    // Without the module, or a format it cannot read: lsar.
    const QString moduleError = error;
    QByteArray json;
    QString lsarError;
    const Result result = runWithProgress(parent, QObject::tr("Open Archive"), QStringLiteral("lsar"),
                                          {"-j", archivePath}, QFileInfo(archivePath).absolutePath(), lsarError,
                                          {}, &json);
    if (result == Result::Cancelled)
        return result;
    if (result == Result::Done)
        contents = contentsFromLsar(archivePath, json);
    if (contents.allEntries.isEmpty() && !partial.isEmpty()) {
        // Better part of it than nothing; Done with a warning in `error`.
        appendArchiveItems(contents, partial);
        error = QObject::tr("Only part of '%1' could be read:\n%2")
                    .arg(QFileInfo(archivePath).fileName(), moduleError);
        return Result::Done;
    }
    if (contents.allEntries.isEmpty()) {
        // The module's reason (e.g. "Unrecognized archive format") says more.
        const QString reason = !moduleError.isEmpty() ? moduleError : lsarError;
        error = QObject::tr("'%1' cannot be read as an archive.").arg(QFileInfo(archivePath).fileName());
        if (!reason.isEmpty())
            error += QLatin1Char('\n') + reason;
        return Result::Failed;
    }
    error.clear();
    return Result::Done;
}

Result extract(QWidget* parent, const ArchiveContents& contents, const QString& destDir, QString& error)
{
    spp::ArchivePlugin* plugin = Plugins::instance().archive();
    if (!plugin || !contents.readByPlugin)
        return extractWithProcess(parent, contents.archivePath, destDir, error);

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
    return extractWithModule(parent, plugin, contents.archivePath, destDir, {}, conflict, error);
}

Result copyOut(QWidget* parent, const ArchiveContents& contents, const QString& archiveDir,
               const QStringList& entries, const QString& destDir, QString& error)
{
    spp::ArchivePlugin* plugin = Plugins::instance().archive();
    // Listed by lsar (no module, or a format it cannot read): 7z extracts.
    const bool by7z = !plugin || !contents.readByPlugin;
    if (by7z && QStandardPaths::findExecutable(QStringLiteral("7z")).isEmpty()) {
        error = QObject::tr("Copying files out of this archive needs the 7z program; "
                            "Alt+F9 extracts all of it.");
        return Result::Failed;
    }
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
    // 7z takes the entries as names: a directory comes with its contents.
    // "-p-": an encrypted archive fails instead of waiting for a password.
    const Result result =
        by7z ? runWithProgress(parent, QObject::tr("Extract"), QStringLiteral("7z"),
                               QStringList{"x", "-y", "-bsp1", "-p-", QStringLiteral("-o%1").arg(staging.path()),
                                           contents.archivePath, "--"} + entries,
                               staging.path(), error)
             : extractWithModule(parent, plugin, contents.archivePath, staging.path(), entries, {}, error);
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

    spp::ArchivePlugin* plugin = Plugins::instance().archive();
    // libarchive writes neither 7z's solid block size nor an encrypted 7z.
    const bool by7z = request.format == QLatin1String("7z")
                      && (!request.solidBlockSize.isEmpty() || !request.password.isEmpty());
    Result result;
    if (plugin && !by7z) {
        spp::PackOptions options;
        options.format = request.format;
        options.volumeSize = volumeBytes;
        options.level = request.level;
        options.password = request.password;
        FileOperationProgressDialog progress(QObject::tr("Pack"), parent);
        bool started = false;
        qint64 reported = 0;
        KeyRouter::instance().setOperationInProgress(true);
        const bool ok = plugin->pack(
            archivePath, baseDir, files, options, error,
            [&](qint64 done, qint64 total, const QString&) {
                if (!started) {
                    progress.setTotals(1, quint64(total));
                    progress.beginFile(QFileInfo(archivePath).fileName(), total);
                    started = true;
                }
                progress.addTransferred(done - reported);  // for the speed
                reported = done;
                progress.addFileBytes(done);
                return !progress.wasCanceled();
            });
        KeyRouter::instance().setOperationInProgress(false);
        result = ok ? Result::Done : progress.wasCanceled() ? Result::Cancelled : Result::Failed;
    } else if (request.format == QLatin1String("7z")) {
        // A solid block size, a password (or no module): 7z.
        QStringList args = {"a", "-bsp1"};
        if (volumeBytes > 0)
            args << QStringLiteral("-v%1").arg(request.volumeSize);
        if (!request.solidBlockSize.isEmpty())
            args << QStringLiteral("-ms=%1").arg(request.solidBlockSize) << "-mqs=on";
        if (request.level >= 0)
            args << QStringLiteral("-mx=%1").arg(request.level);
        // The password on 7z's standard input (asked twice), not on the
        // command line, which other users can see; names encrypted too.
        QByteArray input;
        if (!request.password.isEmpty()) {
            args << "-p" << "-mhe=on";
            const QByteArray password = request.password.toUtf8() + '\n';
            input = password + password;
        }
        // The names in a list file: thousands of them do not fit the command line.
        QTemporaryDir listDir;
        QFile list(listDir.filePath(QStringLiteral("files.txt")));
        if (!listDir.isValid() || !list.open(QIODevice::WriteOnly)) {
            error = QObject::tr("Failed to create temporary file list.");
            return Result::Failed;
        }
        for (const QString& file : files)
            list.write(file.toUtf8() + '\n');
        list.close();
        args << "-scsUTF-8" << archivePath << QStringLiteral("@%1").arg(list.fileName());
        result = runWithProgress(parent, QObject::tr("Pack"), QStringLiteral("7z"), args, baseDir, error, input);
    } else if (request.format == QLatin1String("zip")) {
        // No module: zip (no volumes, no password), names from its standard input.
        QByteArray names;
        for (const QString& file : files)
            names += file.toUtf8() + '\n';
        QStringList args = {"-r", "-y"};
        if (request.level >= 0)
            args << QStringLiteral("-%1").arg(request.level);
        args << archivePath << "-@";
        result = runWithProgress(parent, QObject::tr("Pack"), QStringLiteral("zip"), args, baseDir, error, names);
    } else {
        error = QObject::tr("Packing %1 needs the archive module.").arg(request.format);
        return Result::Failed;
    }

    if (result != Result::Done) {
        // A process stopped halfway leaves its output; the module does not.
        QFile::remove(archivePath);
        for (const QString& volume : volumeFiles(archivePath))
            QFile::remove(volume);
        return result;
    }
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
    , m_byModule(contents.readByPlugin)
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

QString PreviewCache::fileByProgram(QWidget* parent, const QString& entry, qint64 entrySize, QString& error)
{
    if (QStandardPaths::findExecutable(QStringLiteral("7z")).isEmpty()) {
        error = QObject::tr("Viewing files inside this archive needs the 7z program.");
        return {};
    }
    // The file to 7z's standard output, stopped after kBytes; "-p-": an
    // encrypted archive fails instead of waiting for a password.
    QProcess process;
    process.start(QStringLiteral("7z"), {"e", "-so", "-p-", m_archivePath, "--", entry});
    if (!process.waitForStarted()) {
        error = QObject::tr("Cannot start 7z: %1").arg(process.errorString());
        return {};
    }
    process.closeWriteChannel();
    FileOperationProgressDialog progress(QObject::tr("View"), parent);
    progress.setTotals(1, quint64(kBytes));
    progress.beginFile(entry, kBytes);
    KeyRouter::instance().setOperationInProgress(true);
    QByteArray data;
    bool cancelled = false;
    while (data.size() < kBytes) {
        if (!process.waitForReadyRead(50) && process.state() == QProcess::NotRunning)
            break;
        data += process.readAllStandardOutput();
        progress.addFileBytes(qMin<qint64>(data.size(), kBytes));
        progress.processEvents();
        if (progress.wasCanceled()) {
            cancelled = true;
            break;
        }
    }
    KeyRouter::instance().setOperationInProgress(false);
    if (process.state() != QProcess::NotRunning) {
        process.kill();  // the rest is not needed
        process.waitForFinished();
    } else {
        data += process.readAllStandardOutput();
        if (process.exitCode() != 0 && data.isEmpty()) {
            error = QObject::tr("7z cannot read '%1':\n%2")
                        .arg(entry, QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
            return {};
        }
    }
    if (cancelled)
        return {};
    data.truncate(kBytes);
    const QString path = targetPath(entry, entrySize);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
        error = QObject::tr("Cannot write '%1': %2").arg(path, out.errorString());
        return {};
    }
    m_files.insert(entry, path);
    m_bytes += data.size();
    return path;
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
    spp::ArchivePlugin* plugin = Plugins::instance().archive();
    if (!plugin || !m_byModule)
        return fileByProgram(parent, entry, entrySize, error);

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
    bool started = false;
    KeyRouter::instance().setOperationInProgress(true);
    const bool ok = plugin->readEntries(
        m_archivePath, readUntil(entry), kBytes, wanted, start, error,
        [&](qint64 done, qint64 total, const QString&) {
            if (!started) {
                progress.setTotals(1, quint64(total));
                progress.beginFile(QFileInfo(m_archivePath).fileName(), total);
                started = true;
            }
            progress.addFileBytes(done);
            return !progress.wasCanceled();
        },
        passwordAsker(&progress));
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
