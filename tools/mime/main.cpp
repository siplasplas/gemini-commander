#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MimeExplorer");
    app.setApplicationVersion("1.0");

    // Set fallback icon theme if none is set (use Adwaita for better icon coverage)
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName("Adwaita");
    }
    // Set fallback theme for icons not found in primary theme
    QIcon::setFallbackThemeName("hicolor");
    // Add fallback search paths
    QIcon::setFallbackSearchPaths(QIcon::fallbackSearchPaths()
        << "/usr/share/icons"
        << "/usr/share/pixmaps");

    MainWindow window;
    window.show();

    return app.exec();
}
