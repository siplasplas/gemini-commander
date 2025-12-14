#ifndef EDITORFRAME_H
#define EDITORFRAME_H

#include <QString>
#include "mainheader.h"
#include <QMainWindow>

class QSplitter;
class QTreeView;
class MruTabWidget;
class QAction;
class QMenu;
class QStandardItemModel;
class QVBoxLayout;

namespace KTextEditor {
    class View;
}

class Editor;

/**
 * @class EditorFrame
 * @brief Core application window managing IDE components
 *
 * Integrates editor tabs, project tree, build system and LSP integration
 */
class EditorFrame : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorFrame(QWidget *parent = nullptr);
#include  "EditorFrame_decl.inc"
    void extendTabContextMenu(int tabIndex, QMenu* menu);
    ~EditorFrame();

    /// @brief Loads registered build system plugins
    void loadPlugins();

    /// @brief Closes specific editor tab
    bool cleanupBeforeTabClose(int index);

    void tabAboutToClose(int index, bool &allow_close);
    void openFileInEditor(const QString& filePath);
    void openFileInViewer(const QString &fileName);

protected:
    bool tryCloseAll();
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onOpenFileTriggered();
    void onViewFileTriggered();
    void onTreeItemExpanded(const QModelIndex& index);
    void onCloseCurrentTabTriggered();
    void onAboutTriggered();

private:
    // UI Layout Components
    QSplitter *m_mainSplitter;
    QSplitter *m_topSplitter;
    QTreeView *m_projectTree;
    MruTabWidget *m_editorTabWidget;

    QPoint m_dragPosition;
    QStandardItemModel* m_projectModel = nullptr;
    QString m_projectRootPath;

    MainHeader *m_mainHeader = nullptr;
    QVBoxLayout* m_mainLayout;
    QMenuBar *m_menuBar;
    QToolBar *m_mainToolBar;
    // Menu Actions
    QAction *m_openFileAction;
    QAction *m_viewFileAction;
    QAction *m_closeAction;
    QAction *m_exitAction;
    QAction *m_buildAction;
    QAction *m_runAction;
    QAction *m_aboutAction;

    void viewFileInEditor(const QString& filePath);
    int findTabByPath(const QString& filePath);
    QString generateUniqueTabTitle(const QString& filePath);
    void onProjectTreeKeyPressed(QKeyEvent* event);
    bool eventFilter(QObject* obj, QEvent* event);
    void onProjectTreeActivated(const QModelIndex& index);
    void createActions();
};
#endif // EDITORFRAME_H