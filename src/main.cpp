#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget mainWindow;
    mainWindow.setWindowTitle("Qt Minimal");
    mainWindow.resize(400, 300);
    mainWindow.show();

    return app.exec();
}