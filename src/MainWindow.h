#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QSplitter;
class QLineEdit;
class QTreeView;
class QFileSystemModel;
class DirectoryFirstProxyModel;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUi();
    void setupModels();

    QSplitter *mainSplitter;
    QTreeView *leftTreeView;
    QTreeView *rightTreeView;
    QLineEdit *commandLineEdit;

    QFileSystemModel *leftSourceModel;
    QFileSystemModel *rightSourceModel;
    DirectoryFirstProxyModel *leftProxyModel;
    DirectoryFirstProxyModel *rightProxyModel;
};

#endif // MAINWINDOW_H