// Gemini Commander's archive module on libarchive.

#include "plugin/ArchivePlugin.h"

#include <QFile>
#include <QObject>

#include <archive.h>
#include <archive_entry.h>

#include <memory>

namespace {

using ArchivePtr = std::unique_ptr<struct archive, int (*)(struct archive*)>;

ArchivePtr openForReading(const QString& path, QString& error)
{
    ArchivePtr a(archive_read_new(), archive_read_free);
    archive_read_support_filter_all(a.get());
    archive_read_support_format_all(a.get());
    if (archive_read_open_filename(a.get(), QFile::encodeName(path).constData(), 64 * 1024) != ARCHIVE_OK) {
        error = QString::fromLocal8Bit(archive_error_string(a.get()));
        return ArchivePtr(nullptr, archive_read_free);
    }
    return a;
}

// The entry's path: UTF-8 when the archive says so, else in the locale's
// encoding (old zips name their files in a DOS code page, unknown here).
QString entryPath(struct archive_entry* entry)
{
    if (const char* utf8 = archive_entry_pathname_utf8(entry))
        return QString::fromUtf8(utf8);
    if (const char* raw = archive_entry_pathname(entry))
        return QString::fromLocal8Bit(raw);
    return {};
}

} // namespace

class LibarchivePlugin : public QObject, public gemini::ArchivePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID GEMINI_ARCHIVE_PLUGIN_IID)
    Q_INTERFACES(gemini::ArchivePlugin)

public:
    QString name() const override
    {
        return QStringLiteral("libarchive %1").arg(QString::fromLatin1(archive_version_details()).section(' ', 1, 1));
    }

    bool list(const QString& archivePath, QList<gemini::ArchiveItem>& items, QString& error) override
    {
        ArchivePtr a = openForReading(archivePath, error);
        if (!a)
            return false;
        struct archive_entry* entry;
        int status;
        while ((status = archive_read_next_header(a.get(), &entry)) == ARCHIVE_OK
               || status == ARCHIVE_WARN) {
            gemini::ArchiveItem item;
            item.path = entryPath(entry);
            while (item.path.endsWith(QLatin1Char('/')))
                item.path.chop(1);
            if (item.path.startsWith(QLatin1String("./")))
                item.path.remove(0, 2);
            item.isDirectory = archive_entry_filetype(entry) == AE_IFDIR;
            item.size = archive_entry_size(entry);
            if (archive_entry_mtime_is_set(entry))
                item.modTime = QDateTime::fromSecsSinceEpoch(archive_entry_mtime(entry));
            if (!item.path.isEmpty() && item.path != QLatin1String("."))
                items.append(item);
            archive_read_data_skip(a.get());
        }
        if (status != ARCHIVE_EOF) {
            error = QString::fromLocal8Bit(archive_error_string(a.get()));
            // A damaged end still shows what was read.
            return !items.isEmpty();
        }
        return true;
    }
};

#include "LibarchivePlugin.moc"
