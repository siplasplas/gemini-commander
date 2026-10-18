#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProxyStyle>
#include <QSettings>
#include <QStandardPaths>
#include <backward.hpp>
#include <qce/encoding/Encoding.h>
#include <qce/kate/KatePaths.h>
#include <qxmigration.h>
#include <toml++/toml.h>

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>

#include "MainWindow.h"
#include "editor/EditorSettings.h"
#include "git_version.h"

namespace {

// config.toml of an older version: its top-level tables are the ones Config reads.
bool isGeminiConfig(const QString& path)
{
    static const std::set<std::string> known = {
        "general", "window", "ui", "toolbars", "history", "tabs", "editor",
        "search", "show", "viewer", "panels", "favorites"};
    try {
        const toml::table table = toml::parse_file(path.toStdString());
        if (table.empty())
            return false;
        for (const auto& [key, value] : table) {
            if (!known.count(std::string(key.str())))
                return false;
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Moves older versions' files to ~/.config/siplasplas/gemini-commander and the Kate
// syntax data to the shared ~/.local/share/kate-data, logging to stderr.
void migrateFiles(QApplication& app)
{
    QxMigrationSpec spec;
    spec.configEntries = {QStringLiteral("config.toml")};
    spec.recognize = [](const QString& path) {
        return !path.endsWith(QLatin1String("config.toml")) || isGeminiConfig(path);
    };
    const QxMigrationResult moved = qxMigrateToSiplasplas(spec);
    for (const QString& line : moved.log)
        std::fprintf(stderr, "Moving files: %s\n", qPrintable(line));
    if (moved.status == QxMigrationResult::Failed) {
        // ~/.config/gemini-commander for this run; the next start tries again.
        app.setOrganizationName(QString());
    } else {
        // Settings of the KTextEditor-based editor of old versions, read by nothing now.
        const QString oldEditorSettings =
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/gemini-commanderrc");
        if (QFile::exists(oldEditorSettings)) {
            const QStringList groups = QSettings(oldEditorSettings, QSettings::IniFormat).childGroups();
            const bool editorOnly = !groups.isEmpty()
                && std::all_of(groups.cbegin(), groups.cend(), [](const QString& group) {
                       return group.startsWith(QLatin1String("KTextEditor"));
                   });
            if (editorOnly && QFile::remove(oldEditorSettings))
                std::fprintf(stderr, "Moving files: Removed %s, settings of the former editor\n",
                             qPrintable(oldEditorSettings));
        }
    }

    const qce::kate::DataRootMigration kate = qce::kate::migrateDataRoot();
    for (const QString& line : kate.log)
        std::fprintf(stderr, "Moving Kate data: %s\n", qPrintable(line));
    for (const QString& line : kate.warnings)
        std::fprintf(stderr, "Moving Kate data: %s\n", qPrintable(line));
}

// Alt pressed and released alone puts the menu bar in keyboard mode, which
// takes the focus from the panel. Off here; Alt+letter still opens a menu.
class NoAltMenuNavigationStyle : public QProxyStyle {
public:
    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* returnData) const override {
        if (hint == SH_MenuBar_AltKeyNavigation)
            return 0;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle(new NoAltMenuNavigationStyle);

    const QString crashLogPath = QDir(QCoreApplication::applicationDirPath())
                                     .filePath(QStringLiteral("gemini-commander-crash.log"));
    if (std::freopen(crashLogPath.toLocal8Bit().constData(), "a", stderr)) {
        std::setvbuf(stderr, nullptr, _IONBF, 0);
        std::fprintf(stderr, "\n=== Gemini Commander %s (%s) ===\n", APP_VERSION, GIT_SHA);
    }

    // backward-cpp writes a stack trace to stderr after fatal signals and
    // unhandled C++ exceptions; stderr above points at the crash log.
    static backward::SignalHandling crashHandler;

    // Configuration in ~/.config/siplasplas/gemini-commander, data in
    // ~/.local/share/siplasplas/gemini-commander.
    app.setOrganizationName(QStringLiteral("siplasplas"));
    app.setApplicationName(QStringLiteral("gemini-commander"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    migrateFiles(app);

    // The editor (qceditor's library) keeps its settings in gemini's configuration.
    static EditorSettings editorSettings;
    qceditor::setSettings(&editorSettings);

    // Encoding/language models load in the background, ready for the editor.
    qce::encoding::preloadDetectionData();

    // Collect command line arguments (skip program name)
    QStringList startupPaths;
    for (int i = 1; i < argc; ++i) {
        startupPaths << QString::fromLocal8Bit(argv[i]);
    }

    MainWindow mainWindow;
    mainWindow.applyStartupPaths(startupPaths);
    mainWindow.show();

    return app.exec();
}
