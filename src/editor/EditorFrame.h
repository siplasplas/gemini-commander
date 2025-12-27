#ifndef EDITORFRAME_H
#define EDITORFRAME_H

#include <QString>
#include "mainheader.h"
#include <QMainWindow>

class MruTabWidget;
class QAction;
class QMenu;
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
    bool actionsBeforeTabClose(int index);

    void tabAboutToClose(int index, bool askPin, bool &allow_close);
    void newFile();
    void openFile(const QString& filePath);
    void openFileInViewer(const QString &fileName);
    Editor* currentEditor() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onNewFileTriggered();
    void onOpenFileTriggered();
    void onCloseCurrentTabTriggered();
    void onAboutTriggered();
    void onToggleSpecialChars(bool checked);
    void onToggleWrapLines(bool checked);
    void onFindTriggered();
    void onFindNextTriggered();
    void onFindPrevTriggered();
    void onReplaceTriggered();
    void onGotoTriggered();
    void onInsertDateTriggered();
    void onInsertTimeTriggered();
    void onInsertBothTriggered();

private:
    MruTabWidget *m_editorTabWidget;

    MainHeader *m_mainHeader = nullptr;
    QVBoxLayout* m_mainLayout;

    // Menu Actions
    QAction *m_newFileAction;
    QAction *m_openFileAction;
    QAction *m_viewFileAction;
    QAction *m_closeAction;
    QAction *m_exitAction;
    QAction *m_aboutAction;
    QAction *m_showSpecialCharsAction;
    QAction *m_wrapLinesAction;
    QAction *m_findAction;
    QAction *m_findNextAction;
    QAction *m_findPrevAction;
    QAction *m_replaceAction;
    QAction *m_gotoAction;
    QAction *m_insertDateAction;
    QAction *m_insertTimeAction;
    QAction *m_insertBothAction;

    void viewFileInEditor(const QString& filePath);
    int findTabByPath(const QString& filePath);
    QString generateUniqueTabTitle(const QString& filePath);
    void createActions();
    void saveGeometryToConfig();

    bool m_geometryRestored = false;
};
#endif // EDITORFRAME_H