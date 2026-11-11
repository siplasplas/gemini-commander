#include "Plugins.h"

#include "siplasplas/plugin/ArchivePlugin.h"
#include "siplasplas/plugin/FindPlugin.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibrary>
#include <QPluginLoader>

Plugins& Plugins::instance()
{
    static Plugins plugins;
    return plugins;
}

QStringList Plugins::directories()
{
    QStringList result;
    const QString variable = qEnvironmentVariable("GEMINI_PLUGIN_PATH");
    for (const QString& dir : variable.split(QLatin1Char(':'), Qt::SkipEmptyParts))
        result << dir;
    const QString bin = QCoreApplication::applicationDirPath();
    // Set by CMake, e.g. "../lib/gemini-commander/plugins".
    result << QDir::cleanPath(bin + QStringLiteral("/" GEMINI_PLUGIN_DIR_FROM_BIN));
    result << QDir::cleanPath(bin + QStringLiteral("/plugins"));  // build tree
    result.removeDuplicates();
    return result;
}

Plugins::Plugins()
{
    for (const QString& path : directories()) {
        const QDir dir(path);
        if (!dir.exists())
            continue;
        for (const QString& name : dir.entryList(QDir::Files, QDir::Name)) {
            const QString file = dir.absoluteFilePath(name);
            if (!QLibrary::isLibrary(file))
                continue;
            QPluginLoader loader(file);
            QObject* plugin = loader.instance();
            if (!plugin) {
                m_report << QStringLiteral("%1: not loaded: %2").arg(file, loader.errorString());
                continue;
            }
            // The first plugin of each kind is used.
            if (auto* archive = qobject_cast<spp::ArchivePlugin*>(plugin)) {
                if (m_archive) {
                    m_report << QStringLiteral("%1: archives %2, not used: one is loaded")
                                    .arg(file, archive->name());
                } else {
                    m_archive = archive;
                    m_report << QStringLiteral("%1: archives %2").arg(file, archive->name());
                }
                continue;
            }
            if (auto* find = qobject_cast<spp::FindPlugin*>(plugin)) {
                if (m_find) {
                    m_report << QStringLiteral("%1: find bar %2, not used: one is loaded").arg(file, find->name());
                } else {
                    m_find = find;
                    m_report << QStringLiteral("%1: find bar %2").arg(file, find->name());
                }
                continue;
            }
            m_report << QStringLiteral("%1: not a Gemini Commander plugin").arg(file);
            loader.unload();
        }
    }
}
