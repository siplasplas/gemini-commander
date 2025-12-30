#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "FilePaneWidget.h"

#include <QMainWindow>
#include <QPointer>
#include <QTableView>
#include <QProgressDialog>
#include <QToolButton>
#include <QFileSystemWatcher>

#include "editor/EditorFrame.h"
#include "keys/KeyMap.h"
#ifndef _WIN32
#include "udisks/UDisksDeviceManager.h"
#include "mounts/ProcMountsManager.h"
#endif

class FunctionBar;
class ViewerFrame;
class FilePanel;
class SearchDialog;
QT_BEGIN_NAMESPACE
class QSplitter;
class QLineEdit;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void applyStartupPaths(const QStringList& paths);
#include "MainWindow_decl.inc"
protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    bool m_geometryDirty = false;  // True if user resized window interactively
    void setupUi();
    static Side opposite(Side side);
    bool eventFilter(QObject *obj, QEvent *event) override;
    QTabWidget* m_leftTabs;
    QTabWidget* m_rightTabs;
    Side m_activeSide = Side::Left;
    QTabWidget* tabsForSide(Side side) const;
    void goToNextTab(QTabWidget *tabWidget);
    void goToPreviousTab(QTabWidget *tabWidget);
    QLabel *currentPathLabel;
    QLineEdit *commandLineEdit;
    EditorFrame *editorFrame = nullptr;
    QPointer<ViewerFrame> viewerFrame;
    SearchDialog* m_searchDialog = nullptr;
    int numberForWidget(QTableView* widget);
    void showFavoriteDirsMenu(Side side, const QPoint& pos = QPoint());

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
    QToolBar* m_mountsToolBar = nullptr;
    QToolBar* m_procMountsToolBar = nullptr;
    QAction* m_openTerminalAction = nullptr;
    QAction* m_searchAction = nullptr;
    QAction* m_externalToolAction = nullptr;

    void createMountsToolbar();
    void copyFromPanel(FilePanel* srcPanel, bool inPlace = false);
    void moveFromPanel(FilePanel* srcPanel, bool inPlace = false);
    KeyMap keyMap;

    bool handle(const char* handler, QKeyEvent *ev);
    QString currentPanelName();
    QString currentPanelPath();
    void updateCurrentPathLabel();

    void updateExternalToolButton();
    QString findDesktopFile(const QString& executablePath);
    QString extractIconFromDesktop(const QString& desktopFilePath);

    // Methods for opening editor/viewer with specific file
    void openEditorForFile(const QString& filePath);
    void openViewerForFile(const QString& filePath);
    void goToFile(const QString& dir, const QString& name);

    void applyConfigGeometry(bool isStartup = true);

    // Directory monitoring
    QFileSystemWatcher* m_dirWatcher = nullptr;
    void updateWatchedDirectories();

    // File monitoring (visible files only)
    QFileSystemWatcher* m_leftFileWatcher = nullptr;
    QFileSystemWatcher* m_rightFileWatcher = nullptr;
    void updateFileWatcher(Side side, const QStringList& paths);
    void ensureFileWatcherActive(QFileSystemWatcher* watcher);

    void refreshMountsToolbar();

#ifndef _WIN32
    // Mounts monitoring via UDisks2 (Linux only)
    UDisksDeviceManager* m_udisksManager = nullptr;
    void onDeviceMounted(const QString &objectPath, const QString &mountPoint);
    void onDeviceUnmounted(const QString &objectPath);

    // Mounts monitoring via /proc/mounts (Linux only)
    ProcMountsManager* m_procMountsManager = nullptr;
#endif
    void createProcMountsToolbar();
    void refreshProcMountsToolbar();

    // Function bar
    FunctionBar* m_functionBar = nullptr;
    QAction* m_showFunctionBarAction = nullptr;

public slots:
    void onConfigSaved();

private slots:
    void onOpenTerminal();
    void onExternalToolClicked();
    void onExternalToolContextMenu(const QPoint& pos);
    void configureExternalTool();
    void onDirectoryChanged(const QString& path);
    void onVisibleFilesChanged(Side side, const QStringList& paths);
    void onLeftFileChanged(const QString& path);
    void onRightFileChanged(const QString& path);
};

#endif // MAINWINDOW_H