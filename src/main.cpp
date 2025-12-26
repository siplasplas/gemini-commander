#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

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