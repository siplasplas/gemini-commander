#include "ArchiveOperations.h"

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

// "100m" -> bytes (k, m, g: KiB, MiB, GiB, as 7z reads them); 0 when empty.
qint64 sizeInBytes(const QString& size)
{
    QString number = size.trimmed().toLower();
    if (number.isEmpty())
        return 0;
    qint64 unit = 1;
    if (number.endsWith(QLatin1Char('k')))
        unit = 1024;
    else if (number.endsWith(QLatin1Char('m')))
        unit = 1024 * 1024;
    else if (number.endsWith(QLatin1Char('g')))
        unit = 1024LL * 1024 * 1024;
    if (unit > 1)
        number.chop(1);
    return qint64(number.toDouble() * double(unit));
}

// Runs an external program with the progress dialog: percentages it prints
// (7z with -bsp1) move the bar; without them the dialog shows that it works.
// Cancelling kills the program.
Result runWithProgress(QWidget* parent, const QString& title, const QString& program, const QStringList& args,
                       const QString& workDir, QString& error, const QByteArray& input = {})
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
        const QString out = QString::fromLocal8Bit(process.readAllStandardOutput());
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

Result extract(QWidget* parent, const ArchiveContents& contents, const QString& destDir, QString& error)
{
    spp::ArchivePlugin* plugin = Plugins::instance().archive();
    if (!plugin || !contents.readByPlugin)
        return extractWithProcess(parent, contents.archivePath, destDir, error);

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

Result pack(QWidget* parent, const QString& baseDir, const QStringList& files, const QString& archivePath,
            const PackRequest& request, QString& error)
{
    const qint64 volumeBytes = sizeInBytes(request.volumeSize);
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
    const bool by7z = request.format == QLatin1String("7z") && !request.solidBlockSize.isEmpty();
    Result result;
    if (plugin && !by7z) {
        spp::PackOptions options;
        options.format = request.format;
        options.volumeSize = volumeBytes;
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
        // A solid block size (or no module): 7z.
        QStringList args = {"a", "-bsp1"};
        if (volumeBytes > 0)
            args << QStringLiteral("-v%1").arg(request.volumeSize);
        if (!request.solidBlockSize.isEmpty())
            args << QStringLiteral("-ms=%1").arg(request.solidBlockSize) << "-mqs=on";
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
        result = runWithProgress(parent, QObject::tr("Pack"), QStringLiteral("7z"), args, baseDir, error);
    } else {
        // No module: zip (no volumes), names from its standard input.
        QByteArray names;
        for (const QString& file : files)
            names += file.toUtf8() + '\n';
        result = runWithProgress(parent, QObject::tr("Pack"), QStringLiteral("zip"),
                                 {"-r", "-y", archivePath, "-@"}, baseDir, error, names);
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

namespace {

// Compressed as one stream (or solid): reaching a file decompresses
// everything before it. Zip, RAR, ISO and the like are read file by file.
bool isStreamArchive(const QString& archivePath)
{
    const QString name = QFileInfo(archivePath).fileName().toLower();
    static const QStringList suffixes = {".7z", ".7z.001", ".tgz", ".tbz", ".tbz2", ".txz", ".tzst", ".taz", ".tlz"};
    for (const QString& suffix : suffixes) {
        if (name.endsWith(suffix))
            return true;
    }
    return name.contains(QLatin1String(".tar."));
}

} // namespace

PreviewCache::PreviewCache(const ArchiveContents& contents)
    : m_archivePath(contents.archivePath)
    , m_stream(isStreamArchive(contents.archivePath))
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
    const int index = m_order.indexOf(entry);
    if (!m_stream || index < 0)
        return entry;
    // As far again as reaching the file took, at least kReadAhead more.
    const qint64 reached = m_ends[index];
    const qint64 limit = qMax(2 * reached, reached + kReadAhead);
    int last = index;
    while (last + 1 < m_ends.size() && m_ends[last + 1] <= limit)
        ++last;
    return m_order[last];
}

PreviewCache::~PreviewCache() = default;

QString PreviewCache::targetPath(const QString& entry, qint64 entrySize) const
{
    const QString dir = m_dir->dir.path();
    // Below the directory only, whatever the entry's path says ("..").
    QString path = QDir::cleanPath(QDir(dir).absoluteFilePath(entry));
    if (!path.startsWith(dir + QLatin1Char('/')))
        return {};
    if (entrySize > kBytes) {
        const QString tag = QObject::tr(" (first %1)").arg(QLocale().formattedDataSize(kBytes));
        const int slash = path.lastIndexOf(QLatin1Char('/'));
        const int dot = path.lastIndexOf(QLatin1Char('.'));
        path.insert(dot > slash + 1 ? dot : path.size(), tag);
    }
    return path;
}

QString PreviewCache::file(QWidget* parent, const QString& entry, qint64 entrySize, QString& error)
{
    if (const auto kept = m_files.constFind(entry); kept != m_files.constEnd())
        return *kept;
    spp::ArchivePlugin* plugin = Plugins::instance().archive();
    if (!plugin) {
        error = QObject::tr("Viewing files inside archives needs the archive module.");
        return {};
    }
    if (!m_dir->dir.isValid()) {
        error = m_dir->dir.errorString();
        return {};
    }
    if (targetPath(entry, entrySize).isEmpty()) {
        error = QObject::tr("'%1' is not a file to view.").arg(entry);
        return {};
    }

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
