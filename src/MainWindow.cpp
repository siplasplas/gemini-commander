#include "MainWindow.h"

#include "Config.h"
#include "ConfigDialog.h"
#include "FilePaneWidget.h"
#include "FilePanel.h"
#include "widgets/mrutabwidget.h"

#include "editor/EditorFrame.h"
#include "editor/editor.h"
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
#include <QGuiApplication>
#include <QStorageInfo>
#include <QFileDialog>
#include <QToolButton>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QClipboard>

#include "SortedDirIterator.h"
#include "SearchDialog.h"
#include "DistroInfo.h"
#include "DistroInfoDialog.h"
#include "FunctionBar.h"
#include "VerticalToolButton.h"
#include "editor/ViewerFrame.h"
#include "keys/KeyRouter.h"
#include "keys/ObjectRegistry.h"
#include "quitls.h"
#include "FileOperationProgressDialog.h"
#include "FileOperations.h"
#ifndef _WIN32
#include "udisks/UDisksDeviceManager.h"
#endif
#include <kcoreaddons_version.h>
#include "git_version.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    ObjectRegistry::add(this, "MainFrame");
    QString cfg = Config::instance().defaultConfigPath();
    Config::instance().load(cfg);
    Config::instance().setConfigPath(cfg);

#ifndef _WIN32
    // Initialize UDisks2 BEFORE setupUi() so mounts toolbar can be populated
    m_udisksManager = new UDisksDeviceManager(this);

    connect(m_udisksManager, &UDisksDeviceManager::deviceAdded,
            this, [this](const BlockDeviceInfo &) {
                refreshMountsToolbar();
                refreshProcMountsToolbar();
            });

    connect(m_udisksManager, &UDisksDeviceManager::deviceRemoved,
            this, [this](const QString &, const QString &) {
                refreshMountsToolbar();
                refreshProcMountsToolbar();
            });

    connect(m_udisksManager, &UDisksDeviceManager::deviceMounted,
            this, &MainWindow::onDeviceMounted);

    connect(m_udisksManager, &UDisksDeviceManager::deviceUnmounted,
            this, &MainWindow::onDeviceUnmounted);

    connect(m_udisksManager, &UDisksDeviceManager::errorOccurred,
            this, [](const QString &operation, const QString &errorMessage) {
                qWarning() << "UDisks error during" << operation << ":" << errorMessage;
            });

    if (!m_udisksManager->start()) {
        qWarning() << "Failed to start UDisks device manager - mounts toolbar will be empty";
    }
    // Initialize ProcMountsManager for /proc/mounts monitoring
    m_procMountsManager = new ProcMountsManager(this);

    connect(m_procMountsManager, &ProcMountsManager::mountsChanged,
            this, &MainWindow::refreshProcMountsToolbar);

    if (!m_procMountsManager->start()) {
        qWarning() << "Failed to start ProcMountsManager";
    }
#endif

    setupUi();

    // Connect signals for all panels
    for (auto* panel : allFilePanels()) {
        panel->styleInactive();
        // Connect directoryChanged to update the label
        connect(panel, &FilePanel::directoryChanged,
                this, &MainWindow::updateCurrentPathLabel);
        // Connect directoryChanged to update storage info toolbar
        connect(panel, &FilePanel::directoryChanged,
                this, &MainWindow::updateStorageInfoToolbar);
    }

    // Lazy loading: only load active tabs (2 panels instead of all 8)
    FilePanel* leftPanel = filePanelForSide(Side::Left);
    FilePanel* rightPanel = filePanelForSide(Side::Right);
    if (leftPanel) {
        leftPanel->loadDirectory();
        leftPanel->selectFirstEntry();
    }
    if (rightPanel) {
        rightPanel->loadDirectory();
        rightPanel->selectFirstEntry();
    }

    if (leftPanel)
        leftPanel->setFocus();

    updateCurrentPathLabel();  // initial update
    updateStorageInfoToolbar();  // initial update

    this->setStyleSheet(
        "QMainWindow {"
        "  padding: 0px 0px;"
        "}"
    );

    setWindowTitle("Gemini Commander");
    setWindowIcon(QIcon(":/icons/gemini-commander.svg"));

    // Apply window geometry from config
    applyConfigGeometry();

    keyMap.load(":/config/keys.toml");
    KeyRouter::instance().setKeyMap(&keyMap);
    KeyRouter::instance().installOn(qApp, this);

    // Directory monitoring
    m_dirWatcher = new QFileSystemWatcher(this);
    connect(m_dirWatcher, &QFileSystemWatcher::directoryChanged,
            this, &MainWindow::onDirectoryChanged);

    // Debounce timer for directory changes (500ms)
    m_dirChangeDebounceTimer = new QTimer(this);
    m_dirChangeDebounceTimer->setSingleShot(true);
    m_dirChangeDebounceTimer->setInterval(350);
    connect(m_dirChangeDebounceTimer, &QTimer::timeout,
            this, &MainWindow::processPendingDirChanges);

    // Update watched dirs when panels change directory
    for (auto* panel : allFilePanels()) {
        connect(panel, &FilePanel::directoryChanged,
                this, &MainWindow::updateWatchedDirectories);
    }
    updateWatchedDirectories();

    // File monitoring (visible files only)
    m_leftFileWatcher = new QFileSystemWatcher(this);
    m_rightFileWatcher = new QFileSystemWatcher(this);
    connect(m_leftFileWatcher, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::onLeftFileChanged);
    connect(m_rightFileWatcher, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::onRightFileChanged);

    // Connect visible files signals from panels
    for (auto* panel : allFilePanels()) {
        connect(panel, &FilePanel::visibleFilesChanged,
                this, &MainWindow::onVisibleFilesChanged);
    }

    // Update storage info toolbar and reload directory on tab changes
    connect(m_leftTabs, &QTabWidget::currentChanged, this, [this](int index) {
        updateStorageInfoToolbar();
        if (auto* pane = qobject_cast<FilePaneWidget*>(m_leftTabs->widget(index))) {
            pane->filePanel()->doRefresh(pane->filePanel(), nullptr);
            QTimer::singleShot(10, pane->filePanel(), [pane] {
                pane->filePanel()->setFocus();
                pane->pathEdit()->setSelection(0,0);
            });
        }
    });
    connect(m_rightTabs, &QTabWidget::currentChanged, this, [this](int index) {
        updateStorageInfoToolbar();
        if (auto* pane = qobject_cast<FilePaneWidget*>(m_rightTabs->widget(index))) {
            pane->filePanel()->doRefresh(pane->filePanel(), nullptr);
            QTimer::singleShot(10, pane->filePanel(), [pane] {
                pane->filePanel()->setFocus();
                pane->pathEdit()->setSelection(0,0);
            });
        }
    });
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    // Mark geometry as dirty when user interactively resizes
    // This prevents overwriting config values that couldn't be applied (e.g., on Wayland)
    m_geometryDirty = true;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // If editor is visible, close it first instead of closing the app
    if (editorFrame && editorFrame->isVisible()) {
        editorFrame->close();
        event->ignore();
        return;
    }

    // Save panel settings (sorting and column proportions)
    auto& cfg = Config::instance();
    if (auto* leftPanel = filePanelForSide(Side::Left)) {
        cfg.setLeftSort(leftPanel->sortColumn, static_cast<int>(leftPanel->sortOrder));
        cfg.setLeftPanelColumns(leftPanel->columns(), leftPanel->columnProportions());
    }
    if (auto* rightPanel = filePanelForSide(Side::Right)) {
        cfg.setRightSort(rightPanel->sortColumn, static_cast<int>(rightPanel->sortOrder));
        cfg.setRightPanelColumns(rightPanel->columns(), rightPanel->columnProportions());
    }

    // Only save window geometry if user interactively resized the window
    // This prevents overwriting config values that couldn't be applied (e.g., on Wayland)
    if (m_geometryDirty) {
        QRect frame = frameGeometry();
        int winX = frame.x();
        int winY = frame.y();

        // On Wayland, window positions are not reliable - don't save them
        // On X11 (xcb), negative positions are valid (e.g., monitor to the left)
        bool isWayland = QGuiApplication::platformName() == QLatin1String("wayland");
        if (isWayland) {
            winX = -1;  // Mark as "not set" - Wayland doesn't support positioning
            winY = -1;
        }
        cfg.setWindowGeometry(winX, winY, width(), height());
    }

    // Save tab directories (with deduplication)
    auto saveTabsForSide = [](QTabWidget* tabWidget) -> std::pair<QStringList, int> {
        QStringList dirs;
        int selectedIndex = tabWidget->currentIndex();
        QString selectedDir;

        // Get selected directory before deduplication
        if (selectedIndex >= 0 && selectedIndex < tabWidget->count()) {
            if (auto* pane = qobject_cast<FilePaneWidget*>(tabWidget->widget(selectedIndex))) {
                selectedDir = QDir::cleanPath(pane->filePanel()->currentPath);
            }
        }

        // Collect all directories
        for (int i = 0; i < tabWidget->count(); ++i) {
            if (auto* pane = qobject_cast<FilePaneWidget*>(tabWidget->widget(i))) {
                dirs.append(pane->filePanel()->currentPath);
            }
        }

        // Remove duplicates, keeping first occurrence
        QStringList uniqueDirs;
        QSet<QString> seen;
        for (const QString& dir : dirs) {
            QString clean = QDir::cleanPath(dir);
            if (!seen.contains(clean)) {
                seen.insert(clean);
                uniqueDirs.append(clean);
            }
        }

        // Find new index for selected directory after deduplication
        int newIndex = 0;
        if (!selectedDir.isEmpty()) {
            int idx = uniqueDirs.indexOf(selectedDir);
            if (idx >= 0)
                newIndex = idx;
        }

        return {uniqueDirs, newIndex};
    };

    auto [leftDirs, leftIndex] = saveTabsForSide(m_leftTabs);
    auto [rightDirs, rightIndex] = saveTabsForSide(m_rightTabs);
    cfg.setLeftTabs(leftDirs, leftIndex);
    cfg.setRightTabs(rightDirs, rightIndex);

    // Save current toolbar positions (user may have dragged them)
    auto qtAreaToToolbarArea = [](Qt::ToolBarArea qtArea) -> ToolbarArea {
        switch (qtArea) {
            case Qt::TopToolBarArea: return ToolbarArea::Top;
            case Qt::BottomToolBarArea: return ToolbarArea::Bottom;
            case Qt::LeftToolBarArea: return ToolbarArea::Left;
            case Qt::RightToolBarArea: return ToolbarArea::Right;
            default: return ToolbarArea::Top;
        }
    };

    // Collect all toolbars with their current state
    struct ToolbarInfo {
        QString name;
        QToolBar* tb;
        ToolbarArea area;
        bool lineBreak;
        QPoint pos;
    };
    QVector<ToolbarInfo> toolbars;

    auto collectToolbar = [&](const QString& name, QToolBar* tb) {
        if (!tb) return;
        ToolbarInfo info;
        info.name = name;
        info.tb = tb;
        info.area = qtAreaToToolbarArea(toolBarArea(tb));
        info.lineBreak = toolBarBreak(tb);
        info.pos = tb->pos();
        toolbars.append(info);
    };

    collectToolbar("main", m_mainToolBar);
    collectToolbar("mounts", m_mountsToolBar);
    collectToolbar("other_mounts", m_procMountsToolBar);
    collectToolbar("storage_info", m_storageInfoToolBar);
    collectToolbar("function_bar", m_functionBarToolBar);

    // Sort by area, then by row (y for horizontal, x for vertical), then by position in row
    std::sort(toolbars.begin(), toolbars.end(), [](const ToolbarInfo& a, const ToolbarInfo& b) {
        if (a.area != b.area)
            return static_cast<int>(a.area) < static_cast<int>(b.area);
        // For top/bottom areas: sort by y (row), then x (position)
        // For left/right areas: sort by x (column), then y (position)
        bool horizontal = (a.area == ToolbarArea::Top || a.area == ToolbarArea::Bottom);
        if (horizontal) {
            if (a.pos.y() != b.pos.y())
                return a.pos.y() < b.pos.y();
            return a.pos.x() < b.pos.x();
        } else {
            if (a.pos.x() != b.pos.x())
                return a.pos.x() < b.pos.x();
            return a.pos.y() < b.pos.y();
        }
    });

    // Assign order numbers and save
    int order = 0;
    ToolbarArea lastArea = ToolbarArea::Top;
    for (const auto& info : toolbars) {
        if (info.area != lastArea) {
            order = 0;
            lastArea = info.area;
        }
        auto tcfg = cfg.toolbarConfig(info.name);
        tcfg.area = info.area;
        tcfg.visible = info.tb->isVisible();
        tcfg.lineBreak = info.lineBreak;
        tcfg.order = order++;
        cfg.setToolbarConfig(info.name, tcfg);
    }

    // Save menu visibility
    cfg.setMenuVisible(menuBar()->isVisible());

    // Always save config (at least sorting settings)
    cfg.save();

    if (!Config::instance().confirmExit()) {
        event->accept();
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    tr("Exit"),
                    tr("Exit Gemini Commander?"),
                    QMessageBox::Yes|QMessageBox::Cancel,
                    QMessageBox::Yes
                );
    if (reply == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::applyConfigGeometry(bool isStartup)
{
    const auto& cfg = Config::instance();
    bool isWayland = QGuiApplication::platformName() == QLatin1String("wayland");

    // On Wayland, the compositor blocks programmatic window resize for security reasons.
    // At startup we must set the size (it works for initial geometry).
    // After startup, calling resize() would be ignored anyway, but it would trigger
    // resizeEvent() which sets m_geometryDirty = true, overwriting user's config dialog changes.
    if (isStartup || !isWayland) {
        resize(cfg.windowWidth(), cfg.windowHeight());
        if (cfg.windowX() >= 0 && cfg.windowY() >= 0) {
            move(cfg.windowX(), cfg.windowY());
        }
    }
}

void MainWindow::onConfigSaved()
{
    // Reload config from file
    Config::instance().load(Config::instance().configPath());

    // Apply window geometry (not startup, so skip resize on Wayland)
    // NOTE: On Wayland, the compositor may block programmatic window enlargement
    // for security reasons. The new size will be applied on next app start.
    applyConfigGeometry(false);

    // Apply toolbar configuration (visibility, position, order)
    applyToolbarConfig();
    m_showFunctionBarAction->setChecked(Config::instance().showFunctionBar());

    // Update MRU tab limit
    int maxUnpinned = Config::instance().maxUnpinnedTabs();
    m_leftTabs->setTabLimit(maxUnpinned);
    m_rightTabs->setTabLimit(maxUnpinned);
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    auto* splitter = new QSplitter(Qt::Horizontal, centralWidget);

    m_leftTabs = new MruTabWidget(splitter);
    m_leftTabs->setTabLimit(Config::instance().maxUnpinnedTabs());
    m_rightTabs = new MruTabWidget(splitter);
    m_rightTabs->setTabLimit(Config::instance().maxUnpinnedTabs());

    // Set MRU tab limit from config
    int maxUnpinned = Config::instance().maxUnpinnedTabs();
    m_leftTabs->setTabLimit(maxUnpinned);
    m_rightTabs->setTabLimit(maxUnpinned);

    // File panels must always have at least one tab
    m_leftTabs->setMinimalTabCount(1);
    m_rightTabs->setMinimalTabCount(1);

    auto tuneTabBar = [](MruTabWidget* tabs) {
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

    // Helper: compute tab title from path (last component or "/" for root)
    auto tabTitleFromPath = [](const QString& path) -> QString {
        if (path == "/" || path.isEmpty())
            return "/";
        QFileInfo info(path);
        QString name = info.fileName();
        return name.isEmpty() ? path : name;
    };

    // Helper: remove duplicates from list, keeping first occurrence
    auto removeDuplicates = [](const QStringList& list) -> QStringList {
        QStringList result;
        QSet<QString> seen;
        for (const QString& item : list) {
            QString clean = QDir::cleanPath(item);
            if (!seen.contains(clean)) {
                seen.insert(clean);
                result.append(clean);
            }
        }
        return result;
    };

    // Helper: create tabs for a side
    auto createTabsForSide = [&](QTabWidget* tabWidget, Side side,
                                  const QStringList& dirs, int selectedIndex) {
        QStringList uniqueDirs = removeDuplicates(dirs);

        // If no dirs configured, use home directory
        if (uniqueDirs.isEmpty()) {
            uniqueDirs.append(QDir::homePath());
        }

        // Adjust selectedIndex after deduplication
        if (selectedIndex < 0 || selectedIndex >= uniqueDirs.size()) {
            selectedIndex = 0;
        }

        for (int i = 0; i < uniqueDirs.size(); ++i) {
            const QString& dir = uniqueDirs[i];
            auto* pane = new FilePaneWidget(side, tabWidget);

            // Set initial path (lazy loading - will try to load when activated)
            pane->filePanel()->currentPath = dir;

            QString title = tabTitleFromPath(dir);
            tabWidget->addTab(pane, title);

            // Connect favorites button signal
            connect(pane, &FilePaneWidget::favoritesRequested, this, [this, pane](const QPoint& pos) {
                showFavoriteDirsMenu(pane->filePanel()->side(), pos);
            });

            // Install event filter on path edit
            pane->pathEdit()->installEventFilter(this);
        }

        tabWidget->setCurrentIndex(selectedIndex);
    };

    // Create tabs from config
    auto& cfg = Config::instance();
    createTabsForSide(m_leftTabs, Side::Left, cfg.leftTabDirs(), cfg.leftTabIndex());
    createTabsForSide(m_rightTabs, Side::Right, cfg.rightTabDirs(), cfg.rightTabIndex());

    // Extend tab context menu with "Copy Dir" action
    auto extendTabMenu = [this](MruTabWidget* tabs, Side side) {
        connect(tabs, &MruTabWidget::tabContextMenuRequested, this, [this, tabs, side](int tabIndex, QMenu* menu) {
            Q_UNUSED(side)
            menu->addSeparator();
            QAction* copyDirAction = menu->addAction(tr("Copy Dir"));
            connect(copyDirAction, &QAction::triggered, [tabs, tabIndex]() {
                if (auto* pane = qobject_cast<FilePaneWidget*>(tabs->widget(tabIndex))) {
                    QString path = pane->filePanel()->currentPath;
                    QClipboard* clipboard = QGuiApplication::clipboard();
                    clipboard->setText(qEscapePathForShell(path));
                }
            });
        });
    };
    extendTabMenu(m_leftTabs, Side::Left);
    extendTabMenu(m_rightTabs, Side::Right);

    // Bottom line: currentPath label (3/4 of left panel) + command line (rest)
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    currentPathLabel = new QLabel(centralWidget);
    currentPathLabel->setStyleSheet(
        "QLabel {"
        "  padding: 0px 4px;"
        "}"
    );
    currentPathLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    currentPathLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    commandLineEdit = new QLineEdit(centralWidget);
    commandLineEdit->setStyleSheet("QLineEdit { background-color: white; }");
    commandLineEdit->installEventFilter(this);
    ObjectRegistry::add(commandLineEdit, "CommandLine");

    // Label: 3 units (3/4 of first panel), CommandLine: 5 units (1/4 + full second panel)
    bottomLayout->addWidget(currentPathLabel, 3);
    bottomLayout->addWidget(commandLineEdit, 5);

    // Function bar (wrapped in toolbar for docking support)
    m_functionBar = new FunctionBar(this);
    connect(m_functionBar, &FunctionBar::viewClicked, this, [this]() {
        doView(nullptr, nullptr);
    });
    connect(m_functionBar, &FunctionBar::editClicked, this, [this]() {
        doEdit(nullptr, nullptr);
    });
    connect(m_functionBar, &FunctionBar::copyClicked, this, [this]() {
        doCopy(nullptr, nullptr);
    });
    connect(m_functionBar, &FunctionBar::moveClicked, this, [this]() {
        doMove(nullptr, nullptr);
    });
    connect(m_functionBar, &FunctionBar::mkdirClicked, this, [this]() {
        doMakeDirectory(nullptr, nullptr);
    });
    connect(m_functionBar, &FunctionBar::deleteClicked, this, [this]() {
        doDeleteToTrash(nullptr, nullptr);
    });
    connect(m_functionBar, &FunctionBar::terminalClicked, this, &MainWindow::onOpenTerminal);
    connect(m_functionBar, &FunctionBar::exitClicked, this, &QWidget::close);

    mainLayout->addWidget(splitter);
    mainLayout->addLayout(bottomLayout);
    mainLayout->setStretchFactor(splitter, 1);

    setCentralWidget(centralWidget);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QMenu* markMenu = menuBar()->addMenu(tr("&Mark"));
    QMenu* commandsMenu = menuBar()->addMenu(tr("&Commands"));
    QMenu* navigateMenu = menuBar()->addMenu(tr("&Navigate"));
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QMenu* configMenu = menuBar()->addMenu(tr("C&onfiguration"));
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));

    // Mark menu
    QAction* compareDirectoriesAction = new QAction(tr("Compare directories"), this);
    compareDirectoriesAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F2));
    connect(compareDirectoriesAction, &QAction::triggered, this, [this]() {
        doCompareDirectories(nullptr, nullptr);
    });
    markMenu->addAction(compareDirectoriesAction);

    // Help menu
    QAction* aboutAction = new QAction(tr("About"), this);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QString aboutText = tr(
            "<h3>Gemini Commander</h3>"
            "<p>Version: %1 (%2)</p>"
            "<p>Qt Version: %3</p>"
            "<p>KDE Frameworks Version: %4</p>"
        ).arg(APP_VERSION, GIT_SHA, qVersion(), KCOREADDONS_VERSION_STRING);
        QMessageBox::about(this, tr("About Gemini Commander"), aboutText);
    });
    helpMenu->addAction(aboutAction);

    // File menu
    QAction* packAction = new QAction(tr("Pack..."), this);
    packAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F5));
    connect(packAction, &QAction::triggered, this, [this]() {
        doPackFiles(nullptr, nullptr);
    });
    fileMenu->addAction(packAction);

    QAction* extractAction = new QAction(tr("Extract files..."), this);
    extractAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F9));
    connect(extractAction, &QAction::triggered, this, [this]() {
        doExtractFiles(nullptr, nullptr);
    });
    fileMenu->addAction(extractAction);

    QAction* multiRenameAction = new QAction(tr("Multi-&Rename Tool..."), this);
    multiRenameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(multiRenameAction, &QAction::triggered, this, [this]() {
        doMultiRename(nullptr, nullptr);
    });
    fileMenu->addAction(multiRenameAction);

    fileMenu->addSeparator();

    QAction* quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::fromString("Alt+F4"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAction);

    // Configuration menu
    QAction* settingsAction = new QAction(tr("Settings..."), this);
    connect(settingsAction, &QAction::triggered, this, [this]() {
        ConfigDialog dlg(this);
        connect(&dlg, &ConfigDialog::settingsApplied, this, [this]() {
            m_geometryDirty = false;
            onConfigSaved();
        });
        connect(&dlg, &ConfigDialog::sortingChanged, this, [this](int side, int column, int order) {
            FilePanel* panel = filePanelForSide(side == 0 ? Side::Left : Side::Right);
            if (panel) {
                // Convert column index to column name
                if (column >= 0 && column < panel->columns().size()) {
                    panel->sortColumn = panel->columns()[column];
                }
                panel->sortOrder = static_cast<Qt::SortOrder>(order);
                panel->sortEntriesApplyModel();
            }
        });
        connect(&dlg, &ConfigDialog::columnsChanged, this, [this](int side, const QStringList& columns, const QVector<double>& proportions) {
            FilePanel* panel = filePanelForSide(side == 0 ? Side::Left : Side::Right);
            if (panel) {
                panel->setColumns(columns, proportions);
            }
        });
        connect(&dlg, &ConfigDialog::toolbarResetRequested, this, &MainWindow::applyToolbarConfig);
        dlg.exec();
    });
    configMenu->addAction(settingsAction);

    QAction* editConfigAction = new QAction(tr("Change Settings Files Directly..."), this);
    connect(editConfigAction, &QAction::triggered, this, [this]() {
        QString configPath = Config::instance().configPath();
        if (!configPath.isEmpty()) {
            openEditorForFile(configPath);
        }
    });
    configMenu->addAction(editConfigAction);

    // Commands menu - Search action (no shortcut here, managed by KeyRouter/TOML)
    m_searchAction = new QAction(tr("Search files..."), this);
    m_searchAction->setIcon(QIcon(":/icons/search.svg"));
    connect(m_searchAction, &QAction::triggered, this, [this]() {
        doSearchGlobal(nullptr, nullptr);
    });
    commandsMenu->addAction(m_searchAction);

    // Commands menu - Run Terminal (F9 shortcut managed by KeyRouter/TOML)
    QAction* runTerminalAction = new QAction(tr("Run Terminal"), this);
    runTerminalAction->setIcon(QIcon(":/icons/terminal.svg"));
    runTerminalAction->setShortcut(QKeySequence(Qt::Key_F9));
    connect(runTerminalAction, &QAction::triggered, this, &MainWindow::onOpenTerminal);
    commandsMenu->addAction(runTerminalAction);

    // Commands menu - Calculate All Sizes
    QAction* calcAllSizesAction = new QAction(tr("Calculate All Sizes"), this);
    calcAllSizesAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Return));
    connect(calcAllSizesAction, &QAction::triggered, this, [this]() {
        FilePanel* panel = currentFilePanel();
        if (panel) {
            panel->doTotalSizes(nullptr, nullptr);
        }
    });
    commandsMenu->addAction(calcAllSizesAction);

    commandsMenu->addSeparator();

    QAction* distroInfoAction = new QAction(tr("Distribution Info..."), this);
    connect(distroInfoAction, &QAction::triggered, this, [this]() {
        DistroInfoDialog dlg(this);
        dlg.exec();
    });
    commandsMenu->addAction(distroInfoAction);

    // Navigate menu
    QAction* backAction = new QAction(tr("Back"), this);
    backAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    connect(backAction, &QAction::triggered, this, [this]() {
        FilePanel* panel = currentFilePanel();
        if (panel) {
            emit panel->goBackRequested();
        }
    });
    navigateMenu->addAction(backAction);

    QAction* forwardAction = new QAction(tr("Forward"), this);
    forwardAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    connect(forwardAction, &QAction::triggered, this, [this]() {
        FilePanel* panel = currentFilePanel();
        if (panel) {
            emit panel->goForwardRequested();
        }
    });
    navigateMenu->addAction(forwardAction);

    navigateMenu->addSeparator();

    QAction* goToHomeAction = new QAction(tr("Go to Home"), this);
    goToHomeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft));
    connect(goToHomeAction, &QAction::triggered, this, [this]() {
        doGoToHome(nullptr, nullptr);
    });
    navigateMenu->addAction(goToHomeAction);

    QAction* goToRootAction = new QAction(tr("Go to Root"), this);
    goToRootAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Backslash));
    connect(goToRootAction, &QAction::triggered, this, [this]() {
        doGoToRoot(nullptr, nullptr);
    });
    navigateMenu->addAction(goToRootAction);

    navigateMenu->addSeparator();

    QAction* leftFollowsRightAction = new QAction(tr("Left follows Right"), this);
    leftFollowsRightAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left));
    connect(leftFollowsRightAction, &QAction::triggered, this, [this]() {
        doFollowDirFromRight(nullptr, nullptr);
    });
    navigateMenu->addAction(leftFollowsRightAction);

    QAction* rightFollowsLeftAction = new QAction(tr("Right follows Left"), this);
    rightFollowsLeftAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));
    connect(rightFollowsLeftAction, &QAction::triggered, this, [this]() {
        doFollowDirFromLeft(nullptr, nullptr);
    });
    navigateMenu->addAction(rightFollowsLeftAction);

    // --- TOOLBAR ---
    m_mainToolBar = addToolBar(tr("Main toolbar"));
    m_mainToolBar->setMovable(true);
    m_mainToolBar->setFloatable(false);

    m_openTerminalAction = new QAction(tr("Terminal"), this);
    m_openTerminalAction->setIcon(QIcon(":/icons/terminal.svg"));
    connect(m_openTerminalAction, &QAction::triggered,
            this, &MainWindow::onOpenTerminal);

    m_mainToolBar->addAction(m_openTerminalAction);
    m_mainToolBar->addAction(m_searchAction);

    addToolBarBreak(Qt::TopToolBarArea);
    createMountsToolbar();
    createProcMountsToolbar();

    // Storage info toolbar (shows free/total space for active panel)
    m_storageInfoToolBar = addToolBar(tr("Storage Info"));
    m_storageInfoToolBar->setMovable(true);
    m_storageInfoToolBar->setFloatable(false);

    QSize icon16(16,16);
    m_mainToolBar->setIconSize(icon16);
    m_mountsToolBar->setIconSize(icon16);
    if (m_procMountsToolBar)
        m_procMountsToolBar->setIconSize(icon16);
    m_storageInfoToolBar->setIconSize(icon16);

    // Helper to set toolbar size constraints based on orientation
    auto setupToolbarOrientation = [](QToolBar* toolbar) {
        if (!toolbar) return;
        auto updateConstraints = [toolbar]() {
            QFontMetrics fm(toolbar->font());
            int size = fm.height() + 8;
            if (toolbar->orientation() == Qt::Horizontal) {
                toolbar->setFixedHeight(size);
                toolbar->setMaximumWidth(QWIDGETSIZE_MAX);
            } else {
                toolbar->setFixedWidth(size + 8);  // Extra width for vertical text
                toolbar->setMaximumHeight(QWIDGETSIZE_MAX);
            }
        };
        QObject::connect(toolbar, &QToolBar::orientationChanged, toolbar, updateConstraints);
        updateConstraints();  // Initial setup
    };

    setupToolbarOrientation(m_mainToolBar);
    setupToolbarOrientation(m_mountsToolBar);
    setupToolbarOrientation(m_procMountsToolBar);
    setupToolbarOrientation(m_storageInfoToolBar);

    // Function bar toolbar (at bottom)
    m_functionBarToolBar = new QToolBar(tr("Function Bar"), this);
    m_functionBarToolBar->setMovable(true);
    m_functionBarToolBar->setFloatable(false);
    m_functionBarToolBar->setObjectName("FunctionBarToolBar");
    m_functionBarToolBar->addWidget(m_functionBar);
    addToolBar(Qt::BottomToolBarArea, m_functionBarToolBar);

    // Connect orientation changes to FunctionBar
    connect(m_functionBarToolBar, &QToolBar::orientationChanged,
            m_functionBar, &FunctionBar::setOrientation);

    QString tbStyle =
        "QToolBar QToolButton { "
        "  padding: 0px; "
        "  margin: 0px; "
        "  min-height: 16px; "
        "} ";

    m_mainToolBar->setStyleSheet(tbStyle);
    m_mountsToolBar->setStyleSheet(tbStyle);

    // Shared context menu for toolbars and menu bar
    auto showToolbarContextMenu = [this](const QPoint& globalPos) {
        QMenu menu;

        // Menu bar visibility toggle (with safeguard)
        QAction* menuAction = menu.addAction(tr("Show Menu Bar"));
        menuAction->setCheckable(true);
        menuAction->setChecked(menuBar()->isVisible());
        // Disable if hiding menu would leave no way to restore (main toolbar hidden)
        if (menuBar()->isVisible() && !m_mainToolBar->isVisible()) {
            menuAction->setEnabled(false);
            menuAction->setToolTip(tr("Cannot hide - Main Toolbar must be visible first"));
        }
        connect(menuAction, &QAction::toggled, this, [this](bool checked) {
            // Safeguard: can't hide menu if main toolbar is hidden
            if (!checked && !m_mainToolBar->isVisible()) {
                return;
            }
            Config::instance().setMenuVisible(checked);
            menuBar()->setVisible(checked);
            Config::instance().save();
        });

        menu.addSeparator();

        // Toolbar visibility toggles
        auto addToolbarToggle = [&menu, this](const QString& label, const QString& configName, QToolBar* tb, bool isMainToolbar = false) {
            if (!tb) return;
            QAction* action = menu.addAction(label);
            action->setCheckable(true);
            action->setChecked(tb->isVisible());
            // Safeguard: can't hide main toolbar if menu is hidden
            if (isMainToolbar && tb->isVisible() && !menuBar()->isVisible()) {
                action->setEnabled(false);
                action->setToolTip(tr("Cannot hide - Menu Bar must be visible first"));
            }
            connect(action, &QAction::toggled, this, [this, configName, tb, isMainToolbar](bool checked) {
                // Safeguard for main toolbar
                if (isMainToolbar && !checked && !menuBar()->isVisible()) {
                    return;
                }
                auto cfg = Config::instance().toolbarConfig(configName);
                cfg.visible = checked;
                Config::instance().setToolbarConfig(configName, cfg);
                tb->setVisible(checked);
                Config::instance().save();
            });
        };

        addToolbarToggle(tr("Main Toolbar"), "main", m_mainToolBar, true);
        addToolbarToggle(tr("Mounts"), "mounts", m_mountsToolBar);
        if (m_procMountsToolBar)
            addToolbarToggle(tr("Other Mounts"), "other_mounts", m_procMountsToolBar);
        addToolbarToggle(tr("Storage Info"), "storage_info", m_storageInfoToolBar);
        addToolbarToggle(tr("Function Bar"), "function_bar", m_functionBarToolBar);

        menu.exec(globalPos);
    };

    auto setupToolbarContextMenu = [this, showToolbarContextMenu](QToolBar* toolbar) {
        if (!toolbar) return;
        toolbar->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(toolbar, &QToolBar::customContextMenuRequested, this, [toolbar, showToolbarContextMenu](const QPoint& pos) {
            showToolbarContextMenu(toolbar->mapToGlobal(pos));
        });
    };

    setupToolbarContextMenu(m_mainToolBar);
    setupToolbarContextMenu(m_mountsToolBar);
    // Note: m_procMountsToolBar has its own context menu for umount - don't override
    setupToolbarContextMenu(m_storageInfoToolBar);
    setupToolbarContextMenu(m_functionBarToolBar);

    // Context menu on menu bar itself (same as toolbar context menu)
    menuBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(menuBar(), &QMenuBar::customContextMenuRequested, this, [showToolbarContextMenu, this](const QPoint& pos) {
        showToolbarContextMenu(menuBar()->mapToGlobal(pos));
    });

    // View menu - Function Bar toggle
    m_showFunctionBarAction = new QAction(tr("Show Function Bar"), this);
    m_showFunctionBarAction->setCheckable(true);
    m_showFunctionBarAction->setChecked(Config::instance().showFunctionBar());
    connect(m_showFunctionBarAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            m_functionBarToolBar->show();
        } else {
            m_functionBarToolBar->hide();
        }
        Config::instance().setShowFunctionBar(checked);
        Config::instance().save();
    });
    viewMenu->addAction(m_showFunctionBarAction);

    // Apply toolbar configuration from config
    applyToolbarConfig();
}

Side MainWindow::opposite(Side side){
    return side==Side::Left?Side::Right:Side::Left;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Keyboard events are handled by KeyRouter - only focus events here
    if (event->type() == QEvent::FocusIn) {
        // Handle command line edit focus
        if (obj == commandLineEdit) {
            QTimer::singleShot(0, commandLineEdit, &QLineEdit::selectAll);
            return QMainWindow::eventFilter(obj, event);
        }

        // Handle path edit focus for both panes
        auto* leftPane = paneForSide(Side::Left);
        auto* rightPane = paneForSide(Side::Right);
        if (leftPane && obj == leftPane->pathEdit()) {
            QTimer::singleShot(0, leftPane->pathEdit(), &QLineEdit::selectAll);
            return QMainWindow::eventFilter(obj, event);
        }
        if (rightPane && obj == rightPane->pathEdit()) {
            QTimer::singleShot(0, rightPane->pathEdit(), &QLineEdit::selectAll);
            return QMainWindow::eventFilter(obj, event);
        }

        // Handle panel focus
        if (auto* panel = panelForObject(obj)) {
            Side newSide = panel->side();
            if (m_activeSide != newSide) {
                // Change side - visually clear the old panel
                if (auto* oldPanel = filePanelForSide(m_activeSide))
                    oldPanel->clearSelection();
                m_activeSide = newSide;
                updateStorageInfoToolbar();
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

void MainWindow::showFavoriteDirsMenu(Side side, const QPoint& pos)
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

    // Popup position: if pos is provided (from button), use it; otherwise center on panel (Ctrl+D)
    QPoint menuPos;
    if (pos.isNull()) {
        // Ctrl+D: show in center of panel
        menuPos = panel->mapToGlobal(QPoint(panel->width() / 2, panel->height()/2));
    } else {
        // Button click: show at provided position
        menuPos = pos;
    }
    QAction* chosen = menu.exec(menuPos);
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
    panel->selectFirstEntry();
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

MruTabWidget* MainWindow::tabsForSide(Side side) const
{
    if (side == Side::Left)
        return m_leftTabs;
    if (side == Side::Right)
        return m_rightTabs;
    return nullptr;
}

void MainWindow::goToNextTab(MruTabWidget* tabWidget) {
    int current = tabWidget->currentIndex();
    int count = tabWidget->count();

    if (current < count - 1) {
        tabWidget->setCurrentIndex(current + 1);
    } else {
        tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::goToPreviousTab(MruTabWidget* tabWidget) {
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

    // Użyj sugerowanego terminala dla aktualnego środowiska
    QString termCmd = DistroInfo::suggestedTerminal();

    if (QStandardPaths::findExecutable(termCmd).isEmpty()) {
        QString installCmd = DistroInfo::installCommand(termCmd);
        QString msg = tr("Terminal '%1' not found.").arg(termCmd);
        if (!installCmd.isEmpty()) {
            msg += tr("\n\nInstall command:\n%1").arg(installCmd);
        }

        QInputDialog dlg(this);
        dlg.setWindowTitle(tr("Terminal"));
        dlg.setLabelText(msg);
        dlg.setTextValue(installCmd);
        dlg.setOption(QInputDialog::UsePlainTextEditForTextInput, false);
        // Make the line edit read-only
        if (QLineEdit* le = dlg.findChild<QLineEdit*>()) {
            le->setReadOnly(true);
            le->selectAll();
        }
        dlg.exec();
        return;
    }

    QStringList args;
    QString program = termCmd;

    // Specjalne argumenty dla niektórych terminali
    if (termCmd == QLatin1String("gnome-terminal") ||
        termCmd == QLatin1String("xfce4-terminal") ||
        termCmd == QLatin1String("mate-terminal")) {
        args << QStringLiteral("--working-directory=%1").arg(workDir);
    } else if (termCmd == QLatin1String("konsole")) {
        args << QStringLiteral("--workdir") << workDir;
    } else if (termCmd == QLatin1String("wt")) {
        // Windows Terminal: -d sets the starting directory
        args << QStringLiteral("-d") << workDir;
    } else if (termCmd == QLatin1String("powershell")) {
        // PowerShell needs to be launched via cmd /c start to get a visible window
        program = QStringLiteral("cmd");
        args << QStringLiteral("/c") << QStringLiteral("start")
             << QStringLiteral("powershell") << QStringLiteral("-NoExit")
             << QStringLiteral("-Command") << QStringLiteral("cd '%1'").arg(workDir);
    }
    // dla xterm, qterminal i innych – użyjemy tylko workingDirectory w QProcess

    // Use startDetached to fully detach the process from our application
    if (!QProcess::startDetached(program, args, workDir)) {
        QMessageBox::warning(this,
                             tr("Terminal"),
                             tr("Failed to start terminal: %1").arg(termCmd));
    }
}

void MainWindow::createMountsToolbar()
{
    m_mountsToolBar = addToolBar(tr("Mounts"));
    m_mountsToolBar->setMovable(true);
    m_mountsToolBar->setFloatable(false);

    // Initial population will happen after UDisks manager starts
    // in the constructor
    refreshMountsToolbar();
}

void MainWindow::createNewDirectory(QWidget *dialogParent) {
    FilePanel* panel = currentFilePanel();
    QString suggestedName = panel->getCurrentRelPath();

    QWidget *parent = dialogParent ? dialogParent : this;

    QInputDialog dlg(parent);
    dlg.setWindowTitle(tr("Create new directory"));
    dlg.setLabelText(tr("Input new name:"));
    dlg.setTextValue(suggestedName);
    dlg.resize(500, dlg.sizeHint().height());

    if (dlg.exec() != QDialog::Accepted || dlg.textValue().isEmpty())
        return;

    QString name = dlg.textValue();
    QString fullPath = QDir(panel->currentPath).absoluteFilePath(name);

    if (!QDir().mkpath(fullPath)) {
        QMessageBox::warning(parent, tr("Error"), tr("Failed to create directory."));
        return;
    }
    selectPathAfterFileOperation(panel, nullptr, fullPath);
    //auto firstPart = name.section('/', 0, 0);
}

void MainWindow::selectNameAfterFileOperation(FilePanel *srcPanel, FilePanel *dstPanel, const QString& relativeName) {
    QString fullPath = QDir(srcPanel->currentPath).absoluteFilePath(relativeName);
    selectPathAfterFileOperation(srcPanel, dstPanel, fullPath);
}

void MainWindow::selectPathAfterFileOperation(FilePanel *srcPanel, FilePanel *dstPanel, const QString& selectedPath)
{
    // Suppress QFileSystemWatcher reload - we handle it ourselves
    m_suppressDirWatcher = true;

    srcPanel->loadDirectory();
    if (!selectedPath.isEmpty()) {
        QDir srcDir(srcPanel->currentPath);
        QString srcCanonical = QDir::cleanPath(srcPanel->currentPath);
        QString selCanonical = QDir::cleanPath(selectedPath);
        if (selCanonical.startsWith(srcCanonical + "/") || selCanonical == srcCanonical) {
            QString relPath = srcDir.relativeFilePath(selectedPath);
            srcPanel->selectEntryByRelPath(relPath);
            QTimer::singleShot(50, this, [this, srcPanel]() {
                m_suppressDirWatcher = false;
                srcPanel->setFocus();
                srcPanel->restoreSelectionFromMemory();
            });
            return;
        }
    }
    if (dstPanel) {
        dstPanel->loadDirectory();
        if (!selectedPath.isEmpty()) {
            QDir dstDir(dstPanel->currentPath);
            QString dstCanonical = QDir::cleanPath(dstPanel->currentPath);
            QString selCanonical = QDir::cleanPath(selectedPath);
            if (selCanonical.startsWith(dstCanonical + "/") || selCanonical == dstCanonical) {
                QString relPath = dstDir.relativeFilePath(selectedPath);
                dstPanel->selectEntryByRelPath(relPath);
                QTimer::singleShot(50, this, [this, dstPanel]() {
                    m_suppressDirWatcher = false;
                    dstPanel->setFocus();
                    dstPanel->restoreSelectionFromMemory();
                });
                return;
            }
        }
    }

    // If no selection was made, still re-enable watcher
    m_suppressDirWatcher = false;
}

FileOperations::Params MainWindow::askForFileOperation(FilePanel* srcPanel, bool inPlace, bool isMove)
{
    if (!srcPanel)
        return {};

    // Get destination panel info
    Side srcSide = srcPanel->side();
    Side dstSide = opposite(srcSide);
    FilePanel* dstPanel = filePanelForSide(dstSide);
    QString targetDir = dstPanel ? dstPanel->currentPath : srcPanel->currentPath;

    // Check for marked files
    QStringList markedNames = srcPanel->getMarkedNames();
    bool hasMarked = !markedNames.isEmpty();

    // Build suggested path and names list
    QString suggested;
    QStringList names;

    if (hasMarked)
        names = markedNames;
    else {
        QModelIndex currentIndex = srcPanel->currentIndex();
        if (!currentIndex.isValid())
            return {};

        QString currentName = srcPanel->getRowRelPath(currentIndex.row());
        if (currentName.isEmpty())
            return {}; // [..]
        names << currentName;
    }


    if (hasMarked && markedNames.size()>1) {
        suggested = inPlace ? QString() : targetDir;
    } else {
        QString currentName = srcPanel->getCurrentRelPath();
        if (currentName.isEmpty())
            return {}; // [..]

        if (hasMarked) {
            if (inPlace) {
                suggested = markedNames[0];
            } else {
                QDir dstDir(targetDir);
                suggested = dstDir.filePath(markedNames[0]);
            }
        }
        else {
            if (inPlace) {
                suggested = currentName;
            } else {
                QDir dstDir(targetDir);
                suggested = dstDir.filePath(currentName);
            }
        }
    }

    // Show dialog
    QString title = isMove ? tr("Move/Rename") : tr("Copy");
    QString label = isMove
        ? (hasMarked ? tr("Move %1 items to:").arg(markedNames.size()) : tr("Move to:"))
        : (hasMarked ? tr("Copy %1 items to:").arg(markedNames.size()) : tr("Copy to:"));

    QInputDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setLabelText(label);
    dlg.setTextValue(suggested);
    dlg.resize(500, dlg.sizeHint().height());

    if (dlg.exec() != QDialog::Accepted || dlg.textValue().isEmpty())
        return {};

    QString destInput = dlg.textValue();

    FileOperations::Params params;
    params.valid = true;
    params.srcPath = srcPanel->currentPath;
    params.names = names;
    params.destPath = destInput;
    return params;
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
    return currentFilePanel()->getRowRelPath(currentIndex.row());
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

void MainWindow::updateWatchedDirectories()
{
    if (!m_dirWatcher)
        return;

    // Get current directories from both panels
    QSet<QString> neededDirs;
    FilePanel* leftPanel = filePanelForSide(Side::Left);
    FilePanel* rightPanel = filePanelForSide(Side::Right);

    if (leftPanel && !leftPanel->currentPath.isEmpty())
        neededDirs.insert(leftPanel->currentPath);
    if (rightPanel && !rightPanel->currentPath.isEmpty())
        neededDirs.insert(rightPanel->currentPath);

    // Get currently watched directories
    QStringList currentlyWatched = m_dirWatcher->directories();
    QSet<QString> currentSet(currentlyWatched.begin(), currentlyWatched.end());

    // Remove directories no longer needed
    for (const QString& dir : currentlyWatched) {
        if (!neededDirs.contains(dir)) {
            m_dirWatcher->removePath(dir);
        }
    }

    // Add new directories
    for (const QString& dir : neededDirs) {
        if (!currentSet.contains(dir)) {
            m_dirWatcher->addPath(dir);
        }
    }
}

void MainWindow::onDirectoryChanged(const QString& path)
{
    // Skip if we're handling a file operation ourselves
    if (m_suppressDirWatcher)
        return;

    // Add path to pending set and restart debounce timer
    m_pendingDirChanges.insert(path);
    m_dirChangeDebounceTimer->start();

    // Re-add path to watcher (some systems like Linux inotify remove it after change)
    if (m_dirWatcher && QDir(path).exists()) {
        if (!m_dirWatcher->directories().contains(path)) {
            m_dirWatcher->addPath(path);
        }
    }
}

void MainWindow::processPendingDirChanges()
{
    // Take pending paths and clear the set
    QSet<QString> paths = m_pendingDirChanges;
    m_pendingDirChanges.clear();

    // Refresh panels showing these directories
    FilePanel* leftPanel = filePanelForSide(Side::Left);
    FilePanel* rightPanel = filePanelForSide(Side::Right);

    for (const QString& path : paths) {
        if (leftPanel && leftPanel->currentPath == path && !leftPanel->branchMode) {
            leftPanel->loadDirectory();
        }
        if (rightPanel && rightPanel->currentPath == path && !rightPanel->branchMode) {
            rightPanel->loadDirectory();
        }
    }
}

// ============================================================================
// File monitoring (visible files only)
// ============================================================================

void MainWindow::ensureFileWatcherActive(QFileSystemWatcher* watcher)
{
    // QFileSystemWatcher may stop working after file deletion
    // Re-create if needed (check by trying to add a known existing file)
    if (!watcher)
        return;

    // Simple check: if watcher has no files and no directories, it's still active
    // The watcher becomes inactive only in specific edge cases we handle by re-adding
}

void MainWindow::updateFileWatcher(Side side, const QStringList& paths)
{
    QFileSystemWatcher* watcher = (side == Side::Left) ? m_leftFileWatcher : m_rightFileWatcher;
    if (!watcher)
        return;

    // Get currently watched files
    QStringList currentlyWatched = watcher->files();
    QSet<QString> currentSet(currentlyWatched.begin(), currentlyWatched.end());
    QSet<QString> neededSet(paths.begin(), paths.end());

    // Remove files no longer visible
    for (const QString& file : currentlyWatched) {
        if (!neededSet.contains(file)) {
            watcher->removePath(file);
        }
    }

    // Add newly visible files
    for (const QString& file : paths) {
        if (!currentSet.contains(file)) {
            // Only add if file exists
            if (QFileInfo::exists(file)) {
                watcher->addPath(file);
            }
        }
    }
}

void MainWindow::onVisibleFilesChanged(Side side, const QStringList& paths)
{
    updateFileWatcher(side, paths);
}

void MainWindow::onLeftFileChanged(const QString& path)
{
    FilePanel* panel = filePanelForSide(Side::Left);
    if (panel) {
        panel->refreshEntryByPath(path);
    }

    // Re-add to watcher (inotify removes after change)
    if (m_leftFileWatcher && QFileInfo::exists(path)) {
        if (!m_leftFileWatcher->files().contains(path)) {
            m_leftFileWatcher->addPath(path);
        }
    }
}

void MainWindow::onRightFileChanged(const QString& path)
{
    FilePanel* panel = filePanelForSide(Side::Right);
    if (panel) {
        panel->refreshEntryByPath(path);
    }

    // Re-add to watcher (inotify removes after change)
    if (m_rightFileWatcher && QFileInfo::exists(path)) {
        if (!m_rightFileWatcher->files().contains(path)) {
            m_rightFileWatcher->addPath(path);
        }
    }
}

#ifdef _WIN32
// Windows implementation - use QStorageInfo to list drives
void MainWindow::refreshMountsToolbar()
{
    if (!m_mountsToolBar)
        return;

    m_mountsToolBar->clear();

    // Get all mounted volumes (drives) on Windows
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();

    for (const QStorageInfo& vol : volumes) {
        if (!vol.isValid() || !vol.isReady())
            continue;

        QString rootPath = vol.rootPath();  // e.g., "C:/"
        QString label = vol.name();         // Volume label
        QString displayLabel;

        // Format: "C:" or "C: Label" if label exists
        if (rootPath.endsWith('/') || rootPath.endsWith('\\'))
            rootPath.chop(1);  // Remove trailing slash -> "C:"

        if (label.isEmpty()) {
            displayLabel = rootPath;
        } else {
            displayLabel = QString("%1 %2").arg(rootPath, label);
        }

        QString tooltip = QString("%1\n%2\nFree: %3 / %4")
            .arg(vol.rootPath())
            .arg(QString::fromUtf8(vol.fileSystemType()))
            .arg(qFormatSize(vol.bytesFree(), Config::instance().storageSizeFormat()))
            .arg(qFormatSize(vol.bytesTotal(), Config::instance().storageSizeFormat()));

        auto* act = new VerticalToolButtonAction(displayLabel, m_mountsToolBar);
        act->setToolTip(tooltip);
        act->setData(vol.rootPath());

        connect(act, &QAction::triggered, this, [this, vol]() {
            FilePanel* panel = currentFilePanel();
            if (panel) {
                panel->currentPath = vol.rootPath();
                panel->loadDirectory();
            }
        });

        m_mountsToolBar->addAction(act);
    }
}
#else
// Linux implementation - use UDisks2
void MainWindow::refreshMountsToolbar()
{
    if (!m_mountsToolBar || !m_udisksManager)
        return;

    // Clear and rebuild toolbar
    m_mountsToolBar->clear();

    // Get all devices (both mounted and unmounted) from UDisks
    auto devices = m_udisksManager->getDevices(false); // false = exclude system partitions

    for (const BlockDeviceInfo& dev : devices) {
        // Use displayId() - returns label if available, otherwise UUID
        QString label = dev.displayId();

        // Show mount status in tooltip with free/total space
        QString tooltip;
        if (dev.isMounted) {
            QStorageInfo storage(dev.mountPoint);
            tooltip = QString("%1\n%2\n%3\nFree: %4 / %5")
                .arg(dev.device)
                .arg(dev.mountPoint)
                .arg(dev.fsType)
                .arg(qFormatSize(storage.bytesFree(), Config::instance().storageSizeFormat()))
                .arg(qFormatSize(storage.bytesTotal(), Config::instance().storageSizeFormat()));
        } else {
            tooltip = QString("%1\n%2\n%3")
                .arg(dev.device)
                .arg(tr("Not mounted"))
                .arg(dev.fsType);
        }

        auto* act = new VerticalToolButtonAction(label, m_mountsToolBar);
        act->setToolTip(tooltip);

        // Store device info in action data for click handler
        act->setData(dev.objectPath);

        // If unmounted, show with different style
        if (!dev.isMounted) {
            QFont font = act->font();
            font.setItalic(true);
            act->setFont(font);
        }

        connect(act, &QAction::triggered, this, [this, dev]() {
            FilePanel* panel = currentFilePanel();
            if (!panel)
                return;

            // If not mounted, mount it first
            if (!dev.isMounted) {
                qDebug() << "Mounting device" << dev.displayId();
                QString mountPoint = m_udisksManager->mountDevice(dev.objectPath);
                if (mountPoint.isEmpty()) {
                    QMessageBox::warning(this, tr("Mount Error"),
                        tr("Failed to mount device: %1").arg(dev.displayId()));
                    return;
                }
                // Navigate to mount point (will be set in onDeviceMounted signal)
                panel->currentPath = mountPoint;
                panel->loadDirectory();
            } else {
                // Already mounted, just navigate
                panel->currentPath = dev.mountPoint;
                panel->loadDirectory();
            }
        });

        m_mountsToolBar->addAction(act);
    }
}

void MainWindow::onDeviceMounted(const QString &objectPath, const QString &mountPoint)
{
    Q_UNUSED(objectPath)
    qDebug() << "Device mounted at:" << mountPoint;
    refreshMountsToolbar();
    refreshProcMountsToolbar();
}

void MainWindow::onDeviceUnmounted(const QString &objectPath)
{
    Q_UNUSED(objectPath)
    qDebug() << "Device unmounted";
    refreshMountsToolbar();
    refreshProcMountsToolbar();
}
#endif

#ifndef _WIN32
void MainWindow::createProcMountsToolbar()
{
    m_procMountsToolBar = addToolBar(tr("Other Mounts"));
    m_procMountsToolBar->setMovable(true);
    m_procMountsToolBar->setFloatable(false);

    refreshProcMountsToolbar();
}

void MainWindow::refreshProcMountsToolbar()
{
    if (!m_procMountsToolBar || !m_procMountsManager)
        return;

    m_procMountsToolBar->clear();

    // Get mount points from UDisks to exclude them
    QSet<QString> udisksMountPoints;
    if (m_udisksManager) {
        auto devices = m_udisksManager->getDevices(true);
        for (const BlockDeviceInfo& dev : devices) {
            if (!dev.mountPoint.isEmpty())
                udisksMountPoints.insert(dev.mountPoint);
        }
    }
    m_procMountsManager->setUDisksMountPoints(udisksMountPoints);
    m_procMountsManager->refresh();

    auto mounts = m_procMountsManager->getMounts();

    for (const MountInfo& mi : mounts) {
        // Skip if already visible in UDisks toolbar
        if (udisksMountPoints.contains(mi.mountPoint))
            continue;

        QString label = mi.displayLabel();

        QStorageInfo storage(mi.mountPoint);
        QString tooltip = QString("%1\n%2\nFree: %3 / %4")
            .arg(mi.mountPoint)
            .arg(mi.fsType)
            .arg(qFormatSize(storage.bytesFree(), Config::instance().storageSizeFormat()))
            .arg(qFormatSize(storage.bytesTotal(), Config::instance().storageSizeFormat()));

        auto* act = new VerticalToolButtonAction(label, m_procMountsToolBar);
        act->setToolTip(tooltip);
        act->setData(mi.mountPoint);  // Store mount point for context menu

        connect(act, &QAction::triggered, this, [this, mi]() {
            FilePanel* panel = currentFilePanel();
            if (panel) {
                panel->currentPath = mi.mountPoint;
                panel->loadDirectory();
            }
        });

        m_procMountsToolBar->addAction(act);
    }

    // Enable context menu on toolbar
    m_procMountsToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    disconnect(m_procMountsToolBar, &QToolBar::customContextMenuRequested, nullptr, nullptr);
    connect(m_procMountsToolBar, &QToolBar::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        QAction* act = m_procMountsToolBar->actionAt(pos);
        if (!act)
            return;

        QString mountPoint = act->data().toString();
        if (mountPoint.isEmpty())
            return;

        QMenu menu;
        QAction* umountAction = menu.addAction(tr("umount"));

        if (menu.exec(m_procMountsToolBar->mapToGlobal(pos)) == umountAction) {
            QProcess proc;
            proc.start("umount", QStringList() << mountPoint);
            proc.waitForFinished(5000);

            if (proc.exitCode() != 0) {
                QString error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
                if (error.contains("target is busy") || error.contains("device is busy")) {
                    QMessageBox::warning(this, tr("umount"),
                        tr("Cannot unmount: device is busy.\nClose all files and try again."));
                } else {
                    QMessageBox::warning(this, tr("umount"),
                        tr("Failed to unmount %1:\n%2").arg(mountPoint).arg(error));
                }
            }
        }
    });
}
#else
// Windows: don't create procMountsToolBar (it would be empty)
void MainWindow::createProcMountsToolbar()
{
    // m_procMountsToolBar remains nullptr on Windows
}

void MainWindow::refreshProcMountsToolbar()
{
}
#endif

void MainWindow::updateStorageInfoToolbar()
{
    if (!m_storageInfoToolBar)
        return;

    m_storageInfoToolBar->clear();

    FilePanel* panel = currentFilePanel();
    if (!panel)
        return;

    QStorageInfo storage(panel->currentPath);
    if (!storage.isValid())
        return;

    QString text = QString("Free: %1 / %2")
        .arg(qFormatSize(storage.bytesFree(), Config::instance().storageSizeFormat()))
        .arg(qFormatSize(storage.bytesTotal(), Config::instance().storageSizeFormat()));

    auto* label = new VerticalLabel(text, m_storageInfoToolBar);
    m_storageInfoToolBar->addWidget(label);
}

void MainWindow::applyStartupPaths(const QStringList& paths)
{
    if (paths.isEmpty())
        return;

    // First argument -> left panel
    if (paths.size() >= 1) {
        if (FilePanel* panel = filePanelForSide(Side::Left))
            panel->navigateToPath(paths.at(0));
    }

    // Second argument -> right panel
    if (paths.size() >= 2) {
        if (FilePanel* panel = filePanelForSide(Side::Right))
            panel->navigateToPath(paths.at(1));
    }
}

void MainWindow::openEditorForFile(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.isFile())
        return;

    if (!editorFrame)
        editorFrame = new EditorFrame(nullptr);
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

QToolBar* MainWindow::toolbarByName(const QString& name)
{
    if (name == "main") return m_mainToolBar;
    if (name == "mounts") return m_mountsToolBar;
    if (name == "other_mounts") return m_procMountsToolBar;
    if (name == "storage_info") return m_storageInfoToolBar;
    if (name == "function_bar") return m_functionBarToolBar;
    return nullptr;
}

void MainWindow::applyToolbarConfig()
{
    auto& config = Config::instance();

    // Apply menu visibility
    menuBar()->setVisible(config.menuVisible());

    // Helper to convert ToolbarArea to Qt::ToolBarArea
    auto toQtArea = [](ToolbarArea area) -> Qt::ToolBarArea {
        switch (area) {
            case ToolbarArea::Top: return Qt::TopToolBarArea;
            case ToolbarArea::Bottom: return Qt::BottomToolBarArea;
            case ToolbarArea::Left: return Qt::LeftToolBarArea;
            case ToolbarArea::Right: return Qt::RightToolBarArea;
        }
        return Qt::TopToolBarArea;
    };

    // Collect toolbars with their configs, sorted by area and order
    struct ToolbarEntry {
        QString name;
        QToolBar* toolbar;
        ToolbarConfig config;
    };
    QVector<ToolbarEntry> entries;

    for (const QString& name : config.toolbarNames()) {
        QToolBar* tb = toolbarByName(name);
        if (!tb) continue;
        entries.append({name, tb, config.toolbarConfig(name)});
    }

    // Sort by area then order
    std::sort(entries.begin(), entries.end(), [](const ToolbarEntry& a, const ToolbarEntry& b) {
        if (a.config.area != b.config.area)
            return static_cast<int>(a.config.area) < static_cast<int>(b.config.area);
        return a.config.order < b.config.order;
    });

    // Remove all toolbars first
    for (const auto& entry : entries) {
        removeToolBar(entry.toolbar);
    }

    // Add toolbars in correct order with line breaks
    ToolbarArea currentArea = ToolbarArea::Top;
    bool firstInArea = true;

    for (const auto& entry : entries) {
        Qt::ToolBarArea qtArea = toQtArea(entry.config.area);

        // Check if we're in a new area
        if (entry.config.area != currentArea) {
            currentArea = entry.config.area;
            firstInArea = true;
        }

        // Add line break if needed (but not before the first toolbar in an area)
        if (entry.config.lineBreak && !firstInArea) {
            addToolBarBreak(qtArea);
        }

        addToolBar(qtArea, entry.toolbar);
        entry.toolbar->setVisible(entry.config.visible);
        firstInArea = false;
    }

    // Emergency safeguard: ensure at least menu OR main toolbar is visible
    // Without this, user would have no way to restore visibility
    // Note: Use config values, not isVisible() - isVisible() returns false at startup
    // when main window is not yet shown
    bool menuVisible = config.menuVisible();
    bool mainToolbarVisible = config.toolbarConfig("main").visible;
    if (!menuVisible && !mainToolbarVisible) {
        menuBar()->setVisible(true);
        Config::instance().setMenuVisible(true);
        qWarning() << "Emergency safeguard: Both menu and main toolbar were hidden. Restoring menu visibility.";
    }
}

#include "MainWindow_impl.inc"
