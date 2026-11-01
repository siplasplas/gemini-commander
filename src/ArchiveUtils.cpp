#include "ArchiveUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QObject>

namespace ArchiveUtils {

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

QString readUntil(const QStringList& order, const QList<qint64>& ends, const QString& entry, bool stream,
                  qint64 readAhead)
{
    const int index = int(order.indexOf(entry));
    if (!stream || index < 0 || index >= ends.size())
        return entry;
    const qint64 reached = ends[index];
    const qint64 limit = qMax(2 * reached, reached + readAhead);
    int last = index;
    while (last + 1 < ends.size() && ends[last + 1] <= limit)
        ++last;
    return order[last];
}

QString previewPath(const QString& dir, const QString& entry, qint64 entrySize, qint64 maxBytes)
{
    const QString root = QDir(dir).absolutePath();
    // Below the directory only, whatever the entry's path says ("..").
    QString path = QDir::cleanPath(QDir(root).absoluteFilePath(entry));
    if (!path.startsWith(root + QLatin1Char('/')))
        return {};
    if (maxBytes >= 0 && entrySize > maxBytes) {
        const QString tag = QObject::tr(" (first %1)").arg(QLocale().formattedDataSize(maxBytes));
        const int slash = int(path.lastIndexOf(QLatin1Char('/')));
        const int dot = int(path.lastIndexOf(QLatin1Char('.')));
        path.insert(dot > slash + 1 ? dot : path.size(), tag);
    }
    return path;
}

} // namespace ArchiveUtils
