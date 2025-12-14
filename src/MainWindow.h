#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "FilePaneWidget.h"

#include <QMainWindow>
#include <QPointer>
#include <QTableView>
#include <QProgressDialog>

#include "editor/EditorFrame.h"

class ViewerFrame;
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
#include "MainWindow_decl.inc"
private:
    void setupUi();
    static Side opposite(Side side);
    bool eventFilter(QObject *obj, QEvent *event) override;
    QTabWidget* m_leftTabs;
    QTabWidget* m_rightTabs;
    Side m_activeSide = Side::Left;
    QTabWidget* tabsForSide(Side side) const;
    void goToNextTab(QTabWidget *tabWidget);
    void goToPreviousTab(QTabWidget *tabWidget);
    QLineEdit *commandLineEdit;
    EditorFrame *editorFrame = nullptr;
    QPointer<ViewerFrame> viewerFrame;
    int numberForWidget(QTableView* widget);
    void showFavoriteDirsMenu(Side side);

    void reloadAllPanels();
    void showFavoriteDirsMenu();
    QVector<FilePanel*> allFilePanels() const;
    FilePanel *panelForObject(QObject *obj) const;

    FilePaneWidget* currentPane() const;
    FilePanel* currentFilePanel() const;

    FilePanel* oppositeFilePanel() const;

    FilePaneWidget* paneForSide(Side side) const;
    FilePanel* filePanelForSide(Side side) const;

    QToolBar* m_mainToolBar = nullptr;
    QToolBar* m_mountsToolBar = nullptr;;
    QAction* m_openTerminalAction = nullptr;

    void createMountsToolbar();
    QStringList listMountPoints() const;
    void copyFromPanel(FilePanel* srcPanel);

    struct CopyStats {
        quint64 totalBytes = 0;
        quint64 totalFiles = 0;
        quint64 totalDirs  = 0;
    };

    void collectCopyStats(const QString& srcPath, CopyStats& stats, bool& ok);
    bool copyDirectoryRecursive(const QString& srcRoot,
                                const QString& dstRoot,
                                const CopyStats& stats,
                                QProgressDialog& progress,
                                quint64& bytesCopied,
                                bool& userAbort);
    bool handle(const char* handler, QKeyEvent *ev);
private slots:
    void onOpenTerminal();
};

#endif // MAINWINDOW_H