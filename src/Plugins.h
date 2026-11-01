#ifndef PLUGINS_H
#define PLUGINS_H

#include <QStringList>

namespace gemini {
class ArchivePlugin;
}

// The modules of Gemini Commander (Qt plugins), loaded once from the plugin
// directories: $GEMINI_PLUGIN_PATH (directories separated by ':'), the
// installed lib/gemini-commander/plugins next to bin, and plugins/ in the
// build tree. The program works without any of them.
class Plugins {
public:
    static Plugins& instance();

    // The archive module, or null when none was loaded.
    gemini::ArchivePlugin* archive() const { return m_archive; }

    // What was found: one line per plugin file or problem.
    const QStringList& report() const { return m_report; }

    // The directories searched, in order.
    static QStringList directories();

private:
    Plugins();

    gemini::ArchivePlugin* m_archive = nullptr;
    QStringList m_report;
};

#endif // PLUGINS_H
