#ifndef GEMINI_PLUGIN_ARCHIVEPLUGIN_H
#define GEMINI_PLUGIN_ARCHIVEPLUGIN_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QtPlugin>

// The archive module: reading (and later extracting and packing) archives in
// a Qt plugin, so that the main program does not depend on the archive
// library (libarchive). Without the plugin, archives are listed by `lsar`.
namespace gemini {

// An entry of an archive.
struct ArchiveItem {
    QString path;          // inside the archive, '/' separated, no trailing '/'
    bool isDirectory = false;
    qint64 size = 0;       // uncompressed
    QDateTime modTime;     // invalid when the archive has none
};

class ArchivePlugin {
public:
    virtual ~ArchivePlugin() = default;

    // For the plugin list, e.g. "libarchive 3.8.5".
    virtual QString name() const = 0;

    // All entries of the archive. False with `error` set when it cannot be
    // read (not an archive, unsupported format or encryption, damaged).
    virtual bool list(const QString& archivePath, QList<ArchiveItem>& items, QString& error) = 0;
};

} // namespace gemini

// The version changes with the interface: plugins built for another one are
// not loaded.
#define GEMINI_ARCHIVE_PLUGIN_IID "io.github.siplasplas.GeminiCommander.ArchivePlugin/1"
Q_DECLARE_INTERFACE(gemini::ArchivePlugin, GEMINI_ARCHIVE_PLUGIN_IID)

#endif // GEMINI_PLUGIN_ARCHIVEPLUGIN_H
