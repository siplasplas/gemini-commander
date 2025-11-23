#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableView>

#include "editor/EditorFrame.h"

class FilePanel;
QT_BEGIN_NAMESPACE
class QSplitter;
class QLineEdit;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUi();
    bool eventFilter(QObject *obj, QEvent *event) override;
    int nPanel = 0;
    const int numPanels = 2;
    QVector<FilePanel*> panels;
    QSplitter *mainSplitter;
    QLineEdit *commandLineEdit;
    EditorFrame *editorFrame = nullptr;
    int numberForWidget(QTableView* widget);
};

#endif // MAINWINDOW_H