#include "editor/editor.h"
#include "quitls.h"
#include "utils.h"
#include "Archives.h"
#include "PackDialog.h"
#include "DistroInfo.h"

#include <QApplication>
#include <QInputDialog>
#include <QMimeDatabase>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>

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
        currentFilePanel()->run(QString::fromStdString(utils::trim(commandLineEdit->text().toStdString())));
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
    commandLineEdit->setText(commandLineEdit->text() + qEscapePathForShell(currentPath)+" ");
    return true;
}

bool MainWindow::doCurrentNameToCommandLine(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    commandLineEdit->setText(commandLineEdit->text() + qEscapePathForShell(currentPanelName())+" ");
    return true;
}

bool MainWindow::doCurrentPathToCommandLine(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    commandLineEdit->setText(commandLineEdit->text() + qEscapePathForShell(currentPanelPath())+" ");
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
        editorFrame = new EditorFrame(nullptr);
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
    if (!editorFrame)
        editorFrame = new EditorFrame(nullptr);
    if (name.isEmpty()) {
        editorFrame->newFile();
    } else {
        QDir dir(currentFilePanel()->currentPath);
        QString fullPath = dir.absoluteFilePath(name);
        editorFrame->openFile(fullPath);
    }
    QCoreApplication::processEvents();
    editorFrame->show();
    editorFrame->raise();
    editorFrame->activateWindow();
    return true;
}

bool MainWindow::doExtractFiles(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    FilePanel* panel = currentFilePanel();
    if (!panel) return true;

    // 1. Get current file
    auto [entry, row] = panel->currentEntryRow();
    if (!entry) {
        QMessageBox::information(this, tr("Extract"), tr("No file selected."));
        return true;
    }

    QString fileName = entry->info.fileName();
    QString filePath = QDir(panel->currentPath).absoluteFilePath(fileName);

    // 2. Check if it's an archive
    QMimeDatabase mimeDb;
    QMimeType mt = mimeDb.mimeTypeForFile(filePath);
    auto [components, archiveType] = classifyArchive(mt, filePath);

    if (archiveType == DetailedArchiveType::NotArchive) {
        QMessageBox::information(this, tr("Extract"),
            tr("'%1' is not a recognized archive format.").arg(fileName));
        return true;
    }

    // 3. Get destination from opposite panel
    QString destPath;
    if (auto* oppPanel = oppositeFilePanel()) {
        destPath = oppPanel->currentPath;
    } else {
        destPath = panel->currentPath;
    }

    // 4. Show destination dialog
    bool ok;
    QString dest = QInputDialog::getText(this, tr("Extract files"),
        tr("Extract to:"), QLineEdit::Normal, destPath, &ok);

    if (!ok || dest.isEmpty()) {
        return true;
    }

    dest = QDir(dest).absolutePath();

    // 5. Check archive structure and target directory
    ArchiveContents contents = readArchive(filePath);
    bool hasSingleRoot = archiveHasSingleRoot(contents);
    bool targetNotEmpty = !QDir(dest).isEmpty();

    QString finalDest = dest;

    if (!hasSingleRoot && targetNotEmpty) {
        // Archive has multiple roots and target is not empty
        // Propose creating subdirectory
        QFileInfo fi(filePath);
        QString baseName = fi.completeBaseName();

        // Strip multi-volume suffixes like .part01, .part1, etc.
        static QRegularExpression multiVolPattern(R"(\.part\d+$)", QRegularExpression::CaseInsensitiveOption);
        baseName.remove(multiVolPattern);

        // Find unique name
        QString subDir = baseName;
        int counter = 1;
        while (QDir(dest).exists(subDir)) {
            subDir = QString("%1(%2)").arg(baseName).arg(counter++);
        }

        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Extract to subdirectory?"));
        msgBox.setText(tr("Archive contains multiple items and target directory is not empty.\n\n"
                          "Create subdirectory '%1'?").arg(subDir));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.button(QMessageBox::Yes)->setText(tr("Create subdirectory"));
        msgBox.button(QMessageBox::No)->setText(tr("Extract here"));

        int result = msgBox.exec();
        if (result == QMessageBox::Cancel) {
            return true;
        }
        if (result == QMessageBox::Yes) {
            finalDest = QDir(dest).absoluteFilePath(subDir);
        }
    }

    // 6. Extract
    QProgressDialog progress(tr("Extracting files..."), QString(), 0, 0, this);
    progress.setWindowTitle(tr("Extract"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    progress.show();
    QApplication::processEvents();

    QString error = extractArchive(filePath, finalDest);

    progress.close();

    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Extract Error"), error);
    }

    // Refresh panels and restore selection on archive
    reloadAllPanels();
    panel->selectEntryByName(fileName);

    return true;
}

bool MainWindow::doFocusCommandLine(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    commandLineEdit->selectAll();
    commandLineEdit->setFocus();
    return true;
}

bool MainWindow::doGoToHome(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    FilePanel* panel = currentFilePanel();
    if (!panel)
        return true;

    QString homePath = QDir::homePath();
    panel->currentPath = homePath;
    panel->loadDirectory();
    panel->setFocus();

    return true;
}

bool MainWindow::doGoToRoot(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    FilePanel* panel = currentFilePanel();
    if (!panel)
        return true;

    QString rootPath = QDir::rootPath();
    panel->currentPath = rootPath;
    panel->loadDirectory();
    panel->setFocus();

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

    FilePanel* panel = currentFilePanel();
    if (!panel) return true;

    // 1. Determine files to pack
    QStringList markedRelPaths = panel->getMarkedRelPaths();
    bool hasMarked = !markedRelPaths.isEmpty();

    QString defaultName;
    if (!hasMarked || markedRelPaths.size() == 1) {
        // Use current file/dir name
        defaultName = panel->getRowName(panel->currentIndex().row());
    } else {
        // Use parent directory name
        QDir dir(panel->currentPath);
        defaultName = dir.dirName();
    }

    // 2. Get opposite panel path for destination
    QString destPath;
    if (auto* oppPanel = oppositeFilePanel()) {
        destPath = oppPanel->currentPath;
    } else {
        destPath = panel->currentPath;
    }

    // 3. Show dialog
    PackDialog dlg(defaultName, destPath, markedRelPaths.size(), this);
    if (dlg.exec() != QDialog::Accepted)
        return true;

    // 4. Check executable
    QString packer = dlg.packerType();
    QString executable = (packer == "7z") ? "7z" : "zip";
    QString execPath = QStandardPaths::findExecutable(executable);

    if (execPath.isEmpty()) {
        // Show install instructions
        QString installCmd = DistroInfo::installCommand(
            (packer == "7z") ? "p7zip-full" : "zip");
        QMessageBox::warning(this, tr("Packer Not Found"),
            tr("%1 not found.\n\nInstall with:\n%2")
                .arg(executable, installCmd));
        return true;
    }

    // 5. Collect source files
    QStringList filesToPack;
    if (hasMarked) {
        filesToPack = markedRelPaths;
    } else {
        filesToPack << panel->getRowRelPath(panel->currentIndex().row());
    }

    // 6. Validate destination - archive cannot be inside source directory
    QString destDir = QDir(dlg.destination()).absolutePath();
    QString srcDir = QDir(panel->currentPath).absolutePath();

    // Check if destination is inside source or source is inside destination
    for (const QString& fileRelPath : filesToPack) {
        QFileInfo fi(fileRelPath);
        QString filePath = fi.absoluteFilePath();

        // Check if archive destination is inside any source directory
        if (destDir.startsWith(filePath + "/") || destDir == filePath) {
            QMessageBox::warning(this, tr("Invalid Destination"),
                tr("Cannot create archive inside the source directory:\n%1")
                    .arg(filePath));
            return true;
        }
    }

    // 7. Build and execute command
    QString archivePath = QDir(dlg.destination())
        .absoluteFilePath(dlg.archiveName());

    // Show progress dialog
    QProgressDialog progress(tr("Packing files..."), QString(), 0, 0, this);
    progress.setWindowTitle(tr("Pack"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    progress.show();
    QApplication::processEvents();

    QString error;
    if (packer == "7z") {
        error = pack7z(archivePath, filesToPack, dlg.moveFiles(),
                       dlg.volumeSize(), dlg.solidBlockSize());
    } else {
        error = packZip(archivePath, filesToPack, dlg.moveFiles());
    }

    progress.close();

    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Pack Error"), error);
    }

    // Refresh panels to show new archive
    reloadAllPanels();

    return true;
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

bool MainWindow::doFollowDirFromLeft(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    FilePanel* leftPanel = filePanelForSide(Side::Left);
    FilePanel* rightPanel = filePanelForSide(Side::Right);
    FilePanel* currentPanel = currentFilePanel();

    if (!leftPanel || !rightPanel)
        return true;

    // Only work when left panel is active
    if (currentPanel == leftPanel) {
        QModelIndex currentIndex = leftPanel->currentIndex();
        if (currentIndex.isValid()) {
            QString selectedName = leftPanel->getRowRelPath(currentIndex.row());
            if (!selectedName.isEmpty()) {
                // Build full path to selected item
                QDir leftDir(leftPanel->currentPath);
                QString selectedPath = leftDir.absoluteFilePath(selectedName);
                // Use navigateToPath - handles both dirs and files
                rightPanel->navigateToPath(selectedPath);
                return true;
            }
        }
    }

    // [..] or no selection: follow current directory
    rightPanel->currentPath = leftPanel->currentPath;
    rightPanel->loadDirectory();
    rightPanel->selectFirstEntry();

    return true;
}

bool MainWindow::doFollowDirFromRight(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    FilePanel* leftPanel = filePanelForSide(Side::Left);
    FilePanel* rightPanel = filePanelForSide(Side::Right);
    FilePanel* currentPanel = currentFilePanel();

    if (!leftPanel || !rightPanel)
        return true;

    // Only work when right panel is active
    if (currentPanel == rightPanel) {
        QModelIndex currentIndex = rightPanel->currentIndex();
        if (currentIndex.isValid()) {
            QString selectedName = rightPanel->getRowRelPath(currentIndex.row());
            if (!selectedName.isEmpty()) {
                // Build full path to selected item
                QDir rightDir(rightPanel->currentPath);
                QString selectedPath = rightDir.absoluteFilePath(selectedName);
                // Use navigateToPath - handles both dirs and files
                leftPanel->navigateToPath(selectedPath);
                return true;
            }
        }
    }

    // [..] or no selection: follow current directory
    leftPanel->currentPath = rightPanel->currentPath;
    leftPanel->loadDirectory();
    leftPanel->selectFirstEntry();

    return true;
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

