#ifndef EDITORFRAME_H
#define EDITORFRAME_H

#include <QString>
#include "mainheader.h"
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <mrutabwidget.h>

class QAction;
class QMenu;
class QVBoxLayout;
class QStatusBar;
class QLabel;
namespace qce::kate { class KateDataDownloader; }

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
    void extendTabContextMenu(QWidget* page, QMenu* menu);
    ~EditorFrame();

    /// @brief Loads registered build system plugins
    void loadPlugins();

    /// @brief Records the file of an editor tab that is being closed in the recent list
    void actionsBeforeTabClose(QWidget* page);

    void tabAboutToClose(QWidget* page, bool askPin, bool &allow_close);
    /// @brief Asks what happens to a modified tab that a new tab would push over the limit
    void tabLimitReached(QWidget* page, MruTabWidget::LimitAction &action);
    void newFile();
    void openFile(const QString& filePath);
    void openFileInViewer(const QString &fileName);
    Editor* currentEditor() const;

    void checkAllTabsForExternalChanges();

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onNewFileTriggered();
    void onOpenFileTriggered();
    void onSaveTriggered();
    void onSaveAsTriggered();
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
    void onUpdateSyntaxTriggered();

private:
    MruTabWidget *m_editorTabWidget;

    MainHeader *m_mainHeader = nullptr;
    QVBoxLayout* m_mainLayout;

    // Menu Actions
    QAction *m_newFileAction;
    QAction *m_openFileAction;
    QAction *m_saveAction;
    QAction *m_saveAsAction;
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
    QAction *m_updateSyntaxAction;

    QStatusBar* m_statusBar = nullptr;
    // Status bar of the current editor (filled by updateStatusBar()).
    QLabel* m_statusPos = nullptr;        ///< line:column
    QLabel* m_statusCodePoint = nullptr;  ///< U+XXXX of the character after the caret
    QLabel* m_statusLineBreaks = nullptr; ///< Unix (LF), Windows (CRLF), Mac (CR), Mixed
    QLabel* m_statusEncoding = nullptr;   ///< UTF-8 or the code page
    QLabel* m_statusLanguage = nullptr;   ///< detected in the background
    QLabel* m_statusTab = nullptr;        ///< tab width in spaces
    void updateStatusBar();
    qce::kate::KateDataDownloader* m_downloader = nullptr;
    bool m_syntaxDownloadOffered = false;

    /// Adds a new editor as a tab and applies the frame's view options to it.
    void addEditorTab(Editor* editor, const QString& tabTitle, const QString& popupText);
    qce::kate::KateDataDownloader* downloader();
    void offerSyntaxDownload();

    void updateWindowTitle();
    void viewFileInEditor(const QString& filePath);
    int findTabByPath(const QString& filePath);
    QString generateUniqueTabTitle(const QString& filePath);
    void refreshTabTitleOnPathChange(Editor* editor);
    void createActions();
    void saveGeometryToConfig();

    void mruAdd(const QString& filePath);
    void mruRemove(const QString& filePath);
    void mruRebuildMenu();

    QMenu* m_recentMenu = nullptr;

    bool m_geometryRestored = false;
    QTimer* m_fileWatchTimer = nullptr;
    // Tab that tabLimitReached decided to close; tabAboutToClose does not ask about it again
    QPointer<QWidget> m_limitClosePage;
};
#endif // EDITORFRAME_H
