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
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QShortcut>
#include <QClipboard>
#include <QShowEvent>
#include <chrono>
#include <ctime>
#include "../Config.h"

// KTextEditor Includes
#include <qguiapplication.h>
#include <QVBoxLayout>
#include <KTextEditor/Editor>       // The Editor singleton
#include <KTextEditor/Document>     // Document representation
#include <KTextEditor/View>         // Base view class (used for log)
#include <KTextEditor/MainWindow>   // Often needed for full integration/context
#include <KTextEditor/Application>

#include "EditorFrame.h"
#include "editor.h"
#include "../widgets/mrutabwidget.h"
#include "keys/ObjectRegistry.h"

// Helper function to get KTextEditor::EditorFrame (can be nullptr if not embedded)
// This might be needed for some KTextEditor features. For now, we create a dummy one
// if needed, or pass nullptr where possible. A better approach might involve KParts::Part.
KTextEditor::MainWindow* getDummyKateMainWindow()
{
    // This is a HACK - ideally we'd have a proper KTextEditor::EditorFrame
    // or integrate via KParts::Part. Returning nullptr might work for some things.
    // static KTextEditor::EditorFrame dummyKateWin;
    // return &dummyKateWin;
    return nullptr; // Try with nullptr first
}


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
    connect(m_editorTabWidget, &MruTabWidget::actionsBeforeTabClose,
            this, &EditorFrame::actionsBeforeTabClose);
    connect(m_editorTabWidget, &MruTabWidget::tabAboutToClose,
            this, &EditorFrame::tabAboutToClose);
    connect(m_editorTabWidget, &MruTabWidget::tabContextMenuRequested,
            this, &EditorFrame::extendTabContextMenu);

    createActions();
    m_mainHeader->setupMenus(m_openFileAction, m_closeAction,
                             m_exitAction, m_showSpecialCharsAction, m_wrapLinesAction,
                             m_aboutAction, m_findAction, m_findNextAction, m_findPrevAction,
                             m_replaceAction, m_gotoAction);
    m_mainHeader->setupToolsMenu(m_insertDateAction, m_insertTimeAction, m_insertBothAction);

    m_mainLayout->addWidget(m_editorTabWidget);
    setCentralWidget(central);

    // --- Finish setup ---
    setMinimumSize(600, 400);
}

void EditorFrame::extendTabContextMenu(int tabIndex, QMenu* menu) {
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
    QWidget* tabContent = m_editorTabWidget->widget(tabIndex);
    if (auto base_viewer = qobject_cast<BaseViewer*>(tabContent)) {
        QAction* copyPath = menu->addAction("Copy FileName");
        connect(copyPath, &QAction::triggered, [base_viewer]() {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(base_viewer->baseFileName());
        });
    }
    if (auto base_viewer = qobject_cast<BaseViewer*>(tabContent)) {
        QAction* copyPath = menu->addAction("Copy Path");
        connect(copyPath, &QAction::triggered, [base_viewer]() {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(base_viewer->filePath());
        });
    }
}

EditorFrame::~EditorFrame()
{
    // Destructor - Qt parent/child should handle widget deletion.
    // KTextEditor documents might need explicit closing if not managed by KParts
    // KTextEditor::Editor::instance()->closeDocuments(); // Maybe? Check docs.
}

void EditorFrame::createActions()
{
    m_openFileAction = new QAction(tr("&Open File..."), this);
    m_openFileAction->setShortcut(QKeySequence::Open);
    connect(m_openFileAction, &QAction::triggered, this, &EditorFrame::onOpenFileTriggered);

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
    m_showSpecialCharsAction->setChecked(false);
    connect(m_showSpecialCharsAction, &QAction::toggled, this, &EditorFrame::onToggleSpecialChars);

    m_wrapLinesAction = new QAction(tr("Wrap Lines"), this);
    m_wrapLinesAction->setCheckable(true);
    m_wrapLinesAction->setChecked(false);
    connect(m_wrapLinesAction, &QAction::toggled, this, &EditorFrame::onToggleWrapLines);

    // Note: KTextEditor already handles Ctrl+F, F3, Shift+F3 internally
    // We just add menu entries that trigger the same actions
    m_findAction = new QAction(tr("&Find...\tCtrl+F"), this);
    connect(m_findAction, &QAction::triggered, this, &EditorFrame::onFindTriggered);

    m_findNextAction = new QAction(tr("Find &Next\tF3"), this);
    connect(m_findNextAction, &QAction::triggered, this, &EditorFrame::onFindNextTriggered);

    m_findPrevAction = new QAction(tr("Find &Previous\tShift+F3"), this);
    connect(m_findPrevAction, &QAction::triggered, this, &EditorFrame::onFindPrevTriggered);

    m_replaceAction = new QAction(tr("&Replace...\tCtrl+H"), this);
    connect(m_replaceAction, &QAction::triggered, this, &EditorFrame::onReplaceTriggered);

    m_gotoAction = new QAction(tr("&Goto..."), this);
    m_gotoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_gotoAction, &QAction::triggered, this, &EditorFrame::onGotoTriggered);

    m_insertDateAction = new QAction(tr("Insert &Date"), this);
    connect(m_insertDateAction, &QAction::triggered, this, &EditorFrame::onInsertDateTriggered);

    m_insertTimeAction = new QAction(tr("Insert &Time"), this);
    connect(m_insertTimeAction, &QAction::triggered, this, &EditorFrame::onInsertTimeTriggered);

    m_insertBothAction = new QAction(tr("Insert &Both Date and Time"), this);
    connect(m_insertBothAction, &QAction::triggered, this, &EditorFrame::onInsertBothTriggered);
}

/**
 * @brief Opens file in editor tab
 * @param filePath Full path to file
 *
 * Reuses existing tabs for already open files
 */
void EditorFrame::openFile(const QString& fileName)
{
    KTextEditor::Editor* kate = KTextEditor::Editor::instance(); // Singleton KTE Editor
    KTextEditor::Application* app = kate->application();
    if (!app)
    {
        // This should not happen, but security
        qWarning() << "Could not get KTextEditor::Application interface from KTextEditor::Editor!";
        return;; // Go to next file, cannot continue
    }

    QUrl fileUrl = QUrl::fromLocalFile(fileName);

    int existingTabIndex = findTabByPath(fileName);
    if (existingTabIndex >= 0)
    {
        m_editorTabWidget->setCurrentIndex(existingTabIndex);
        qDebug() << "Activated existing tab for:" << fileName;
        return;
    }

    KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
    if (doc)
    {
        if (!doc->openUrl(fileUrl))
        {
            qWarning() << "Manual openUrl on Document failed for:" << fileUrl;
            QMessageBox::warning(this, tr("Error"), tr("Could not open document for:\n%1").arg(fileName));
            delete doc; // Clean up
            return;
        }
    }
    else
    {
        qWarning() << "Failed to create KTextEditor::Document manually!";
        QMessageBox::warning(this, tr("Error"), tr("Could not create document for:\n%1").arg(fileName));
        return;
    }
    Editor* newEditor = new Editor(doc, m_editorTabWidget);
    QString tabTitle = generateUniqueTabTitle(newEditor->filePath());
    int newIndex = m_editorTabWidget->addTab(newEditor, tabTitle);
    int removedCount = m_editorTabWidget->enforceTabLimit();
    m_editorTabWidget->setCurrentIndex(newIndex - removedCount);
    auto view = qobject_cast<Editor*>(m_editorTabWidget->currentWidget())->view();
    ObjectRegistry::add(view, "Editor");

    // Disable KTextEditor's built-in Ctrl+G (go_goto_line) so we can use our own Goto dialog
    if (QAction* gotoLineAction = view->action("go_goto_line")) {
        gotoLineAction->setShortcut(QKeySequence());
    }

    // Sync menu checkboxes with KTextEditor settings on first file open
    if (m_editorTabWidget->count() == 1) {
        if (QAction* wrapAction = view->action("view_dynamic_word_wrap")) {
            m_wrapLinesAction->setChecked(wrapAction->isChecked());
        }
        if (QAction* wsAction = view->action("view_show_whitespaces")) {
            m_showSpecialCharsAction->setChecked(wsAction->isChecked());
        }
    }

    view->setFocus();
}

Editor* EditorFrame::currentEditor() const
{
    if (!m_editorTabWidget)
        return nullptr;
    return qobject_cast<Editor*>(m_editorTabWidget->currentWidget());
}

/**
 * @brief Handles file open action
 *
 * Shows file dialog and opens selected files in editor tabs
 */
void EditorFrame::onOpenFileTriggered()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
                                                          tr("Open File(s)"),
                                                          QString(),
                                                          tr(
                                                              "All Files (*.*);;Text Files (*.txt);;C++ Files (*.cpp *.h)"),
                                                          nullptr,
                                                          QFileDialog::DontUseNativeDialog);

    if (fileNames.isEmpty())
    {
        return;
    }
    for (const QString& fileName : fileNames)
        openFile(fileName);
}

void EditorFrame::onCloseCurrentTabTriggered()
{
    m_editorTabWidget->requestCloseTab(m_editorTabWidget-> currentIndex());
}

void EditorFrame::onAboutTriggered()
{
    QString aboutText;
    aboutText += tr("Qt Version: ") + QString::fromUtf8(QT_VERSION_STR) + "\n";
    QString frameworksVersion;
#if (QT_VERSION_MAJOR >= 6)
    frameworksVersion = QStringLiteral("6.x (KF6 assumed)");
#else
    frameworksVersion = QStringLiteral("5.x (KF5 assumed)");
#endif
    aboutText += tr("KDE Frameworks Version: ") + frameworksVersion + "\n";

    // Program specific info
#ifdef VERSION
    QString version = QStringLiteral(VERSION);
#else
    QString version = QStringLiteral("unknown");
#endif
    QMessageBox::about(this, tr("About"), aboutText);
}

/**
 * @brief Closes editor tab
 * @param index Tab index to close
 * @return true if closed successfully
 *
 * Handles document saving and KTextEditor integration
 */
bool EditorFrame::actionsBeforeTabClose(int index)
{
    QWidget* widget = m_editorTabWidget->widget(index);
    Editor* editor = qobject_cast<Editor*>(widget); // Cast to our Editor view
    if (!editor)
        return true;
    if (editor && editor->document())
    {
        KTextEditor::Document* doc = editor->document(); // Pobierz wskaźnik do dokumentu
        // Zamknij dokument przez interfejs Application <<< POPRAWKA
        KTextEditor::Editor* kate = KTextEditor::Editor::instance();
        KTextEditor::Application* app = kate->application();
        bool closedDocOk = false;

        if (app && doc)
        {
            // Użyj app->closeDocument(doc) zamiast błędnego kate->closeUrl() <<< POPRAWKA
            closedDocOk = app->closeDocument(doc);
            qDebug() << "KTextEditor::Application::closeDocument(" << doc->url().toLocalFile() << ") returned:" <<
                closedDocOk;
        }
        else
        {
            qWarning() << "Could not get Application or Document pointer to close document via KTE.";
        }
    }
    else if (widget)
    {
        qDebug() << "Closing non-Editor tab at index:" << index;
    }
    return true;
}

void EditorFrame::tabAboutToClose(int index, bool askPin, bool &allow_close)
{
    if (!allow_close)
        return;
    QWidget* w = m_editorTabWidget->widget(index);
    auto* editor = qobject_cast<Editor*>(w);
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

// generateUniqueTabTitle (MODIFIED to check Editor's document URL/path)
QString EditorFrame::generateUniqueTabTitle(const QString& filePath)
{
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
    // Iterate through all open editor tabs and toggle whitespace display
    for (int i = 0; i < m_editorTabWidget->count(); ++i) {
        Editor* editor = qobject_cast<Editor*>(m_editorTabWidget->widget(i));
        if (editor && editor->view()) {
            // Use the built-in KTextEditor action for showing whitespace
            QAction* wsAction = editor->view()->action("view_show_whitespaces");
            if (wsAction && wsAction->isChecked() != checked) {
                wsAction->trigger();
            }
        }
    }
}

void EditorFrame::onToggleWrapLines(bool checked)
{
    // Iterate through all open editor tabs and toggle word wrap
    for (int i = 0; i < m_editorTabWidget->count(); ++i) {
        Editor* editor = qobject_cast<Editor*>(m_editorTabWidget->widget(i));
        if (editor && editor->view()) {
            // Use the built-in KTextEditor action for dynamic word wrap
            QAction* wrapAction = editor->view()->action("view_dynamic_word_wrap");
            if (wrapAction && wrapAction->isChecked() != checked) {
                wrapAction->trigger();
            }
        }
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
    Editor* editor = currentEditor();
    if (!editor || !editor->view())
        return;

    // Use KTextEditor's built-in search bar (appears at bottom of view)
    QAction* findAction = editor->view()->action("edit_find");
    if (findAction)
        findAction->trigger();
}

void EditorFrame::onFindNextTriggered()
{
    Editor* editor = currentEditor();
    if (!editor || !editor->view())
        return;

    QAction* findNextAction = editor->view()->action("edit_find_next");
    if (findNextAction)
        findNextAction->trigger();
}

void EditorFrame::onFindPrevTriggered()
{
    Editor* editor = currentEditor();
    if (!editor || !editor->view())
        return;

    QAction* findPrevAction = editor->view()->action("edit_find_prev");
    if (findPrevAction)
        findPrevAction->trigger();
}

void EditorFrame::onReplaceTriggered()
{
    Editor* editor = currentEditor();
    if (!editor || !editor->view())
        return;

    QAction* replaceAction = editor->view()->action("edit_replace");
    if (replaceAction)
        replaceAction->trigger();
}

void EditorFrame::onGotoTriggered()
{
    Editor* editor = currentEditor();
    if (!editor || !editor->view())
        return;

    bool ok;
    QString text = QInputDialog::getText(this, tr("Goto Line"),
                                         tr("Line[:Column] (1-based):"),
                                         QLineEdit::Normal,
                                         "1:1", &ok);
    if (!ok || text.isEmpty())
        return;

    // Parse line:column format
    int line = 1;
    int column = 1;

    QStringList parts = text.split(':');
    if (!parts.isEmpty()) {
        bool lineOk;
        int parsedLine = parts[0].trimmed().toInt(&lineOk);
        if (lineOk && parsedLine >= 1)
            line = parsedLine;

        if (parts.size() > 1) {
            bool colOk;
            int parsedCol = parts[1].trimmed().toInt(&colOk);
            if (colOk && parsedCol >= 1)
                column = parsedCol;
        }
    }

    // KTextEditor uses 0-based line and column
    KTextEditor::Cursor cursor(line - 1, column - 1);
    editor->view()->setCursorPosition(cursor);
    editor->view()->setFocus();
}

static const char* daysOfWeek[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

void EditorFrame::onInsertDateTriggered()
{
    Editor* editor = currentEditor();
    if (!editor || !editor->view() || !editor->document())
        return;

    auto nowChrono = std::chrono::system_clock::now();
    time_t now_time_t = std::chrono::system_clock::to_time_t(nowChrono);
    struct tm* now_tm = localtime(&now_time_t);
    char buf[64];
    sprintf(buf, "%s %d-%.2d-%.2d",
            daysOfWeek[now_tm->tm_wday],
            now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday);

    KTextEditor::Cursor cursor = editor->view()->cursorPosition();
    editor->document()->insertText(cursor, QString::fromUtf8(buf));
}

void EditorFrame::onInsertTimeTriggered()
{
    Editor* editor = currentEditor();
    if (!editor || !editor->view() || !editor->document())
        return;

    auto nowChrono = std::chrono::system_clock::now();
    time_t now_time_t = std::chrono::system_clock::to_time_t(nowChrono);
    struct tm* now_tm = localtime(&now_time_t);
    char buf[64];
    sprintf(buf, "%.2d:%.2d:%.2d",
            now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);

    KTextEditor::Cursor cursor = editor->view()->cursorPosition();
    editor->document()->insertText(cursor, QString::fromUtf8(buf));
}

void EditorFrame::onInsertBothTriggered()
{
    Editor* editor = currentEditor();
    if (!editor || !editor->view() || !editor->document())
        return;

    auto nowChrono = std::chrono::system_clock::now();
    time_t now_time_t = std::chrono::system_clock::to_time_t(nowChrono);
    struct tm* now_tm = localtime(&now_time_t);
    char buf[128];
    sprintf(buf, "%s %d-%.2d-%.2d %.2d:%.2d:%.2d",
            daysOfWeek[now_tm->tm_wday],
            now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday,
            now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);

    KTextEditor::Cursor cursor = editor->view()->cursorPosition();
    editor->document()->insertText(cursor, QString::fromUtf8(buf));
}

#include  "EditorFrame_impl.inc"