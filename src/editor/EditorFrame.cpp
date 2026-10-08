/**
* @brief Constructs main application window
 * @param parent Parent widget
 *
 * Initializes all UI components and builds initial layout
 */

#include <QApplication>
#include <QString>
#include <QDebug>
#include <QtGlobal>
#include <qxfiledialog.h>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include "../quitls.h"
#include <QInputDialog>
#include <QShortcut>
#include <QClipboard>
#include <QShowEvent>
#include <chrono>
#include <ctime>
#include "../Config.h"

#include <qguiapplication.h>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QLabel>
#include <QHash>
#include <qce/CodeEdit.h>
#include <qce/CodeEditArea.h>
#include <qce/encoding/Encoding.h>
#include <qce/kate/KateDataDownloader.h>
#include <qce/kate/KatePaths.h>

#include "EditorFrame.h"
#include "editor.h"
#include "SyntaxData.h"
#include <mrutabwidget.h>
#include "keys/ObjectRegistry.h"

EditorFrame::EditorFrame(QWidget* parent)
    : QMainWindow(parent)
{
    auto* central = new QWidget(this);
    m_mainLayout = new QVBoxLayout(central);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // --- Main Header (Menu + Toolbar) ---
    m_mainHeader = new MainHeader(this);
    m_mainLayout->addWidget(m_mainHeader);

    // --- Editor Tab Widget ---
    m_editorTabWidget = new MruTabWidget(this);
    m_editorTabWidget->setTabLimit(16);
    m_editorTabWidget->setTabsClosable(true);
    m_editorTabWidget->setMovable(true);
    m_editorTabWidget->setUsesScrollButtons(true);
    connect(m_editorTabWidget, &MruTabWidget::tabClosing,
            this, &EditorFrame::actionsBeforeTabClose);
    // tabAboutToClose writes to allow_close, so it needs a direct connection
    connect(m_editorTabWidget, &MruTabWidget::tabAboutToClose,
            this, &EditorFrame::tabAboutToClose, Qt::DirectConnection);
    // Asked before a new tab is added (makeRoomForNewTab); writes to action
    connect(m_editorTabWidget, &MruTabWidget::tabLimitReached,
            this, &EditorFrame::tabLimitReached, Qt::DirectConnection);
    connect(m_editorTabWidget, &MruTabWidget::tabContextMenuRequested,
            this, &EditorFrame::extendTabContextMenu);
    connect(m_editorTabWidget, &MruTabWidget::currentChanged, this, [this](int) {
        updateWindowTitle();
        updateStatusBar();
    });

    createActions();
    QMenu* fileMenu = m_mainHeader->setupMenus(m_newFileAction, m_openFileAction,
                             m_saveAction, m_saveAsAction, m_closeAction,
                             m_exitAction, m_showSpecialCharsAction, m_wrapLinesAction,
                             m_aboutAction, m_findAction, m_findNextAction, m_findPrevAction,
                             m_replaceAction, m_gotoAction);
    m_mainHeader->setupToolsMenu(m_insertDateAction, m_insertTimeAction, m_insertBothAction,
                                 m_updateSyntaxAction);

    // Insert "Recent Files" submenu before the separator that precedes Close
    m_recentMenu = new QMenu(tr("Recent Files"), fileMenu);
    // Insert it after Open (index 1), before the first separator
    QList<QAction*> fileActions = fileMenu->actions();
    QAction* insertBefore = fileActions.size() > 2 ? fileActions[2] : nullptr;
    fileMenu->insertMenu(insertBefore, m_recentMenu);
    mruRebuildMenu();

    m_mainLayout->addWidget(m_editorTabWidget);
    setCentralWidget(central);
    m_statusBar = statusBar();
    // Status bar: line:column | line breaks | encoding | language | tab width.
    for (QLabel** label : {&m_statusPos, &m_statusLineBreaks, &m_statusEncoding,
                           &m_statusLanguage, &m_statusTab}) {
        *label = new QLabel(this);
        m_statusBar->addPermanentWidget(*label);
    }
    updateStatusBar();

    // --- Finish setup ---
    setMinimumSize(600, 400);

    m_fileWatchTimer = new QTimer(this);
    m_fileWatchTimer->setInterval(1000);
    connect(m_fileWatchTimer, &QTimer::timeout, this, [this]() {
        if (isActiveWindow())
            checkAllTabsForExternalChanges();
    });
    m_fileWatchTimer->start();
}

void EditorFrame::extendTabContextMenu(QWidget* page, QMenu* menu) {
    menu->addSeparator();
    QAction* closeUnmodifiedAction = menu->addAction(tr("Close Unmodified Tabs"));
    connect(closeUnmodifiedAction, &QAction::triggered, [this]() {
        for (int i = m_editorTabWidget->count() - 1; i >= 0; --i)
        {
            QWidget* w = m_editorTabWidget->widget(i);
            auto* base_viewer = qobject_cast<BaseViewer*>(w);
            if (base_viewer && !base_viewer->isModified())
            {
                m_editorTabWidget->requestCloseTab(i);
            }
        }
        });
    if (auto base_viewer = qobject_cast<BaseViewer*>(page)) {
        QAction* copyPath = menu->addAction("Copy FileName");
        connect(copyPath, &QAction::triggered, [base_viewer]() {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(base_viewer->baseFileName());
        });
    }
    if (auto base_viewer = qobject_cast<BaseViewer*>(page)) {
        QAction* copyPath = menu->addAction("Copy Path");
        connect(copyPath, &QAction::triggered, [base_viewer]() {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(base_viewer->filePath());
        });
    }
}

EditorFrame::~EditorFrame() = default;

void EditorFrame::createActions()
{
    m_newFileAction = new QAction(tr("&New"), this);
    m_newFileAction->setShortcut(QKeySequence::New);
    connect(m_newFileAction, &QAction::triggered, this, &EditorFrame::onNewFileTriggered);

    m_openFileAction = new QAction(tr("&Open File..."), this);
    m_openFileAction->setShortcut(QKeySequence::Open);
    connect(m_openFileAction, &QAction::triggered, this, &EditorFrame::onOpenFileTriggered);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &EditorFrame::onSaveTriggered);

    m_saveAsAction = new QAction(tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &EditorFrame::onSaveAsTriggered);

    m_closeAction = new QAction(tr("&Close"), this);
    m_closeAction->setShortcut(QKeySequence::Close);
    connect(m_closeAction, &QAction::triggered, this, &EditorFrame::onCloseCurrentTabTriggered);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    m_aboutAction = new QAction(tr("&About"), this);
    connect(m_aboutAction, &QAction::triggered, this, &EditorFrame::onAboutTriggered);

    m_showSpecialCharsAction = new QAction(tr("Show Special Characters"), this);
    m_showSpecialCharsAction->setCheckable(true);
    m_showSpecialCharsAction->setChecked(Config::instance().editorShowWhitespace());
    connect(m_showSpecialCharsAction, &QAction::toggled, this, &EditorFrame::onToggleSpecialChars);

    m_wrapLinesAction = new QAction(tr("Wrap Lines"), this);
    m_wrapLinesAction->setCheckable(true);
    m_wrapLinesAction->setChecked(Config::instance().editorWordWrap());
    connect(m_wrapLinesAction, &QAction::toggled, this, &EditorFrame::onToggleWrapLines);

    m_findAction = new QAction(tr("&Find..."), this);
    m_findAction->setShortcut(QKeySequence::Find);
    connect(m_findAction, &QAction::triggered, this, &EditorFrame::onFindTriggered);

    m_findNextAction = new QAction(tr("Find &Next"), this);
    m_findNextAction->setShortcut(QKeySequence(Qt::Key_F3));
    connect(m_findNextAction, &QAction::triggered, this, &EditorFrame::onFindNextTriggered);

    m_findPrevAction = new QAction(tr("Find &Previous"), this);
    m_findPrevAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(m_findPrevAction, &QAction::triggered, this, &EditorFrame::onFindPrevTriggered);

    m_replaceAction = new QAction(tr("&Replace..."), this);
    m_replaceAction->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_H), QKeySequence(Qt::CTRL | Qt::Key_R)});
    connect(m_replaceAction, &QAction::triggered, this, &EditorFrame::onReplaceTriggered);

    m_gotoAction = new QAction(tr("&Go to Line/Column..."), this);
    m_gotoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_gotoAction, &QAction::triggered, this, &EditorFrame::onGotoTriggered);

    m_insertDateAction = new QAction(tr("Insert &Date"), this);
    connect(m_insertDateAction, &QAction::triggered, this, &EditorFrame::onInsertDateTriggered);

    m_insertTimeAction = new QAction(tr("Insert &Time"), this);
    connect(m_insertTimeAction, &QAction::triggered, this, &EditorFrame::onInsertTimeTriggered);

    m_insertBothAction = new QAction(tr("Insert &Both Date and Time"), this);
    connect(m_insertBothAction, &QAction::triggered, this, &EditorFrame::onInsertBothTriggered);

    m_updateSyntaxAction = new QAction(tr("&Update Syntax Definitions"), this);
    connect(m_updateSyntaxAction, &QAction::triggered, this, &EditorFrame::onUpdateSyntaxTriggered);
}

/**
 * @brief Opens file in editor tab
 * @param filePath Full path to file
 *
 * Reuses existing tabs for already open files
 */
void EditorFrame::openFile(const QString& fileName)
{
    int existingTabIndex = findTabByPath(fileName);
    if (existingTabIndex >= 0)
    {
        m_editorTabWidget->setCurrentIndex(existingTabIndex);
        auto* editor = qobject_cast<Editor*>(m_editorTabWidget->widget(existingTabIndex));
        if (editor && !editor->isModified() && editor->isChangedOnDisk())
            editor->reloadFromDisk();
        return;
    }

    // Make room before anything is created, so that Cancel opens nothing
    if (!m_editorTabWidget->makeRoomForNewTab())
        return;

    auto* newEditor = new Editor(m_editorTabWidget);
    if (!newEditor->loadFile(fileName))
    {
        qWarning() << "Could not open file:" << fileName;
        QMessageBox::warning(this, tr("Error"), tr("Could not open document for:\n%1").arg(fileName));
        delete newEditor;
        return;
    }
    // Ctrl+Tab popup shows the last two path components (e.g. dir1/file.txt)
    addEditorTab(newEditor, generateUniqueTabTitle(newEditor->filePath()),
                 qLastPathComponents(newEditor->filePath()));
}

void EditorFrame::addEditorTab(Editor* editor, const QString& tabTitle, const QString& popupText)
{
    editor->setWordWrap(m_wrapLinesAction->isChecked());
    editor->setShowWhitespace(m_showSpecialCharsAction->isChecked());
    int finalIndex = m_editorTabWidget->addTab(editor, tabTitle);
    m_editorTabWidget->setCurrentIndex(finalIndex);
    m_editorTabWidget->setTabPopupText(finalIndex, popupText);
    refreshTabTitleOnPathChange(editor);
    connect(editor, &Editor::modificationChanged, this, [this, editor]() {
        if (editor == currentEditor())
            updateWindowTitle();
    });
    connect(editor, &Editor::statusChanged, this, [this, editor]() {
        if (editor == currentEditor())
            updateStatusBar();
    });
    connect(editor->codeEdit()->area(), &qce::CodeEditArea::cursorPositionChanged, this, [this, editor]() {
        if (editor == currentEditor())
            updateStatusBar();
    });
    updateStatusBar();
    ObjectRegistry::add(editor, "Editor");
    editor->setFocus();
}

// Status bar texts for the file format reported by qcodeedit-encoding.
static QString lineBreaksText(const qce::encoding::FileFormat& format, bool ignoreMixed = false)
{
    if (format.mixedLineBreaks && !ignoreMixed) return QObject::tr("Mixed");
    if (format.cr) return QObject::tr("Mac (CR)");
    if (format.crlf) return QObject::tr("Windows (CRLF)");
    return QObject::tr("Unix (LF)");
}

static QString encodingText(const qce::encoding::FileFormat& format)
{
    static const QHash<QString, QString> unicode = {
        {"utf8", "UTF-8"}, {"utf16", "UTF-16LE"}, {"utf16be", "UTF-16BE"},
        {"utf32", "UTF-32LE"}, {"utf32be", "UTF-32BE"}};
    const QString name = unicode.value(format.encoding.toLower(), format.encoding);
    return format.bom ? name + " BOM" : name;
}

void EditorFrame::updateStatusBar()
{
    if (!m_statusPos) return;
    const QList<QLabel*> labels = {m_statusPos, m_statusLineBreaks, m_statusEncoding,
                                   m_statusLanguage, m_statusTab};
    const Editor* editor = currentEditor();
    if (!editor) {
        for (QLabel* label : labels) label->clear();
        return;
    }
    const auto* area = editor->codeEdit()->area();
    const qce::TextCursor c = area->cursorPosition();
    m_statusPos->setText(QString("%1:%2").arg(c.line + 1).arg(c.column + 1));
    const auto format = editor->fileFormat();
    m_statusLineBreaks->setText(lineBreaksText(format));
    // A mixed file is saved with its most frequent kind of line break.
    m_statusLineBreaks->setToolTip(format.mixedLineBreaks
        ? tr("Mixed line breaks; saving uses %1").arg(lineBreaksText(format, true))
        : QString());
    m_statusEncoding->setText(encodingText(format));
    m_statusLanguage->setText(editor->languageName());
    m_statusLanguage->setToolTip(tr("Language detected from the text"));
    m_statusTab->setText(tr("Tab: %1").arg(area->tabWidth()));
}

Editor* EditorFrame::currentEditor() const
{
    if (!m_editorTabWidget)
        return nullptr;
    return qobject_cast<Editor*>(m_editorTabWidget->currentWidget());
}

void EditorFrame::newFile()
{
    onNewFileTriggered();
}

void EditorFrame::onNewFileTriggered()
{
    // Make room before anything is created, so that Cancel opens nothing
    if (!m_editorTabWidget->makeRoomForNewTab())
        return;

    auto* newEditor = new Editor(m_editorTabWidget);
    QString tabTitle = generateUniqueTabTitle(QString());
    // Untitled document has no path, so the popup just mirrors the tab title
    addEditorTab(newEditor, tabTitle, tabTitle);
}

/**
 * @brief Handles file open action
 *
 * Shows file dialog and opens selected files in editor tabs
 */
void EditorFrame::onOpenFileTriggered()
{
    QString fileName = QxFileDialog::getOpenFileName(this,
                                                     tr("Open File"),
                                                     QString(),
                                                     tr("All Files (*.*);;Text Files (*.txt);;C++ Files (*.cpp *.h)"));

    if (fileName.isEmpty())
    {
        return;
    }
    openFile(fileName);
}

void EditorFrame::onSaveTriggered()
{
    if (Editor* editor = currentEditor())
        editor->saveFile();
}

void EditorFrame::onSaveAsTriggered()
{
    if (Editor* editor = currentEditor())
        editor->saveFileAs();
}

void EditorFrame::onCloseCurrentTabTriggered()
{
    m_editorTabWidget->requestCloseTab(m_editorTabWidget-> currentIndex());
}

void EditorFrame::onAboutTriggered()
{
    QString aboutText;
    aboutText += tr("Gemini Commander editor %1").arg(QStringLiteral(APP_VERSION)) + "\n";
    aboutText += tr("Qt Version: ") + QString::fromUtf8(qVersion()) + "\n";
    aboutText += tr("Editor component: qcodeedit") + "\n";
    aboutText += tr("Syntax definitions: %1").arg(qce::kate::dataDir()) + "\n";
    QMessageBox::about(this, tr("About"), aboutText);
}

/**
 * @brief Releases an editor tab that is being closed
 * @param page Page of the tab being closed
 *
 * Adds the file to the recent list
 */
void EditorFrame::actionsBeforeTabClose(QWidget* page)
{
    Editor* editor = qobject_cast<Editor*>(page); // Cast to our Editor view
    if (!editor)
        return;
    // Add to MRU when tab is closed (only real files, not untitled)
    if (!editor->filePath().isEmpty())
        mruAdd(editor->filePath());
}

void EditorFrame::tabLimitReached(QWidget* page, MruTabWidget::LimitAction &action)
{
    auto* editor = qobject_cast<Editor*>(page);
    const bool deleted = editor && !editor->filePath().isEmpty()
                         && !QFileInfo::exists(editor->filePath());
    if (!editor || (!editor->isModified() && !deleted)) {
        // Nothing would be lost: close it without asking
        m_limitClosePage = page;
        return;
    }

    // Show the tab in question, so the user sees what is being decided about
    m_editorTabWidget->setCurrentWidget(page);
    const QString reason = editor->isModified()
        ? tr("\"%1\" has unsaved changes.").arg(editor->baseFileName())
        : tr("\"%1\" has been deleted on disk.").arg(editor->baseFileName());
    QMessageBox box(QMessageBox::Question, tr("Too Many Tabs"),
                    tr("The new tab needs room: the least recently used tab has to close.\n\n%1")
                        .arg(reason),
                    QMessageBox::NoButton, this);
    QPushButton* save = box.addButton(tr("&Save and Close"), QMessageBox::AcceptRole);
    QPushButton* discard = box.addButton(tr("Close &Without Saving"), QMessageBox::DestructiveRole);
    QPushButton* keep = box.addButton(tr("&Keep Open (Pin)"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(save);
    box.exec();

    QAbstractButton* clicked = box.clickedButton();
    if (clicked == keep) {
        action = MruTabWidget::LimitAction::Keep;
    } else if (clicked == discard) {
        m_limitClosePage = page;
    } else if (clicked == save && editor->saveFile()) {
        m_limitClosePage = page;
    } else {
        // Cancel, Escape, or saving failed or was cancelled: don't open the new tab
        action = MruTabWidget::LimitAction::Cancel;
    }
}

void EditorFrame::tabAboutToClose(QWidget* page, bool askPin, bool &allow_close)
{
    if (!allow_close)
        return;
    if (page == m_limitClosePage) {
        // Already decided in tabLimitReached
        m_limitClosePage = nullptr;
        allow_close = true;
        return;
    }
    auto* editor = qobject_cast<Editor*>(page);
    if (editor && editor->isModified())
    {
        QString message;
        if (askPin) {
            message = "TabWidget has set tab limit and must be closed least used unpinned tab.\n\n";
            message += "The text in the file " + editor->baseFileName() + " has been changed.\n";
            message += "Do you want to save the modifications? (No = close and discard changes, Cancel = pin tab)";
        } else {
            message = "The text in the file " + editor->baseFileName() + " has been changed.\n";
            message += "Do you want to save the modifications? (No = close and discard changes)";
        }

        QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    tr("Unsaved Changes"),
                    message,
                    QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel
                );
        switch (reply) {
            case QMessageBox::Cancel:allow_close = false; break;
            case QMessageBox::No:allow_close = true; break;
            case QMessageBox::Yes:
                allow_close = editor->saveFile(); break;
            default:allow_close = true;
        }
    } else if (editor && !editor->filePath().isEmpty() && !QFileInfo::exists(editor->filePath())) {
        // Unmodified, but the file has been deleted on disk while open. Don't lose
        // the buffer silently - let the user save it back or discard it.
        QString message = "The file " + editor->baseFileName() + " has been deleted on disk.\n";
        message += "Do you want to save it back? (No = close and discard)";
        QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    tr("File Deleted"),
                    message,
                    QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel
                );
        switch (reply) {
            case QMessageBox::Cancel:allow_close = false; break;
            case QMessageBox::No:allow_close = true; break;
            case QMessageBox::Yes:
                allow_close = editor->saveFile(); break;
            default:allow_close = true;
        }
    } else
        allow_close = true;
}

// Close all tabs helper (no change needed in logic, uses updated closeTab)
// findTabByPath (MODIFIED to check Editor's document URL)
int EditorFrame::findTabByPath(const QString& filePath)
{
    for (int i = 0; i < m_editorTabWidget->count(); ++i)
    {
        QWidget* widget = m_editorTabWidget->widget(i);
        auto editor = qobject_cast<BaseViewer*>(widget);
        if (editor && editor->filePath() == filePath)
        {
            return i;
        }
    }
    return -1;
}

// Keep an editor tab's title and Ctrl+Tab popup in sync with the document
// path, which changes on Save As.
void EditorFrame::updateWindowTitle()
{
    Editor* editor = currentEditor();
    if (!editor) {
        setWindowTitle(QString());
        return;
    }
    const QString path = editor->filePath();
    QString title = path.isEmpty() ? m_editorTabWidget->tabText(m_editorTabWidget->currentIndex())
                                   : path;
    if (editor->isModified())
        title += QStringLiteral(" *");
    setWindowTitle(title);
}

void EditorFrame::refreshTabTitleOnPathChange(Editor* editor)
{
    connect(editor, &Editor::filePathChanged, this, [this, editor]() {
        int idx = m_editorTabWidget->indexOf(editor);
        if (idx < 0)
            return;
        const QString path = editor->filePath();
        m_editorTabWidget->setTabText(idx, generateUniqueTabTitle(path));
        m_editorTabWidget->setTabPopupText(
            idx, path.isEmpty() ? m_editorTabWidget->tabText(idx)
                                : qLastPathComponents(path));
        if (m_editorTabWidget->currentIndex() == idx)
            updateWindowTitle();
    });
}

// generateUniqueTabTitle (MODIFIED to check Editor's document URL/path)
QString EditorFrame::generateUniqueTabTitle(const QString& filePath)
{
    // For empty path (new untitled document), generate Untitled-1, Untitled-2, etc.
    if (filePath.isEmpty()) {
        int counter = 1;
        QString candidate;
        while (true) {
            candidate = QString("Untitled-%1").arg(counter);
            bool found = false;
            for (int i = 0; i < m_editorTabWidget->count(); ++i) {
                if (m_editorTabWidget->tabText(i) == candidate) {
                    found = true;
                    break;
                }
            }
            if (!found)
                return candidate;
            counter++;
        }
    }

    QString baseName = QFileInfo(filePath).fileName();
    QString uniqueTitle = baseName;
    int counter = 1;
    bool titleIsUnique = false;

    while (!titleIsUnique)
    {
        titleIsUnique = true;
        for (int i = 0; i < m_editorTabWidget->count(); ++i)
        {
            QWidget* widget = m_editorTabWidget->widget(i);
            auto base_viewer = qobject_cast<BaseViewer*>(widget);
            // Check editor, document, and if the current tab text matches the candidate title
            if (base_viewer && m_editorTabWidget->tabText(i) == uniqueTitle)
            {
                // Title matches, check if it's for a DIFFERENT file path
                if (base_viewer->filePath() != filePath)
                {
                    titleIsUnique = false;
                    uniqueTitle = QString("%1 (%2)").arg(baseName).arg(counter++);
                    break; // Re-check the new candidate title
                }
            }
        }
    }
    return uniqueTitle;
}

void EditorFrame::onToggleSpecialChars(bool checked)
{
    for (int i = 0; i < m_editorTabWidget->count(); ++i) {
        if (Editor* editor = qobject_cast<Editor*>(m_editorTabWidget->widget(i)))
            editor->setShowWhitespace(checked);
    }
    Config::instance().setEditorShowWhitespace(checked);
    Config::instance().save();
}

void EditorFrame::onToggleWrapLines(bool checked)
{
    for (int i = 0; i < m_editorTabWidget->count(); ++i) {
        if (Editor* editor = qobject_cast<Editor*>(m_editorTabWidget->widget(i)))
            editor->setWordWrap(checked);
    }
    Config::instance().setEditorWordWrap(checked);
    Config::instance().save();
}

void EditorFrame::mruAdd(const QString& filePath)
{
    if (filePath.isEmpty())
        return;
    Config& cfg = Config::instance();
    QStringList paths = cfg.editorMruPaths();
    paths.removeAll(filePath);
    paths.prepend(filePath);
    while (paths.size() > cfg.editorMruMaxCount())
        paths.removeLast();
    cfg.setEditorMruPaths(paths);
    cfg.save();
    mruRebuildMenu();
}

void EditorFrame::mruRemove(const QString& filePath)
{
    if (filePath.isEmpty())
        return;
    Config& cfg = Config::instance();
    QStringList paths = cfg.editorMruPaths();
    if (!paths.removeAll(filePath))
        return;
    cfg.setEditorMruPaths(paths);
    cfg.save();
    mruRebuildMenu();
}

void EditorFrame::mruRebuildMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    const QStringList paths = Config::instance().editorMruPaths();
    if (paths.isEmpty()) {
        QAction* empty = m_recentMenu->addAction(tr("(empty)"));
        empty->setEnabled(false);
        return;
    }
    for (const QString& path : paths) {
        QAction* act = m_recentMenu->addAction(path);
        connect(act, &QAction::triggered, this, [this, path]() {
            mruRemove(path);
            openFile(path);
        });
    }
}

bool EditorFrame::event(QEvent* event)
{
    const bool handled = QMainWindow::event(event);
    if (event->type() == QEvent::WindowActivate)
        checkAllTabsForExternalChanges();
    return handled;
}

void EditorFrame::checkAllTabsForExternalChanges()
{
    // Only react to *changes* on disk: silently reload unmodified tabs whose file
    // changed. A deleted file is left untouched - it might be a still-unsaved name
    // (e.g. opened via Shift+F4 for a path that never existed), and the deletion is
    // only acted upon when the tab is closed (see tabAboutToClose).
    for (int i = m_editorTabWidget->count() - 1; i >= 0; --i) {
        auto* editor = qobject_cast<Editor*>(m_editorTabWidget->widget(i));
        if (!editor || editor->filePath().isEmpty())
            continue;

        QFileInfo fi(editor->filePath());
        if (fi.exists() && !editor->isModified() && editor->isChangedOnDisk())
            editor->reloadFromDisk();
    }
}

void EditorFrame::closeEvent(QCloseEvent* event)
{
    if (!m_editorTabWidget->requestCloseAllTabs()) {
        event->ignore();
    } else {
        saveGeometryToConfig();
        event->accept();
    }
}

void EditorFrame::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    if (!m_geometryRestored) {
        m_geometryRestored = true;

        Config& cfg = Config::instance();
        int w = cfg.editorWidth();
        int h = cfg.editorHeight();
        int relX = cfg.editorX();
        int relY = cfg.editorY();

        resize(w, h);

        // Position relative to parent window
        if (parentWidget()) {
            QPoint parentPos = parentWidget()->pos();
            move(parentPos.x() + relX, parentPos.y() + relY);
        }
        // After the window is shown: offer the first download if needed.
        QTimer::singleShot(0, this, &EditorFrame::offerSyntaxDownload);
    }
}

void EditorFrame::saveGeometryToConfig()
{
    Config& cfg = Config::instance();

    int relX = 0;
    int relY = 0;

    // Calculate position relative to parent window
    if (parentWidget()) {
        QPoint parentPos = parentWidget()->pos();
        relX = x() - parentPos.x();
        relY = y() - parentPos.y();
    }

    cfg.setEditorGeometry(relX, relY, width(), height());
    cfg.save();
}

void EditorFrame::onFindTriggered()
{
    if (Editor* editor = currentEditor())
        editor->showSearch();
}

void EditorFrame::onFindNextTriggered()
{
    if (Editor* editor = currentEditor())
        editor->findNext();
}

void EditorFrame::onFindPrevTriggered()
{
    if (Editor* editor = currentEditor())
        editor->findNext(true);
}

void EditorFrame::onReplaceTriggered()
{
    if (Editor* editor = currentEditor())
        editor->showReplace();
}

void EditorFrame::onGotoTriggered()
{
    if (Editor* editor = currentEditor())
        editor->goToPosition();
}

qce::kate::KateDataDownloader* EditorFrame::downloader()
{
    if (m_downloader)
        return m_downloader;

    m_downloader = new qce::kate::KateDataDownloader(this);
    connect(m_downloader, &qce::kate::KateDataDownloader::progress,
            this, [this](int done, int total) {
        m_statusBar->showMessage(tr("Downloading syntax definitions: %1/%2").arg(done).arg(total));
    });
    connect(m_downloader, &qce::kate::KateDataDownloader::finished,
            this, [this](bool ok, int downloaded, int failed) {
        SyntaxData::reload();
        for (int i = 0; i < m_editorTabWidget->count(); ++i)
            if (Editor* editor = qobject_cast<Editor*>(m_editorTabWidget->widget(i)))
                editor->reapplyHighlighter();
        m_statusBar->showMessage(
            ok ? tr("Syntax definitions up to date (%1 files downloaded)").arg(downloaded)
               : tr("Syntax definitions incomplete: %1 downloaded, %2 failed")
                     .arg(downloaded).arg(failed),
            8000);
    });
    return m_downloader;
}

// Ask once when the editor is first shown and no (complete) data set is
// present. A "No" is remembered; Tools > Update Syntax Definitions stays available.
void EditorFrame::offerSyntaxDownload()
{
    if (m_syntaxDownloadOffered)
        return;
    m_syntaxDownloadOffered = true;
    Config& cfg = Config::instance();
    if (cfg.syntaxDownloadDeclined() || !downloader()->mustDownload())
        return;

    const auto answer = QMessageBox::question(
        this, tr("Syntax Definitions"),
        tr("Download Kate syntax definitions %1 and color themes from "
           "kate-editor.org and invent.kde.org?\n\nThey will be stored in:\n%2")
            .arg(qce::kate::supportedSyntaxVersion().toString(),
                 downloader()->dataDir()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        cfg.setSyntaxDownloadDeclined(true);
        cfg.save();
        return;
    }
    onUpdateSyntaxTriggered();
}

void EditorFrame::onUpdateSyntaxTriggered()
{
    Config& cfg = Config::instance();
    if (cfg.syntaxDownloadDeclined()) {
        cfg.setSyntaxDownloadDeclined(false);
        cfg.save();
    }
    if (downloader()->busy())
        return;
    m_statusBar->showMessage(tr("Downloading syntax definitions…"));
    downloader()->start();
}

static const char* daysOfWeek[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

void EditorFrame::onInsertDateTriggered()
{
    Editor* editor = currentEditor();
    if (!editor)
        return;

    auto nowChrono = std::chrono::system_clock::now();
    time_t now_time_t = std::chrono::system_clock::to_time_t(nowChrono);
    struct tm* now_tm = localtime(&now_time_t);
    char buf[64];
    sprintf(buf, "%s %d-%.2d-%.2d",
            daysOfWeek[now_tm->tm_wday],
            now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday);

    editor->insertAtCursor(QString::fromUtf8(buf));
}

void EditorFrame::onInsertTimeTriggered()
{
    Editor* editor = currentEditor();
    if (!editor)
        return;

    auto nowChrono = std::chrono::system_clock::now();
    time_t now_time_t = std::chrono::system_clock::to_time_t(nowChrono);
    struct tm* now_tm = localtime(&now_time_t);
    char buf[64];
    sprintf(buf, "%.2d:%.2d:%.2d",
            now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);

    editor->insertAtCursor(QString::fromUtf8(buf));
}

void EditorFrame::onInsertBothTriggered()
{
    Editor* editor = currentEditor();
    if (!editor)
        return;

    auto nowChrono = std::chrono::system_clock::now();
    time_t now_time_t = std::chrono::system_clock::to_time_t(nowChrono);
    struct tm* now_tm = localtime(&now_time_t);
    char buf[128];
    sprintf(buf, "%s %d-%.2d-%.2d %.2d:%.2d:%.2d",
            daysOfWeek[now_tm->tm_wday],
            now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday,
            now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);

    editor->insertAtCursor(QString::fromUtf8(buf));
}

#include  "EditorFrame_impl.inc"
