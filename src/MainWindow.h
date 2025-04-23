#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QSplitter;
class QLineEdit;
class QFrame;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUi();

    QSplitter *mainSplitter;
    QFrame    *leftPanel;
    QFrame    *rightPanel;
    QLineEdit *commandLineEdit;
};

#endif // MAINWINDOW_H