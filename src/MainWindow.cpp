#include "MainWindow.h"

#include "Config.h"
#include "FilePaneWidget.h"
#include "FilePanel.h"

#include "editor/EditorFrame.h"
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

#include <QHeaderView>
#include <QTimer>
#include <QActionGroup>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QInputDialog>
#include <QProcess>
#include <QStandardPaths>
#include <QMessageBox>
#include <QStorageInfo>

#include "SortedDirIterator.h"
#include "SearchDialog.h"
#include "editor/ViewerFrame.h"
#include "keys/KeyRouter.h"
#include "keys/ObjectRegistry.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    ObjectRegistry::add(this, "MainFrame");
    QString cfg = Config::instance().defaultConfigPath();
    Config::instance().load(cfg);
    Config::instance().setConfigPath(cfg);

    setupUi();

    for (auto* panel : allFilePanels()) {
        panel->loadDirectory();
        // Connect directoryChanged to update the label
        connect(panel, &FilePanel::directoryChanged,
                this, &MainWindow::updateCurrentPathLabel);
    }
    filePanelForSide(Side::Left)->setFocus();

    updateCurrentPathLabel();  // initial update

    this->setStyleSheet(
        "QMainWindow {"
        "  background-color: #f4f4f4;"
        "  padding: 0px 0px;"
        "}"
    );

    setWindowTitle("Gemini Commander");
    setWindowIcon(QIcon(":/icons/gemini-commander.svg"));
    resize(1024, 768);
    keyMap.load(":/config/keys.toml");
    KeyRouter::instance().setKeyMap(&keyMap);
    KeyRouter::instance().installOn(qApp, this);
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    auto* splitter = new QSplitter(Qt::Horizontal, centralWidget);

    m_leftTabs = new QTabWidget(splitter);
    ObjectRegistry::add(m_leftTabs, "Tabs");
    m_rightTabs = new QTabWidget(splitter);
    ObjectRegistry::add(m_rightTabs, "Tabs");
    auto tuneTabBar = [](QTabWidget* tabs) {
        QTabBar* bar = tabs->tabBar();
        QFontMetrics fm(bar->font());
        int h = fm.height() + 4; // smaller than default
        bar->setStyleSheet(QStringLiteral(
            "QTabBar::tab { "
            "  height: %1px; "
            "}").arg(h));
    };
    tuneTabBar(m_leftTabs);
    tuneTabBar(m_rightTabs);

    splitter->addWidget(m_leftTabs);
    splitter->addWidget(m_rightTabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    auto* leftPane  = new FilePaneWidget(Side::Left, m_leftTabs);
    auto* rightPane = new FilePaneWidget(Side::Right, m_rightTabs);
    m_leftTabs->addTab(leftPane,  "Left");
    m_rightTabs->addTab(rightPane, "Right");

    // Bottom line: currentPath label (3/4 of left panel) + command line (rest)
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    currentPathLabel = new QLabel(centralWidget);
    currentPathLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #f4f4f4;"
        "  padding: 0px 4px;"
        "}"
    );
    currentPathLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    currentPathLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    commandLineEdit = new QLineEdit(centralWidget);
    commandLineEdit->setStyleSheet("QLineEdit { background-color: white; }");
    ObjectRegistry::add(commandLineEdit, "CommandLine");

    // Label: 3 units (3/4 of first panel), CommandLine: 5 units (1/4 + full second panel)
    bottomLayout->addWidget(currentPathLabel, 3);
    bottomLayout->addWidget(commandLineEdit, 5);

    mainLayout->addWidget(splitter);
    mainLayout->addLayout(bottomLayout);
    mainLayout->setStretchFactor(splitter, 1);

    setCentralWidget(centralWidget);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QMenu* commandsMenu = menuBar()->addMenu(tr("&Commands"));
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));

    // File menu
    QAction* quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAction);

    // Commands menu - Search action (no shortcut here, managed by KeyRouter/TOML)
    m_searchAction = new QAction(tr("Search files..."), this);
    m_searchAction->setIcon(QIcon(":/icons/search.svg"));
    connect(m_searchAction, &QAction::triggered, this, [this]() {
        doSearchGlobal(nullptr, nullptr);
    });
    commandsMenu->addAction(m_searchAction);

    // --- TOOLBAR ---
    m_mainToolBar = addToolBar(tr("Main toolbar"));
    m_mainToolBar->setMovable(true);

    m_openTerminalAction = new QAction(tr("Terminal"), this);
    // Ikonka później: m_openTerminalAction->setIcon(QIcon(":/icons/terminal.svg"));
    connect(m_openTerminalAction, &QAction::triggered,
            this, &MainWindow::onOpenTerminal);

    m_mainToolBar->addAction(m_openTerminalAction);
    m_mainToolBar->addAction(m_searchAction);

    addToolBarBreak(Qt::TopToolBarArea);
    createMountsToolbar();
    QSize icon16(16,16);
    m_mainToolBar->setIconSize(icon16);
    m_mountsToolBar->setIconSize(icon16);

    QFontMetrics fm(m_mainToolBar->font());
    int h = fm.height() + 8;
    m_mainToolBar->setFixedHeight(h);
    m_mountsToolBar->setFixedHeight(h);

    QString tbStyle =
        "QToolBar QToolButton { "
        "  padding: 0px; "
        "  margin: 0px; "
        "  min-height: 16px; "
        "} ";

    m_mainToolBar->setStyleSheet(tbStyle);
    m_mountsToolBar->setStyleSheet(tbStyle);
}

Side MainWindow::opposite(Side side){
    return side==Side::Left?Side::Right:Side::Left;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        Qt::KeyboardModifiers modifiers = keyEvent->modifiers();

        FilePanel* panel = panelForObject(obj);
        if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_Tab) {
            if (auto* panel = panelForObject(obj)) {
                // here you can be sure that the event came from the FilePanel / its viewport
                // e.g., switch the tab in the active QTabWidget
                // switchTab(+1);
                goToNextTab(tabsForSide(m_activeSide));
                return true; // IMPORTANT: we do not let you pass any further
            }
        }

        // Ctrl+Shift+Tab
        if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)
            && keyEvent->key() == Qt::Key_Backtab) {
            if (panel) {
                goToPreviousTab(tabsForSide(m_activeSide));
                return true;
            }
            }

        if ((keyEvent->key() == Qt::Key_F3||keyEvent->key() == Qt::Key_F4) && modifiers == Qt::NoModifier) {
            QModelIndex currentIndex = currentFilePanel()->currentIndex();
            if (!currentIndex.isValid()) {
                return true;
            }

            QString name = currentFilePanel()->getRowName(currentIndex.row());
            if (name == "") {
                return true;
            }

            QDir dir(currentFilePanel()->currentPath);
            QString fullPath = dir.absoluteFilePath(name);

            QFileInfo info(fullPath);
            if (!info.isFile()) {
                // selected is directory - nothing
                return true;
            }

            if (keyEvent->key() == Qt::Key_F3) {

            } else {

            }
            return true;
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_D) {
            showFavoriteDirsMenu(m_activeSide);
            return true;
        }
         else if (modifiers == Qt::NoModifier && keyEvent->key() == Qt::Key_Insert) {
            if (auto* panel = panelForObject(obj)) {
                panel->toggleMarkOnCurrent(true);
                return true;
            }

        } else if (modifiers == Qt::NoModifier && keyEvent->key() == Qt::Key_F7) {
            if (auto* panel = panelForObject(obj)) {
                panel->createNewDirectory(this);
                return true;
            }
        }

        else if (modifiers == Qt::NoModifier && keyEvent->key() == Qt::Key_F5) {
            if (auto* panel = panelForObject(obj)) {
                copyFromPanel(panel);
                return true;
            }
        }

        else if (modifiers == Qt::NoModifier && keyEvent->key() == Qt::Key_F6) {
            if (auto* panel = panelForObject(obj)) {
                Side srcSide = panel->side(); // masz już taką funkcję
                QString targetDir;
                Side dstSide = opposite(srcSide);
                if (auto* dstPanel = filePanelForSide(dstSide)) {
                    targetDir = dstPanel->currentPath;
                }
                panel->renameOrMoveEntry(this, targetDir);
                return true;
            }
        } else if ((keyEvent->key() == Qt::Key_F8 || keyEvent->key() == Qt::Key_Delete)
           && modifiers == Qt::NoModifier) {

        FilePanel* panel = currentFilePanel();
        if (!panel)
            return true;

        QModelIndex currentIndex = panel->currentIndex();
        if (!currentIndex.isValid())
            return true;

        // nazwa z wiersza
        QString name = panel->getRowName(currentIndex.row());
        if (name.isEmpty())
            return true;

        QDir dir(panel->currentPath);
        QString fullPath = dir.absoluteFilePath(name);
        QFileInfo info(fullPath);
        if (!info.exists())
            return true;

        QString question;
        bool useTrash = true;

        if (info.isDir()) {
            // ponowne, bieżące sprawdzenie pustego katalogu
            QDir d(fullPath);
            bool empty = d.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

            if (empty) {
                question = tr("Delete selected empty dir '%1'?").arg(name);
                useTrash = false;   // pusty katalog – kasujemy od razu
            } else {
                question = tr("Delete selected non empty dir '%1' into trashcan?").arg(name);
            }
        } else {
            question = tr("Delete selected '%1' into trashcan?").arg(name);
        }

        auto reply = QMessageBox::question(
            this,
            tr("Delete"),
            question,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        if (reply != QMessageBox::Yes)
            return true;

        bool ok = false;

        if (info.isDir() && !useTrash) {
            // pusty katalog – usuwamy bez kosza
            ok = dir.rmdir(name);
        } else {
        // plik lub niepusty katalog – próbujemy przenieść do kosza
        ok = QFile::moveToTrash(fullPath);
            }
            if (!ok) {
                QMessageBox::warning(
                    this,
                    tr("Error"),
                    tr("Failed to delete '%1'.").arg(name)
                );
            } else {
                panel->loadDirectory();
                panel->setCurrentIndex(currentIndex);
                panel->scrollTo(currentIndex);
            }
            return true;
        }
        else if ((keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right) && modifiers == Qt::NoModifier) {
            if (auto* panel = panelForObject(obj)) {
                commandLineEdit->setFocus();
                commandLineEdit->selectAll();
                return true; // Event handled
            }
        }             // Plain Enter: Let default handling (activated signal) proceed
         else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_T) {
            // Ctrl+T: duplicate current tab on active side
            QTabWidget* tabs = tabsForSide(m_activeSide);
            FilePaneWidget* pane = currentPane();
            if (!tabs || !pane)
                return true;

            const QString path = pane->currentPath();

            auto* newPane = new FilePaneWidget(m_activeSide, tabs);
            // duplicate the same folder
            newPane->setCurrentPath(path);

            // Connect directoryChanged signal for the new panel
            connect(newPane->filePanel(), &FilePanel::directoryChanged,
                    this, &MainWindow::updateCurrentPathLabel);

            const int insertIndex = tabs->currentIndex() + 1;
            const int newIndex = tabs->insertTab(insertIndex, newPane, tabs->tabText(tabs->currentIndex()));

            tabs->setCurrentIndex(newIndex);
            return true;
        }
    } else if (event->type() == QEvent::FocusIn) {
        if (auto* panel = panelForObject(obj)) {
            Side newSide = panel->side();
            if (m_activeSide != newSide) {
                // Change side - visually clear the old panel
                if (auto* oldPanel = filePanelForSide(m_activeSide))
                    oldPanel->clearSelection();
                m_activeSide = newSide;
            }
            // Restore selection (whether changing side or returning from the editor)
            panel->restoreSelectionFromMemory();
            panel->styleActive();
            updateCurrentPathLabel();
        }
    } else if (event->type() == QEvent::FocusOut) {
        if (auto* panel = panelForObject(obj)) {
            panel->rememberSelection();
            panel->styleInactive();
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::showFavoriteDirsMenu(Side side)
{
    FilePanel* panel = filePanelForSide(side);
    if (!panel)
        return;

    const QString currentDir = QDir::cleanPath(panel->currentPath);
    const auto& favorites = Config::instance().favoriteDirs();

    // Split favorites into root entries (no group) and grouped entries
    QVector<const FavoriteDir*> rootEntries;
    QMap<QString, QVector<const FavoriteDir*>> grouped;

    for (const auto& fav : favorites) {
        if (fav.group.isEmpty()) {
            rootEntries.append(&fav);
        } else {
            grouped[fav.group].append(&fav);
        }
    }

    QMenu menu(this);
    QActionGroup groupActions(&menu);
    groupActions.setExclusive(true);

    auto makeLabel = [](const FavoriteDir& fav) -> QString {
        if (!fav.label.isEmpty())
            return fav.label;
        QFileInfo fi(fav.path);
        QString label = fi.fileName();
        if (label.isEmpty())
            label = fav.path;
        return label;
    };

    int accelIndex = 1;

    // Root favorites – shown directly in the main menu
    for (const FavoriteDir* fav : rootEntries) {
        QString path = QDir::cleanPath(fav->path);
        QString label = makeLabel(*fav);

        QString text;
        if (accelIndex <= 9) {
            text = QString("&%1  %2").arg(accelIndex).arg(label);
            ++accelIndex;
        } else {
            text = label;
        }

        QAction* act = menu.addAction(text);
        act->setCheckable(true);
        act->setData(path);

        if (!currentDir.isEmpty() && path == currentDir)
            act->setChecked(true);

        groupActions.addAction(act);
    }

    // Grouped favorites – each group as submenu
    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
        const QString& groupName = it.key();
        const auto& entries = it.value();

        QMenu* sub = menu.addMenu(groupName);

        for (const FavoriteDir* fav : entries) {
            QString path = QDir::cleanPath(fav->path);
            QString label = makeLabel(*fav);

            QString text;
            if (accelIndex <= 9) {
                text = QString("&%1  %2").arg(accelIndex).arg(label);
                ++accelIndex;
            } else {
                text = label;
            }

            QAction* act = sub->addAction(text);
            act->setCheckable(true);
            act->setData(path);

            if (!currentDir.isEmpty() && path == currentDir)
                act->setChecked(true);

            groupActions.addAction(act);
        }
    }

    if (!favorites.isEmpty())
        menu.addSeparator();

    if (favorites.isEmpty()) {
        QAction* infoAct = menu.addAction("No dirs yet");
        infoAct->setEnabled(false);
    }

    menu.addSeparator();
    QAction* addAct = menu.addAction("Add current dir");

    bool alreadyInFavorites = Config::instance().containsFavoriteDir(currentDir);
    addAct->setEnabled(!alreadyInFavorites && !currentDir.isEmpty());

    // Popup position: above the active panel
    QPoint panelPos = panel->mapToGlobal(QPoint(panel->width() / 2, panel->height()/2));
    QAction* chosen = menu.exec(panelPos);
    if (!chosen)
        return;

    if (chosen == addAct) {
        // Add to root group ("")
        Config::instance().addFavoriteDir(currentDir, /*label*/ QString(), /*group*/ QString());
        Config::instance().save();
        return;
    }

    QString targetDir = chosen->data().toString();
    if (targetDir.isEmpty())
        return;

    QDir dir(targetDir);
    if (!dir.exists())
        return;

    panel->currentPath = dir.absolutePath();
    panel->loadDirectory();
    panel->setFocus();
}

void MainWindow::reloadAllPanels()
{
    for (QTabWidget* tabs : {m_leftTabs, m_rightTabs}) {
        if (!tabs) continue;
        for (int i = 0; i < tabs->count(); i++) {
            if (auto* pane = qobject_cast<FilePaneWidget*>(tabs->widget(i))) {
                pane->filePanel()->loadDirectory();
            }
        }
    }
}

QVector<FilePanel*> MainWindow::allFilePanels() const
{
    QVector<FilePanel*> result;

    for (QTabWidget* tabs : {m_leftTabs, m_rightTabs}) {
        if (!tabs) continue;

        for (int i = 0; i < tabs->count(); ++i) {
            if (auto* pane = qobject_cast<FilePaneWidget*>(tabs->widget(i))) {
                result.append(pane->filePanel());
            }
        }
    }

    return result;
}


FilePaneWidget* MainWindow::paneForSide(Side side) const
{
    QTabWidget* tabs = nullptr;
    if (side == Side::Left)
        tabs = m_leftTabs;
    else
        tabs = m_rightTabs;

    if (!tabs || tabs->count() == 0)
        return nullptr;

    return qobject_cast<FilePaneWidget*>(tabs->currentWidget());
}

FilePanel* MainWindow::filePanelForSide(Side side) const
{
    if (auto* pane = paneForSide(side))
        return pane->filePanel();
    return nullptr;
}

FilePaneWidget* MainWindow::currentPane() const
{
    return paneForSide(m_activeSide);
}

FilePanel* MainWindow::currentFilePanel() const
{
    return filePanelForSide(m_activeSide);
}

FilePanel* MainWindow::oppositeFilePanel() const
{
    return filePanelForSide(opposite(m_activeSide));
}

FilePanel* MainWindow::panelForObject(QObject* obj) const
{
    // Fokus czasem trafia w viewport, czasem w sam FilePanel
    if (auto* panel = qobject_cast<FilePanel*>(obj))
        return panel;

    if (auto* w = qobject_cast<QWidget*>(obj)) {
        if (auto* panel = qobject_cast<FilePanel*>(w->parentWidget()))
            return panel;
    }

    return nullptr;
}

QTabWidget* MainWindow::tabsForSide(Side side) const
{
    if (side == Side::Left)
        return m_leftTabs;
    if (side == Side::Right)
        return m_rightTabs;
    return nullptr;
}

void MainWindow::goToNextTab(QTabWidget* tabWidget) {
    int current = tabWidget->currentIndex();
    int count = tabWidget->count();

    if (current < count - 1) {
        tabWidget->setCurrentIndex(current + 1);
    } else {
        tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::goToPreviousTab(QTabWidget* tabWidget) {
    int current = tabWidget->currentIndex(); // Bieżący indeks

    if (current > 0) {
        tabWidget->setCurrentIndex(current - 1);
    } else {
        tabWidget->setCurrentIndex(tabWidget->count() - 1);
    }
}

void MainWindow::onOpenTerminal()
{
    // Ustal katalog roboczy – z aktywnego panelu, a jak nie ma, to HOME
    QString workDir;
    if (auto* pane = currentPane()) {
        workDir = pane->currentPath();
    }
    if (workDir.isEmpty()) {
        workDir = QDir::homePath();
    }

    // Lista preferowanych terminali
    const QStringList candidates = {
        QStringLiteral("gnome-terminal"),
        QStringLiteral("konsole"),
        QStringLiteral("xfce4-terminal"),
        QStringLiteral("xterm")
    };

    QString termCmd;
    for (const QString& c : candidates) {
        if (!QStandardPaths::findExecutable(c).isEmpty()) {
            termCmd = c;
            break;
        }
    }

    if (termCmd.isEmpty()) {
        QMessageBox::warning(this,
                             tr("Terminal"),
                             tr("No terminal emulator found (tried gnome-terminal, konsole, xfce4-terminal, xterm)."));
        return;
    }

    QStringList args;
    // Specjalne argumenty dla niektórych terminali
    if (termCmd == QLatin1String("gnome-terminal")) {
        args << QStringLiteral("--working-directory=%1").arg(workDir);
    } else if (termCmd == QLatin1String("konsole")) {
        args << QStringLiteral("--workdir") << workDir;
    } else if (termCmd == QLatin1String("xfce4-terminal")) {
        args << QStringLiteral("--working-directory=%1").arg(workDir);
    }
    // dla xterm i innych – użyjemy tylko workingDirectory w QProcess

    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(workDir);
    proc->start(termCmd, args);

    if (!proc->waitForStarted(1000)) {
        QMessageBox::warning(this,
                             tr("Terminal"),
                             tr("Failed to start terminal: %1").arg(termCmd));
        proc->deleteLater();
    }
}

QStringList MainWindow::listMountPoints() const
{
    QStringList pts;

    QFile f("/proc/mounts");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return pts;

    QTextStream in(&f);

    const QString user = qEnvironmentVariable("USER");
    const QString userMedia1 = user.isEmpty()
        ? QString()
        : ("/media/" + user + "/");
    const QString userMedia2 = user.isEmpty()
        ? QString()
        : ("/run/media/" + user + "/");

    while (true) {
        const QString line = in.readLine();
        if (line.isNull())
            break; // EOF

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const auto parts = trimmed.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 3)
            continue;

        const QString device     = parts[0];
        const QString mountPoint = parts[1];
        const QString fsType     = parts[2];

        static const QSet<QString> goodFsTypes = {
            "ext4",
            "exfat",
            "vfat",
            "ntfs",
            "btrfs",
            "xfs"
        };
        if (!goodFsTypes.contains(fsType))
            continue;

        if (!user.isEmpty()) {
            if (!mountPoint.startsWith(userMedia1) &&
                !mountPoint.startsWith(userMedia2))
            {
                // eq. /boot/efi (vfat) will skipped
                continue;
            }
        } else {
            // fallback if USER is not set:
            if (!mountPoint.startsWith("/media/") &&
                !mountPoint.startsWith("/run/media/"))
            {
                continue;
            }
        }

        pts << mountPoint;
    }

    pts.removeDuplicates();
    pts.sort();
    return pts;
}

void MainWindow::createMountsToolbar()
{
    m_mountsToolBar = addToolBar(tr("Mounts"));
    m_mountsToolBar->setMovable(true);

    QStringList pts = listMountPoints();
    for (const QString& mp : pts) {
        QAction* act = new QAction(mp, m_mountsToolBar);

        connect(act, &QAction::triggered, this, [this, mp]() {
            FilePanel* panel = currentFilePanel();
            if (!panel)
                return;

            panel->currentPath = mp;
            panel->loadDirectory();
        });

        m_mountsToolBar->addAction(act);
    }
}

void MainWindow::copyFromPanel(FilePanel* srcPanel)
{
    if (!srcPanel)
        return;

    QModelIndex currentIndex = srcPanel->currentIndex();
    if (!currentIndex.isValid())
        return;

    QStandardItem* item = srcPanel->model->item(currentIndex.row(), COLUMN_NAME);
    if (!item)
        return;

    const QString fullName = item->data(Qt::UserRole).toString();
    if (fullName.isEmpty())
        return; // np. [..]

    QDir srcDir(srcPanel->currentPath);
    const QString srcPath = srcDir.absoluteFilePath(fullName);
    QFileInfo srcInfo(srcPath);

    // ustalenie panelu docelowego
    Side srcSide = srcPanel->side();
    FilePanel* dstPanel = nullptr;
    QString targetDir;
    Side dstSide = opposite(srcSide);
    dstPanel = filePanelForSide(dstSide);
    if (dstPanel)
        targetDir = dstPanel->currentPath;
    QString suggested;
    if (!targetDir.isEmpty()) {
        QDir dstDir(targetDir);
        suggested = dstDir.filePath(fullName);
    } else {
        suggested = srcPath; // fallback
    }

    bool ok = false;
    QString destInput = QInputDialog::getText(
        this,
        tr("Copy"),
        tr("Copy to:"),
        QLineEdit::Normal,
        suggested,
        &ok
    );

    if (!ok || destInput.isEmpty())
        return;

    QString baseDirForRelative;
    if (!targetDir.isEmpty())
        baseDirForRelative = targetDir;
    else
        baseDirForRelative = srcPanel->currentPath;

    QString dstPath;
    if (QDir::isAbsolutePath(destInput)) {
        dstPath = destInput;
    } else {
        QDir dstDir(baseDirForRelative);
        dstPath = dstDir.absoluteFilePath(destInput);
    }

    QFileInfo dstInfo(dstPath);

    // plik źródłowy
    if (srcInfo.isFile()) {
        // sprawdzamy, czy istnieje cel
        if (dstInfo.exists()) {
            auto reply = QMessageBox::question(
                this,
                tr("Overwrite"),
                tr("File '%1' already exists.\nOverwrite?")
                    .arg(dstInfo.fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (reply != QMessageBox::Yes)
                return;

            QFile::remove(dstPath);
        }

        if (!QFile::copy(srcPath, dstPath)) {
            QMessageBox::warning(
                this,
                tr("Error"),
                tr("Failed to copy:\n%1\nto\n%2")
                    .arg(srcPath, dstPath)
            );
            return;
        }

        if (dstPanel) {
            QDir dstPanelDir(dstPanel->currentPath);
            if (dstPanelDir.absoluteFilePath(QFileInfo(dstPath).fileName()) == dstPath) {
                dstPanel->loadDirectory();
                dstPanel->selectEntryByName(QFileInfo(dstPath).fileName());
            }
        }

        return;
    }

    // katalog źródłowy
    if (srcInfo.isDir()) {
        // dstRoot = katalog docelowy (może być nowy);
        // jeśli path wskazuje istniejący katalog, kopiujemy DO niego (tworząc podkatalog o tej samej nazwie)
        QString dstRoot = dstPath;
        if (dstInfo.exists() && dstInfo.isDir()) {
            QDir base(dstPath);
            dstRoot = base.filePath(srcInfo.fileName());
        }

        // na razie NIE wspieramy kopiowania do istniejącego, niepustego katalogu docelowego
        QDir checkDir(dstRoot);
        if (checkDir.exists()) {
            const QStringList entries = checkDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
            if (!entries.isEmpty()) {
                QMessageBox::warning(
                    this,
                    tr("Copy"),
                    tr("Destination directory '%1' already exists and is not empty.\n"
                       "Recursive copy into non-empty directories is not supported yet.")
                        .arg(dstRoot)
                );
                return;
            }
        }

        FilePanel::CopyStats stats;
        bool statsOk = false;
        FilePanel::collectCopyStats(srcPath, stats, statsOk);
        if (!statsOk || stats.totalFiles == 0) {
            QMessageBox::warning(
                this,
                tr("Copy"),
                tr("No files to copy in '%1'.").arg(srcPath)
            );
            return;
        }

        QProgressDialog progress(
            tr("Copying %1 files (%2 bytes)...")
                .arg(stats.totalFiles)
                .arg(static_cast<qulonglong>(stats.totalBytes)),
            tr("Cancel"),
            0,
            static_cast<int>(qMin<quint64>(stats.totalBytes, std::numeric_limits<int>::max())),
            this
        );
        progress.setWindowModality(Qt::ApplicationModal);
        progress.setMinimumDuration(0);
        progress.show();

        quint64 bytesCopied = 0;
        bool userAbort = false;

        const bool okCopy =
            FilePanel::copyDirectoryRecursive(srcPath, dstRoot, stats, progress, bytesCopied, userAbort);

        if (!okCopy && userAbort) {
            return;
        } else if (!okCopy) {
            // błąd – komunikaty zostały już pokazane w copyDirectoryRecursive
            return;
        }

        // sukces – odśwież panel docelowy
        if (dstPanel) {
            dstPanel->loadDirectory();
            dstPanel->selectEntryByName(QFileInfo(dstRoot).fileName());
        }

        return;
    }

    // inne typy – na razie ignorujemy
}

bool MainWindow::handle(const char* handler, QKeyEvent* ev) {
    bool handlerResult = true;
    QMetaObject::invokeMethod(
    this,
    handler,
    Q_RETURN_ARG(bool, handlerResult),
    Q_ARG(QKeyEvent*, ev)
    );
    return handlerResult;
}

QString MainWindow::currentPanelName() {
    QModelIndex currentIndex = currentFilePanel()->currentIndex();
    if (!currentIndex.isValid()) {
        return QString{};
    }
    return currentFilePanel()->getRowName(currentIndex.row());
}

QString MainWindow::currentPanelPath() {
    auto name = currentPanelName();
    if (name.isEmpty())
        return currentFilePanel()->currentPath;
    QDir dir(currentFilePanel()->currentPath);
    return dir.absoluteFilePath(name);
}

void MainWindow::updateCurrentPathLabel() {
    if (!currentPathLabel)
        return;

    FilePanel* panel = currentFilePanel();
    if (!panel) {
        currentPathLabel->clear();
        return;
    }

    QString path = panel->currentPath;
    QFontMetrics fm(currentPathLabel->font());
    int availableWidth = currentPathLabel->width() - 12;  // accounting for padding

    // Elide text if too long
    QString displayPath = fm.elidedText(path, Qt::ElideLeft, availableWidth);
    currentPathLabel->setText(displayPath);
}

#include "MainWindow_impl.inc"
