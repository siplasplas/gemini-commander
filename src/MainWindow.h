#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QString>

QT_BEGIN_NAMESPACE
class QSplitter;
class QLineEdit;
class QTableView;
class QStandardItemModel;
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
    void loadDirectory(QStandardItemModel *model, const QString &path, QTableView *view);

    QSplitter *mainSplitter;
    QTableView *leftTableView;
    QTableView *rightTableView;
    QLineEdit *commandLineEdit;

    QStandardItemModel *leftModel;
    QStandardItemModel *rightModel;

    QString leftCurrentPath;
    QString rightCurrentPath;
};

#endif // MAINWINDOW_H