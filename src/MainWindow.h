#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QSplitter;
class QLineEdit;
// class QFrame; // Już niepotrzebne
class QTreeView;        // Dodane
class QFileSystemModel; // Dodane
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUi();
    void setupModels(); // Nowa metoda pomocnicza

    // Wskaźniki na główne elementy UI
    QSplitter *mainSplitter;
    // QFrame    *leftPanel; // Zastąpione
    // QFrame    *rightPanel; // Zastąpione
    QTreeView *leftTreeView;  // Nowy widok
    QTreeView *rightTreeView; // Nowy widok
    QLineEdit *commandLineEdit;

    // Modele danych dla widoków
    QFileSystemModel *leftModel;
    QFileSystemModel *rightModel;
};

#endif // MAINWINDOW_H