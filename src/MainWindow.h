#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>

QT_BEGIN_NAMESPACE
class QSplitter;
class QLineEdit;
class QTableView;
class QFileSystemModel;
class DirectoryFirstProxyModel;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onLeftPanelDoubleClick(const QModelIndex &index);
    void onRightPanelDoubleClick(const QModelIndex &index);

private:
    void setupUi();
    void setupModels();

    QSplitter *mainSplitter;
    QTableView *leftTableView;
    QTableView *rightTableView;
    QLineEdit *commandLineEdit;

    QFileSystemModel *leftSourceModel;
    QFileSystemModel *rightSourceModel;
    DirectoryFirstProxyModel *leftProxyModel;
    DirectoryFirstProxyModel *rightProxyModel;
};

#endif // MAINWINDOW_H