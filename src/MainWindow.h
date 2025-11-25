#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "FilePaneWidget.h"

#include <QMainWindow>
#include <QTableView>
#include <QProgressDialog>

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

private:
    void setupUi();
    bool eventFilter(QObject *obj, QEvent *event) override;
    QTabWidget* m_leftTabs;
    QTabWidget* m_rightTabs;
    int m_activeSide = 0; // 0 = left, 1 = right
    QTabWidget* tabsForSide(int side) const;
    void goToNextTab(QTabWidget *tabWidget);
    void goToPreviousTab(QTabWidget *tabWidget);
    QLineEdit *commandLineEdit;
    EditorFrame *editorFrame = nullptr;
    int numberForWidget(QTableView* widget);
    void showFavoriteDirsMenu(int side);

    void reloadAllPanels();
    void showFavoriteDirsMenu();
    QVector<FilePanel*> allFilePanels() const;
    enum Side {
        LeftSide  = 0,
        RightSide = 1
    };

    // Zwróć aktywną stronę
    int activeSide() const { return m_activeSide; }
    void setActiveSide(int side);
    FilePanel *panelForObject(QObject *obj) const;
    int sideForPanel(FilePanel *panel) const;

    // Dostęp do aktualnego FilePaneWidget na aktywnej stronie
    FilePaneWidget* currentPane() const;
    FilePanel* currentFilePanel() const;

    // Dostęp do panelu po stronie (left/right)
    FilePaneWidget* paneForSide(int side) const;
    FilePanel* filePanelForSide(int side) const;

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



private slots:
    void onOpenTerminal();
};

#endif // MAINWINDOW_H