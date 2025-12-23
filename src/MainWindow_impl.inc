#include "utils.h"
#include "editor/editor.h"


bool MainWindow::doNextTab(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    goToNextTab(tabsForSide(m_activeSide));
    return true;
}

bool MainWindow::doPreviousTab(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    goToPreviousTab(tabsForSide(m_activeSide));
    return true;
}

bool MainWindow::doActivate(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    if (!commandLineEdit->text().isEmpty()) {
        currentFilePanel()->activate(QString::fromStdString(utils::trim(commandLineEdit->text().toStdString())));
        doClearAndReturnToPanel(obj, keyEvent);
    } else {
        currentFilePanel()->trigger(currentPanelName());
    }
    return true;
}

bool MainWindow::doAddTab(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    // Ctrl+T: duplicate current tab on active side
    QTabWidget* tabs = tabsForSide(m_activeSide);
    FilePaneWidget* pane = currentPane();
    if (!tabs || !pane)
        return true;

    const QString path = pane->currentPath();

    auto* newPane = new FilePaneWidget(m_activeSide, tabs);
    // duplicate the same folder - force load even if path matches default
    newPane->filePanel()->currentPath.clear();
    newPane->setCurrentPath(path);
    // Select first row
    newPane->filePanel()->selectRow(0);

    // Connect directoryChanged signal for the new panel
    connect(newPane->filePanel(), &FilePanel::directoryChanged,
            this, &MainWindow::updateCurrentPathLabel);

    const int insertIndex = tabs->currentIndex() + 1;
    const int newIndex = tabs->insertTab(insertIndex, newPane, tabs->tabText(tabs->currentIndex()));

    tabs->setCurrentIndex(newIndex);
    newPane->filePanel()->setFocus();
    return true;
}

bool MainWindow::doCompareDirectories(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doCompareDirectories";
    return false;
}

bool MainWindow::doCopy(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    FilePanel* panel = currentFilePanel();
    if (panel) {
        copyFromPanel(panel);
    }
    return true;
}

bool MainWindow::doCopyInPlace(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    FilePanel* panel = currentFilePanel();
    if (panel) {
        copyFromPanel(panel, true);  // inPlace = true
    }
    return true;
}

bool MainWindow::doCurrentDirToCommandLine(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    QString currentPath = currentFilePanel()->currentPath;
    commandLineEdit->setText(commandLineEdit->text() + QString::fromStdString(utils::escapePathForShell(currentPath.toStdString()))+" ");
    return true;
}

bool MainWindow::doCurrentNameToCommandLine(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    commandLineEdit->setText(commandLineEdit->text() + QString::fromStdString(utils::escapePathForShell(currentPanelName().toStdString()))+" ");
    return true;
}

bool MainWindow::doCurrentPathToCommandLine(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    commandLineEdit->setText(commandLineEdit->text() + QString::fromStdString(utils::escapePathForShell(currentPanelPath().toStdString()))+" ");
    return true;
}

bool MainWindow::doDirHotlist(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    showFavoriteDirsMenu(m_activeSide);
    return true;
}

bool MainWindow::doDirHotlistConfig(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doDirHotlistConfig";
    return false;
}

bool MainWindow::doEdit(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    auto fullPath = currentPanelPath();
    QFileInfo info(fullPath);
    if (!info.isFile()) {
        // selected is directory - nothing
        return true;
    }
    if (!editorFrame)
        editorFrame = new EditorFrame(this);
    editorFrame->openFile(fullPath);
    editorFrame->show();
    editorFrame->raise();
    editorFrame->activateWindow();
    return true;
}

bool MainWindow::doEditNew(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    auto suggestedName = currentPanelName();

    bool ok = false;
    QString name = QInputDialog::getText(
        this,
        tr("Create new directory"),
        tr("Input new name:"),
        QLineEdit::Normal,
        suggestedName,
        &ok
    );

    if (!ok)
        return false;
    QString fullPath;
    if (!name.isEmpty()) {
        QDir dir(currentFilePanel()->currentPath);
        fullPath = dir.absoluteFilePath(name);
    }
    if (!editorFrame)
        editorFrame = new EditorFrame(this);
    editorFrame->openFile(fullPath);
    editorFrame->show();
    editorFrame->raise();
    editorFrame->activateWindow();
    return true;
}

bool MainWindow::doExtractFiles(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doExtractFiles";
    return false;
}

bool MainWindow::doFocusCommandLine(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    commandLineEdit->selectAll();
    commandLineEdit->setFocus();
    return true;
}

bool MainWindow::doMakeDirectoryOpposite(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doMakeDirectoryOpposite";
    return false;
}

bool MainWindow::doMove(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    FilePanel* panel = currentFilePanel();
    if (panel) {
        moveFromPanel(panel, false);  // inPlace = false
    }
    return true;
}

bool MainWindow::doMoveInPlace(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    FilePanel* panel = currentFilePanel();
    if (panel) {
        moveFromPanel(panel, true);  // inPlace = true (rename)
    }
    return true;
}

bool MainWindow::doPackFiles(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doPackFiles";
    return false;
}

bool MainWindow::doRemoveTab(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    QTabWidget* tabs = tabsForSide(m_activeSide);
    if (!tabs || tabs->count() <= 1)
        return true;  // Don't close last tab

    int index = tabs->currentIndex();
    QWidget* w = tabs->widget(index);
    tabs->removeTab(index);
    w->deleteLater();

    // Focus on the new current panel
    currentFilePanel()->setFocus();
    return true;
}

bool MainWindow::doRename(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doRename";
    return false;
}

bool MainWindow::doReturnToPanel(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    currentFilePanel()->setFocus();
    return true;
}

bool MainWindow::doClearAndReturnToPanel(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    commandLineEdit->clear();
    currentFilePanel()->setFocus();
    return true;
}

bool MainWindow::doSearchGlobal(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    QString startPath = currentFilePanel()->currentPath;

    if (!m_searchDialog) {
        m_searchDialog = new SearchDialog(startPath, this);

        connect(m_searchDialog, &SearchDialog::requestEdit, this, &MainWindow::openEditorForFile);
        connect(m_searchDialog, &SearchDialog::requestView, this, &MainWindow::openViewerForFile);
        connect(m_searchDialog, &SearchDialog::requestGoToFile, this, &MainWindow::goToFile);
        connect(m_searchDialog, &SearchDialog::requestFeedToListbox, this, [this](const QVector<SearchResult>& results, const QString& searchPath) {
            currentFilePanel()->feedSearchResults(results, searchPath);
            currentFilePanel()->setFocus();
        });
    }

    // Always update search path from current panel
    m_searchDialog->setSearchPath(startPath);
    m_searchDialog->show();
    m_searchDialog->raise();
    m_searchDialog->activateWindow();

    return true;
}

bool MainWindow::doSwapPanels(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doSwapPanels";
    return false;
}

bool MainWindow::doSwitchSide(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    auto new_panel = oppositeFilePanel();
    if (new_panel) new_panel->setFocus();
    return true;
}

bool MainWindow::doTestArchives(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doTestArchives";
    return false;
}

bool MainWindow::doView(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    auto fullPath = currentPanelPath();
    QFileInfo info(fullPath);
    if (!info.isFile()) {
        // selected is directory - nothing
        return true;
    }
    if (!viewerFrame)
        viewerFrame = new ViewerFrame(fullPath, this);
    else
        viewerFrame->openFile(fullPath);
    viewerFrame->show();
    viewerFrame->raise();
    viewerFrame->activateWindow();
    return true;
}

bool MainWindow::doToggleMarkDown(QObject *obj, QKeyEvent *keyEvent) {
    FilePanel* panel = currentFilePanel();
    if (panel)
        return panel->doToggleMarkDown(obj, keyEvent);
    return true;
}

bool MainWindow::doMakeDirectory(QObject *obj, QKeyEvent *keyEvent) {
    FilePanel* panel = currentFilePanel();
    if (panel)
        return panel->doMakeDirectory(obj, keyEvent);
    return true;
}

bool MainWindow::doDeleteToTrash(QObject *obj, QKeyEvent *keyEvent) {
    FilePanel* panel = currentFilePanel();
    if (panel)
        return panel->doDeleteToTrash(obj, keyEvent);
    return true;
}

bool MainWindow::doDeletePermanently(QObject *obj, QKeyEvent *keyEvent) {
    FilePanel* panel = currentFilePanel();
    if (panel)
        return panel->doDeletePermanently(obj, keyEvent);
    return true;
}

void MainWindow::openEditorForFile(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.isFile())
        return;

    if (!editorFrame)
        editorFrame = new EditorFrame(this);
    editorFrame->openFile(filePath);

    // If opening config file, connect signal to reload config on save
    if (Config::instance().isConfigFile(filePath)) {
        if (Editor* editor = editorFrame->currentEditor()) {
            connect(editor, &Editor::configFileSaved,
                    this, &MainWindow::onConfigSaved,
                    Qt::UniqueConnection);
        }
    }

    editorFrame->show();
    editorFrame->raise();
    editorFrame->activateWindow();
}

void MainWindow::openViewerForFile(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.isFile())
        return;

    if (!viewerFrame)
        viewerFrame = new ViewerFrame(filePath, this);
    else
        viewerFrame->openFile(filePath);
    viewerFrame->show();
    viewerFrame->raise();
    viewerFrame->activateWindow();
}

void MainWindow::goToFile(const QString& dir, const QString& name) {
    FilePanel* panel = currentFilePanel();
    if (!panel)
        return;

    QDir targetDir(dir);
    if (!targetDir.exists())
        return;

    panel->currentPath = targetDir.absolutePath();
    panel->loadDirectory();
    panel->selectEntryByName(name);
    panel->setFocus();
}

