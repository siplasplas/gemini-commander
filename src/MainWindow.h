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
#include "udisks/UDisksDeviceManager.h"

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
#include "MainWindow_decl.inc"
protected:
    void closeEvent(QCloseEvent *event) override;
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
    QToolBar* m_mountsToolBar = nullptr;;
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

    void applyConfigGeometry();

    // Directory monitoring
    QFileSystemWatcher* m_dirWatcher = nullptr;
    void updateWatchedDirectories();

    // Mounts monitoring via UDisks2
    UDisksDeviceManager* m_udisksManager = nullptr;
    void refreshMountsToolbar();
    void onDeviceMounted(const QString &objectPath, const QString &mountPoint);
    void onDeviceUnmounted(const QString &objectPath);

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
};

#endif // MAINWINDOW_H