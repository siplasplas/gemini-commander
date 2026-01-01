#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "FilePaneWidget.h"

#include <QMainWindow>
#include <QPointer>
#include <QSet>
#include <QTableView>
#include <QProgressDialog>
#include <QTimer>
#include <QToolButton>
#include <QFileSystemWatcher>

#include "FileOperations.h"
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
class MruTabWidget;
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
    MruTabWidget* m_leftTabs;
    MruTabWidget* m_rightTabs;
    Side m_activeSide = Side::Left;
    MruTabWidget* tabsForSide(Side side) const;
    void goToNextTab(MruTabWidget *tabWidget);
    void goToPreviousTab(MruTabWidget *tabWidget);
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
    QToolBar* m_storageInfoToolBar = nullptr;
    QToolBar* m_functionBarToolBar = nullptr;
    QAction* m_openTerminalAction = nullptr;
    QAction* m_searchAction = nullptr;

    void createMountsToolbar();
    void createNewDirectory(QWidget *dialogParent);
    void selectNameAfterFileOperation(FilePanel *srcPanel, FilePanel *dstPanel, const QString &srcName, const QString &relativeName);
    void selectPathAfterFileOperation(FilePanel *srcPanel, FilePanel *dstPanel, const QString &srcPath, const QString& selectedPath);
    void selectNameAfterDelete(FilePanel *srcPanel, FilePanel *dstPanel, const QString &deletedName);
    FileOperations::Params askForFileOperation(FilePanel* srcPanel, bool inPlace, bool isMove);

    KeyMap keyMap;

    bool handle(const char* handler, QKeyEvent *ev);
    QString currentPanelName();
    QString currentPanelPath();
    void updateCurrentPathLabel();

    // Methods for opening editor/viewer with specific file
    void openEditorForFile(const QString& filePath);
    void openViewerForFile(const QString& filePath);
    void goToFile(const QString& dir, const QString& name);

    void applyConfigGeometry(bool isStartup = true);

    // Directory monitoring
    QFileSystemWatcher* m_dirWatcher = nullptr;
    bool m_suppressDirWatcher = false;  // Suppress reload during file operations
    QTimer* m_dirChangeDebounceTimer = nullptr;
    QSet<QString> m_pendingDirChanges;  // Paths waiting for debounced reload
    void updateWatchedDirectories();
    void processPendingDirChanges();

    // File monitoring (visible files only)
    QFileSystemWatcher* m_leftFileWatcher = nullptr;
    QFileSystemWatcher* m_rightFileWatcher = nullptr;
    void updateFileWatcher(Side side, const QStringList& paths);
    void ensureFileWatcherActive(QFileSystemWatcher* watcher);

    void refreshMountsToolbar();
    void applyToolbarConfig();
    QToolBar* toolbarByName(const QString& name);

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
    void updateStorageInfoToolbar();

    // Function bar
    FunctionBar* m_functionBar = nullptr;
    QAction* m_showFunctionBarAction = nullptr;

public slots:
    void onConfigSaved();

private slots:
    void onOpenTerminal();
    void onDirectoryChanged(const QString& path);
    void onVisibleFilesChanged(Side side, const QStringList& paths);
    void onLeftFileChanged(const QString& path);
    void onRightFileChanged(const QString& path);
};

#endif // MAINWINDOW_H