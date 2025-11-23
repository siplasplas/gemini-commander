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
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QTreeView>
#include <QGraphicsDropShadowEffect>
#include <QShortcut>
#include <QStandardItemModel>
#include <QClipboard>

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
#include "Viewer.h"
#include "../widgets/mrutabwidget.h"

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

    // --- Main Splitter for content ---
    m_mainSplitter = new QSplitter(Qt::Vertical, this);

    m_topSplitter = new QSplitter(Qt::Horizontal, m_mainSplitter);

    m_projectTree = new QTreeView(m_topSplitter);
    connect(m_projectTree, &QTreeView::doubleClicked,
            this, &EditorFrame::onProjectTreeActivated);

    m_editorTabWidget = new MruTabWidget(m_topSplitter);
    m_editorTabWidget->setTabLimit(3); // Small value for tests
    m_editorTabWidget->setTabsClosable(true);
    m_editorTabWidget->setMovable(true);
    m_editorTabWidget->setUsesScrollButtons(true);
    connect(m_editorTabWidget, &MruTabWidget::cleanupBeforeTabClose,
            this, &EditorFrame::cleanupBeforeTabClose);
    connect(m_editorTabWidget, &MruTabWidget::tabAboutToClose,
            this, &EditorFrame::tabAboutToClose);

    connect(m_editorTabWidget, &MruTabWidget::tabContextMenuRequested,
        this, &EditorFrame::extendTabContextMenu);

    m_topSplitter->addWidget(m_projectTree);
    m_topSplitter->addWidget(m_editorTabWidget);
    m_topSplitter->setStretchFactor(0, 1);
    m_topSplitter->setStretchFactor(1, 3);
    m_topSplitter->setSizes({200, 600});

    m_mainSplitter->addWidget(m_topSplitter);

    m_mainSplitter->setStretchFactor(0, 4);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({600, 150});

    createActions();
    m_mainHeader->setupMenus(m_openFileAction, m_viewFileAction, m_closeAction,
                         m_exitAction, m_buildAction, m_runAction, m_aboutAction);
    m_mainHeader->setupToolBar(m_buildAction, m_runAction);

    m_mainLayout->addWidget(m_mainSplitter);
    setCentralWidget(central);

    // --- Finish setup ---
    setMinimumSize(600, 400);

    qApp->installEventFilter(this);
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
                m_editorTabWidget->closeTab(i);
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

    m_viewFileAction = new QAction(tr("&View File..."), this);
    connect(m_viewFileAction, &QAction::triggered, this, &EditorFrame::onViewFileTriggered);

    m_closeAction = new QAction(tr("&Close"), this);
    m_closeAction->setShortcut(QKeySequence::Close);
    connect(m_closeAction, &QAction::triggered, this, &EditorFrame::onCloseCurrentTabTriggered);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    //-----------
    m_buildAction = new QAction(QIcon(":/icons/hammer.svg"), tr("Build"), this);
    m_buildAction->setEnabled(false);
    //-----------
    m_runAction = new QAction(QIcon(":/icons/run.svg"), tr("&Run"), this);
    //-----------
    m_aboutAction = new QAction(tr("&About"), this);
    connect(m_aboutAction, &QAction::triggered, this, &EditorFrame::onAboutTriggered);
}

void EditorFrame::openFileInViewer(const QString& fileName)
{
    auto* viewer = new Viewer(fileName, m_editorTabWidget);

    // ESC inside viewer should close only this tab, not the whole app
    connect(viewer, &Viewer::closeRequested, this, [this, viewer]() {
        int idx = m_editorTabWidget->indexOf(viewer);
        if (idx >= 0) {
            m_editorTabWidget->removeTab(idx);
            viewer->deleteLater();
        }
        close();
    });

    // Add tab normally
    const QString baseName = QFileInfo(fileName).fileName();
    int newIndex = m_editorTabWidget->addTab(viewer, baseName);

    // If you have MRU/limit logic, keep it:
    int removedCount = m_editorTabWidget->enforceTabLimit();
    m_editorTabWidget->setCurrentIndex(newIndex - removedCount);
    viewer->setFocus();
}

/**
 * @brief Opens file in editor tab
 * @param filePath Full path to file
 *
 * Reuses existing tabs for already open files
 */
void EditorFrame::openFileInEditor(const QString& fileName)
{
    KTextEditor::Editor* kate = KTextEditor::Editor::instance(); // Singleton KTE Editor
    // Download Application interface from singleton Editor
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

    KTextEditor::Document* doc = app->findUrl(fileUrl);
    bool needsOpening = (doc == nullptr);

    if (needsOpening)
    {
        qDebug() << "Document not found by KTE Application, manually creating new Document for:" << fileUrl;
        doc = KTextEditor::Editor::instance()->createDocument(nullptr);

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
    }
    else
    {
        qDebug() << "Found existing document for:" << fileUrl;
    }

    Editor* newEditor = new Editor(doc, m_editorTabWidget);
    QString tabTitle = generateUniqueTabTitle(newEditor->filePath());
    int newIndex = m_editorTabWidget->addTab(newEditor, tabTitle);
    int removedCount = m_editorTabWidget->enforceTabLimit();
    m_editorTabWidget->setCurrentIndex(newIndex - removedCount);
    qobject_cast<Editor*>(m_editorTabWidget->currentWidget())->view()->setFocus();
    if (needsOpening)
    {
        qDebug() << "Manually opened file" << fileName << "in new tab (" << newIndex << ") with title:" << tabTitle;
    }
    else
    {
        qDebug() << "Created new tab (" << newIndex << ") for existing KTE document:" << fileName;
    }
}


void EditorFrame::viewFileInEditor(const QString& fileName) {
    int existingTabIndex = findTabByPath(fileName);
    if (existingTabIndex >= 0)
    {
        m_editorTabWidget->setCurrentIndex(existingTabIndex);
        qDebug() << "Activated existing tab for:" << fileName;
        return;
    }

    auto newViewer = new Viewer(fileName, m_editorTabWidget);
    QString tabTitle = generateUniqueTabTitle(newViewer->filePath());
    int newIndex = m_editorTabWidget->addTab(newViewer, tabTitle);
    int removedCount = m_editorTabWidget->enforceTabLimit();
    m_editorTabWidget->setCurrentIndex(newIndex - removedCount);
    newViewer->setFocus();
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
        openFileInEditor(fileName);
}


void EditorFrame::onViewFileTriggered()
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
        viewFileInEditor(fileName);
}

void EditorFrame::onTreeItemExpanded(const QModelIndex& index)
{
    QStandardItem* item = m_projectModel->itemFromIndex(index);
    if (!item)
        return;

    // Jeśli dzieci są prawdziwe (nie placeholder), już załadowane
    if (item->rowCount() > 0 && item->child(0)->data(Qt::UserRole + 1).isValid())
        return;

    // Usuwamy placeholdery
    item->removeRows(0, item->rowCount());

    QString path = item->data(Qt::UserRole + 1).toString();
    QDir dir(path);
    QFileInfoList entries = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files,
        QDir::Name
    );

    for (const QFileInfo& entry : entries)
    {
        QStandardItem* child = new QStandardItem(entry.fileName());
        child->setEditable(false);
        child->setData(entry.absoluteFilePath(), Qt::UserRole + 1);

        if (entry.isDir())
        {
            child->appendRow(new QStandardItem()); // Placeholder dla folderu
        }

        item->appendRow(child);
    }
}

void EditorFrame::onCloseCurrentTabTriggered()
{
    m_editorTabWidget->closeTab(m_editorTabWidget-> currentIndex());
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
    aboutText += "\n" + tr("Eudaimonia IDE\nVersion ") + version;

    QMessageBox::about(this, tr("About Eudaimonia"), aboutText);
}

/**
 * @brief Closes editor tab
 * @param index Tab index to close
 * @return true if closed successfully
 *
 * Handles document saving and KTextEditor integration
 */
bool EditorFrame::cleanupBeforeTabClose(int index)
{
    QWidget* widget = m_editorTabWidget->widget(index);
    Editor* editor = qobject_cast<Editor*>(widget); // Cast to our Editor view
    if (!editor)
        return true;
    bool success = true;

    if (editor && editor->document())
    {
        qDebug() << "save if needed";
        KTextEditor::Document* doc = editor->document(); // Pobierz wskaźnik do dokumentu

        // Sprawdzenie modyfikacji i zapis (kod jak poprzednio)
        if (editor->isModified())
        {
            qDebug() << "Tab" << index << "is modified. Saving file:" << editor->filePath();
            if (!editor->saveFile())
            {
                qWarning() << "Failed to save file via Editor::saveFile:" << editor->filePath() << "Aborting close.";
                success = false;
            }
            else
            {
                qDebug() << "File saved successfully:" << editor->filePath();
            }
        }
        else
        {
            qDebug() << "Tab" << index << "not modified. Closing.";
        }

        if (success)
        {
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
    }
    else if (widget)
    {
        qDebug() << "Closing non-Editor tab at index:" << index;
    }
    return success;
}

void EditorFrame::tabAboutToClose(int index, bool &allow_close)
{
    if (!allow_close)
        return;
    QWidget* w = m_editorTabWidget->widget(index);
    auto base_viewer = qobject_cast<BaseViewer*>(w);
    if (base_viewer && base_viewer->isModified())
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    tr("Unsaved Changes"),
                    tr("Close tab with unsaved changes?"),
                    QMessageBox::Yes|QMessageBox::No
                );
        allow_close = (reply == QMessageBox::Yes);
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

void EditorFrame::onProjectTreeKeyPressed(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        QModelIndex current = m_projectTree->currentIndex();
        if (current.isValid())
            onProjectTreeActivated(current);
    }
}

bool EditorFrame::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_projectTree && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        onProjectTreeKeyPressed(keyEvent);
        return true;
    }
    QObject *testObj = obj;
    while (testObj) {
        if (
            qobject_cast<KTextEditor::View*>(testObj)) {
            if (event->type() == QEvent::KeyPress) {
                auto keyEvent = dynamic_cast<QKeyEvent*>(event);
                if (keyEvent->key() == Qt::Key_Tab && keyEvent->modifiers() == Qt::ControlModifier)
                {
                    QKeyEvent *newEvent = new QKeyEvent(
                       event->type(),
                       keyEvent->key(),
                       keyEvent->modifiers(),
                       keyEvent->text(),
                       keyEvent->isAutoRepeat(),
                       keyEvent->count()
                   );
                    QApplication::postEvent(m_editorTabWidget, newEvent);
                    return true;
                }
            }
            }
        testObj = testObj->parent();
    }
    return false;
}

void EditorFrame::onProjectTreeActivated(const QModelIndex& index)
{
    QString path = index.data(Qt::UserRole + 1).toString();
    if (path.isEmpty())
        return;

    QFileInfo info(path);

    if (info.isDir())
    {
        if (m_projectTree->isExpanded(index))
            m_projectTree->collapse(index);
        else
            m_projectTree->expand(index);
    }
    else if (info.isFile())
    {
        // Otwieranie pliku jak w File->Open File
        openFileInEditor(path);
    }
}

void EditorFrame::closeEvent(QCloseEvent* event)
{
    if (tryCloseAll()) {
        event->accept();
    } else {
        event->ignore();
    }
}

bool EditorFrame::tryCloseAll() {
    return true;
}