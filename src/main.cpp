#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <backward.hpp>

#include <cstdio>

#include "MainWindow.h"
#include "git_version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QString crashLogPath = QDir(QCoreApplication::applicationDirPath())
                                     .filePath(QStringLiteral("gemini-commander-crash.log"));
    if (std::freopen(crashLogPath.toLocal8Bit().constData(), "a", stderr)) {
        std::setvbuf(stderr, nullptr, _IONBF, 0);
        std::fprintf(stderr, "\n=== Gemini Commander %s (%s) ===\n", APP_VERSION, GIT_SHA);
    }

    // backward-cpp writes a stack trace to stderr after fatal signals and
    // unhandled C++ exceptions; stderr above points at the crash log.
    static backward::SignalHandling crashHandler;

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
