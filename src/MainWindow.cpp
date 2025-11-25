#include "MainWindow.h"

#include "Config.h"
#include "FilePaneWidget.h"
#include "FilePanel.h"

#include "editor/EditorFrame.h"
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <QHeaderView>
#include <QTimer>
#include <QActionGroup>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QProcess>
#include <QStandardPaths>
#include <QMessageBox>
#include <QStorageInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString cfg = Config::instance().defaultConfigPath();
    Config::instance().load(cfg);
    Config::instance().setConfigPath(cfg);

    setupUi();

    for (auto* panel : allFilePanels())
        panel->loadDirectory();

    setWindowTitle("Gemini Commander");
    resize(1024, 768);
    qApp->installEventFilter(this);
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    auto* splitter = new QSplitter(Qt::Horizontal, centralWidget);

    m_leftTabs = new QTabWidget(splitter);
    m_rightTabs = new QTabWidget(splitter);
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

    commandLineEdit = new QLineEdit(centralWidget);

    auto* leftPane  = new FilePaneWidget(m_leftTabs);
    auto* rightPane = new FilePaneWidget(m_rightTabs);
    m_leftTabs->addTab(leftPane,  "Left");
    m_rightTabs->addTab(rightPane, "Right");

    mainLayout->addWidget(splitter);
    mainLayout->addWidget(commandLineEdit);
    mainLayout->setStretchFactor(splitter, 1);
    mainLayout->setStretchFactor(commandLineEdit, 0);

    setCentralWidget(centralWidget);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));

    // Na razie tylko proste akcje, można rozwinąć później
    QAction* quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAction);

    // --- TOOLBAR ---
    m_mainToolBar = addToolBar(tr("Main toolbar"));
    m_mainToolBar->setMovable(true);

    m_openTerminalAction = new QAction(tr("Terminal"), this);
    // Ikonka później: m_openTerminalAction->setIcon(QIcon(":/icons/terminal.svg"));
    connect(m_openTerminalAction, &QAction::triggered,
            this, &MainWindow::onOpenTerminal);

    m_mainToolBar->addAction(m_openTerminalAction);

    addToolBarBreak(Qt::TopToolBarArea);
    createMountsToolbar();

    m_activeSide = LeftSide;
    if (auto* left = filePanelForSide(LeftSide))
        left->active(true);
    if (auto* right = filePanelForSide(RightSide))
        right->active(false);

    QTimer::singleShot(0, this, [this]() {
    setActiveSide(LeftSide);
    });
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
        // we check if it's a file event (viewport or FilePanel itself)
        if (auto* panel = panelForObject(obj)) {

            // CTRL + DOWN
            if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_Down) {
                panel->jumpWithControl(+1);
                return true;
            }

            // CTRL + UP
            if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_Up) {
                panel->jumpWithControl(-1);
                return true;
            }
        }
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
            if (auto* panel = panelForObject(obj)) {
                goToPreviousTab(tabsForSide(m_activeSide));
                return true;
            }
            }

        if (keyEvent->key() == Qt::Key_Tab) {
            auto view = dynamic_cast<QTableView*> (obj);
            if (view) {
                m_activeSide = 1 - m_activeSide;
                currentFilePanel()->setFocus();
                return true; // Event handled
            }
        } else if ((keyEvent->key() == Qt::Key_F3||keyEvent->key() == Qt::Key_F4) && modifiers == Qt::NoModifier) {
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

            if (!editorFrame)
                editorFrame = new EditorFrame(this);

            if (keyEvent->key() == Qt::Key_F3)
                editorFrame->openFileInViewer(fullPath);
            else
                editorFrame->openFileInEditor(fullPath);

            editorFrame->show();
            editorFrame->raise();
            editorFrame->activateWindow();

            return true;
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_D) {
            showFavoriteDirsMenu(m_activeSide);
            return true;
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F3) {
            FilePanel* panel =  currentFilePanel();
            if (panel->sortColumn == COLUMN_NAME) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_NAME;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->addAllEntries();
            return true;

        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F4) {
            FilePanel* panel =  currentFilePanel();
            if (panel->sortColumn == COLUMN_EXT) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_EXT;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->addAllEntries();
            return true;

        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F6) {
            FilePanel* panel =  currentFilePanel();
            if (panel->sortColumn == COLUMN_SIZE) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_SIZE;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->addAllEntries();
            return true;

        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F5) {
            FilePanel* panel = currentFilePanel();
            if (panel->sortColumn == COLUMN_DATE) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_DATE;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->addAllEntries();
            return true;

        } else if (modifiers == Qt::NoModifier && keyEvent->key() == Qt::Key_F7) {
            if (auto* panel = panelForObject(obj)) {
                panel->createNewDirectory(this);
                return true;
            }
        }  else if ((keyEvent->key() == Qt::Key_F8 || keyEvent->key() == Qt::Key_Delete)
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
            bool empty = d.isEmpty();

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        ok = QFile::moveToTrash(fullPath);
#else
        // fallback gdyby moveToTrash nie było dostępne
        if (info.isDir()) {
            QDir d(fullPath);
            ok = d.removeRecursively();
        } else {
            ok = QFile::remove(fullPath);
        }
#endif
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
            commandLineEdit->setFocus();
            commandLineEdit->selectAll();
            return true; // Event handled
        } else if (obj==commandLineEdit && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) && modifiers == Qt::NoModifier) {
            currentFilePanel()->setFocus();
            return true; // Event handled
        }
        else if (keyEvent->key() == Qt::Key_Home && modifiers == Qt::NoModifier) {
            auto *panel = currentFilePanel();
            int rows = panel->model->rowCount();
            if (rows > 0) {
                QModelIndex idx = panel->model->index(0, COLUMN_NAME);
                panel->setCurrentIndex(idx);
                panel->scrollTo(idx, QAbstractItemView::PositionAtTop);
            }
            return true;

        } else if (keyEvent->key() == Qt::Key_End && modifiers == Qt::NoModifier) {
            auto *panel = currentFilePanel();
            int rows = panel->model->rowCount();
            if (rows > 0) {
                QModelIndex idx = panel->model->index(rows - 1, COLUMN_NAME);
                panel->setCurrentIndex(idx);
                panel->scrollTo(idx, QAbstractItemView::PositionAtBottom);
            }
            return true;
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_PageUp) {
            FilePanel* panel = currentFilePanel();
            QDir dir(panel->currentPath);

            // If we are in the root directory – do nothing
            if (dir.isRoot()) {
                return true;
            }

            // Remember the name of the current directory (as in onPanelActivated)
            QString selectedName = dir.dirName();

            // Go up one directory
            dir.cdUp();
            panel->currentPath = dir.absolutePath();

            // Reload panel content
            panel->loadDirectory();
            panel->selectEntryByName(selectedName);

            return true;
        } else if (keyEvent->key() == Qt::Key_P && modifiers == Qt::ControlModifier) {
            // Ctrl + P: Set current directory to commandLineEdit
            QString currentPath = currentFilePanel()->currentPath;
            commandLineEdit->setText(commandLineEdit->text() + currentPath);
            return true; // Event handled
        } else if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
            if (modifiers == Qt::ControlModifier) {
                // Ctrl + Enter: Set selected item name to commandLineEdit
                QModelIndex currentIndex = currentFilePanel()->currentIndex();
                if (currentIndex.isValid()) {
                    QString name = currentFilePanel()->model->data(currentIndex.sibling(currentIndex.row(), 0)).toString();
                    commandLineEdit->setText(commandLineEdit->text() + name);
                    return true; // Event handled
                }
            } else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
                // Shift + Ctrl + Enter: Set full path of selected item to commandLineEdit
                QModelIndex currentIndex = currentFilePanel()->currentIndex();
                if (currentIndex.isValid()) {
                    QString name = currentFilePanel()->model->data(currentIndex.sibling(currentIndex.row(), 0)).toString();
                    if (name == "[..]") {
                        name = "..";
                    }
                    QDir dir(currentFilePanel()->currentPath);
                    QString fullPath = dir.absoluteFilePath(name);
                    commandLineEdit->setText(commandLineEdit->text() + fullPath);
                    return true; // Event handled
                }
            }
            // Plain Enter: Let default handling (activated signal) proceed
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_T) {
            // Ctrl+T: duplicate current tab on active side
            QTabWidget* tabs = tabsForSide(m_activeSide);
            FilePaneWidget* pane = currentPane();
            if (!tabs || !pane)
                return true;

            const QString path = pane->currentPath();

            auto* newPane = new FilePaneWidget(tabs);
            // duplicate the same folder
            newPane->setCurrentPath(path);

            const int insertIndex = tabs->currentIndex() + 1;
            const int newIndex = tabs->insertTab(insertIndex, newPane, tabs->tabText(tabs->currentIndex()));

            tabs->setCurrentIndex(newIndex);
            setActiveSide(m_activeSide); //ensure, focus and style are consistent
            if (auto* fp = newPane->filePanel())
                fp->setFocus();

            return true;
        }
    } else if (event->type() == QEvent::FocusIn) {
        if (auto* panel = panelForObject(obj)) {
            int side = sideForPanel(panel);
            if (side != -1) {
                // dezaktywuj poprzedni side, jeśli chcesz mieć tylko jeden aktywny „na niebiesko”
                if (side != m_activeSide) {
                    if (auto* prev = currentFilePanel())
                        prev->active(false);
                    m_activeSide = side;
                }

                panel->active(true);
            }
        }
    }
    else if (event->type() == QEvent::FocusOut) {
        if (auto* panel = panelForObject(obj)) {
            // prosto: panel traci focus → nieaktywny
            panel->active(false);
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::showFavoriteDirsMenu(int side)
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


FilePaneWidget* MainWindow::paneForSide(int side) const
{
    QTabWidget* tabs = nullptr;
    if (side == LeftSide)
        tabs = m_leftTabs;
    else
        tabs = m_rightTabs;

    if (!tabs || tabs->count() == 0)
        return nullptr;

    return qobject_cast<FilePaneWidget*>(tabs->currentWidget());
}

FilePanel* MainWindow::filePanelForSide(int side) const
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

void MainWindow::setActiveSide(int side)
{
    if (side != LeftSide && side != RightSide)
        return;
    m_activeSide = side;

    if (auto* panel = currentFilePanel())
        panel->setFocus();
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

int MainWindow::sideForPanel(FilePanel* panel) const
{
    if (!panel)
        return -1;

    QWidget* w = panel;
    QTabWidget* tabs = nullptr;

    // idziemy w górę po parentach, aż znajdziemy QTabWidget
    while (w) {
        if (auto* tw = qobject_cast<QTabWidget*>(w)) {
            tabs = tw;
            break;
        }
        w = w->parentWidget();
    }

    if (!tabs)
        return -1;

    if (tabs == m_leftTabs)
        return LeftSide;
    if (tabs == m_rightTabs)
        return RightSide;

    return -1;
}

QTabWidget* MainWindow::tabsForSide(int side) const
{
    if (side == LeftSide)
        return m_leftTabs;
    if (side == RightSide)
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
    auto* tb = addToolBar(tr("Mounts"));
    tb->setMovable(true);

    QStringList pts = listMountPoints();
    for (const QString& mp : pts) {
        QAction* act = new QAction(mp, tb);

        connect(act, &QAction::triggered, this, [this, mp]() {
            FilePanel* panel = currentFilePanel();
            if (!panel)
                return;

            panel->currentPath = mp;
            panel->loadDirectory();
        });

        tb->addAction(act);
    }
}
