#ifndef PLUGINS_H
#define PLUGINS_H

#include <QStringList>

namespace spp {
class ArchivePlugin;
class FindPlugin;
}

// The modules of Gemini Commander (Qt plugins from siplasplas-plugins),
// loaded once from the plugin directories: $GEMINI_PLUGIN_PATH (directories
// separated by ':'), the installed lib/gemini-commander/plugins next to bin,
// and plugins/ in the build tree. The program works without any of them.
class Plugins {
public:
    static Plugins& instance();

    // The archive module, or null when none was loaded.
    spp::ArchivePlugin* archive() const { return m_archive; }
    // The find bar module, or null when none was loaded.
    spp::FindPlugin* find() const { return m_find; }

    // What was found: one line per plugin file or problem.
    const QStringList& report() const { return m_report; }

    // The directories searched, in order.
    static QStringList directories();

private:
    Plugins();

    spp::ArchivePlugin* m_archive = nullptr;
    spp::FindPlugin* m_find = nullptr;
    QStringList m_report;
};

#endif // PLUGINS_H
