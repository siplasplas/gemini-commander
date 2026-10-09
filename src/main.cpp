#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QProxyStyle>
#include <backward.hpp>
#include <qce/encoding/Encoding.h>

#include <cstdio>

#include "MainWindow.h"
#include "git_version.h"

namespace {

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
