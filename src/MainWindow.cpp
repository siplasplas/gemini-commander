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
#include <QGuiApplication>
#include <QStorageInfo>
#include <QFileDialog>
#include <QToolButton>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

#include "SortedDirIterator.h"
#include "SearchDialog.h"
#include "FunctionBar.h"
#include "editor/ViewerFrame.h"
#include "keys/KeyRouter.h"
#include "keys/ObjectRegistry.h"
#include "quitls.h"
#include "udisks/UDisksDeviceManager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    ObjectRegistry::add(this, "MainFrame");
    QString cfg = Config::instance().defaultConfigPath();
    Config::instance().load(cfg);
    Config::instance().setConfigPath(cfg);

    // Initialize UDisks2 BEFORE setupUi() so mounts toolbar can be populated
    m_udisksManager = new UDisksDeviceManager(this);

    connect(m_udisksManager, &UDisksDeviceManager::deviceAdded,
            this, [this](const BlockDeviceInfo &) {
                refreshMountsToolbar();
            });

    connect(m_udisksManager, &UDisksDeviceManager::deviceRemoved,
            this, [this](const QString &, const QString &) {
                refreshMountsToolbar();
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

    // Apply window geometry from config
    applyConfigGeometry();

    keyMap.load(":/config/keys.toml");
    KeyRouter::instance().setKeyMap(&keyMap);
    KeyRouter::instance().installOn(qApp, this);

    // Directory monitoring
    m_dirWatcher = new QFileSystemWatcher(this);
    connect(m_dirWatcher, &QFileSystemWatcher::directoryChanged,
            this, &MainWindow::onDirectoryChanged);

    // Update watched dirs when panels change directory
    for (auto* panel : allFilePanels()) {
        connect(panel, &FilePanel::directoryChanged,
                this, &MainWindow::updateWatchedDirectories);
    }
    updateWatchedDirectories();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // If editor is visible, close it first instead of closing the app
    if (editorFrame && editorFrame->isVisible()) {
        editorFrame->close();
        event->ignore();
        return;
    }

    // Save window geometry before closing
    // Use frameGeometry for position (includes window decorations)
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
    Config::instance().setWindowGeometry(winX, winY, width(), height());
    Config::instance().save();

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

void MainWindow::applyConfigGeometry()
{
    const auto& cfg = Config::instance();
    resize(cfg.windowWidth(), cfg.windowHeight());
    if (cfg.windowX() >= 0 && cfg.windowY() >= 0) {
        move(cfg.windowX(), cfg.windowY());
    }
}

void MainWindow::onConfigSaved()
{
    // Reload config from file
    Config::instance().load(Config::instance().configPath());

    // Apply window geometry
    applyConfigGeometry();

    // Update external tool button (in case path changed)
    updateExternalToolButton();

    // Update function bar visibility
    bool showFunctionBar = Config::instance().showFunctionBar();
    if (showFunctionBar) {
        m_functionBar->show();
    } else {
        m_functionBar->hide();
    }
    m_showFunctionBarAction->setChecked(showFunctionBar);
}

void MainWindow::setupUi() {
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

    // Function bar
    m_functionBar = new FunctionBar(centralWidget);
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
    mainLayout->addWidget(m_functionBar);
    mainLayout->setStretchFactor(splitter, 1);

    setCentralWidget(centralWidget);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QMenu* commandsMenu = menuBar()->addMenu(tr("&Commands"));
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QMenu* configMenu = menuBar()->addMenu(tr("C&onfiguration"));
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));

    // File menu
    QAction* quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::fromString("Alt+F4"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAction);

    // Configuration menu
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

    // --- TOOLBAR ---
    m_mainToolBar = addToolBar(tr("Main toolbar"));
    m_mainToolBar->setMovable(true);

    m_openTerminalAction = new QAction(tr("Terminal"), this);
    m_openTerminalAction->setIcon(QIcon(":/icons/terminal.svg"));
    connect(m_openTerminalAction, &QAction::triggered,
            this, &MainWindow::onOpenTerminal);

    // External tool action
    m_externalToolAction = new QAction(tr("External Tool"), this);
    connect(m_externalToolAction, &QAction::triggered,
            this, &MainWindow::onExternalToolClicked);

    m_mainToolBar->addAction(m_openTerminalAction);
    m_mainToolBar->addAction(m_externalToolAction);
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

    // Setup context menu for external tool button
    if (QToolButton* toolBtn = qobject_cast<QToolButton*>(
            m_mainToolBar->widgetForAction(m_externalToolAction))) {
        toolBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(toolBtn, &QToolButton::customContextMenuRequested,
                this, &MainWindow::onExternalToolContextMenu);
    }

    // Initialize external tool button from config
    updateExternalToolButton();

    // View menu - Function Bar toggle
    m_showFunctionBarAction = new QAction(tr("Show Function Bar"), this);
    m_showFunctionBarAction->setCheckable(true);
    m_showFunctionBarAction->setChecked(Config::instance().showFunctionBar());
    connect(m_showFunctionBarAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            m_functionBar->show();
        } else {
            m_functionBar->hide();
        }
        Config::instance().setShowFunctionBar(checked);
        Config::instance().save();
    });
    viewMenu->addAction(m_showFunctionBarAction);

    // Apply initial function bar visibility from config
    if (!Config::instance().showFunctionBar()) {
        m_functionBar->hide();
    }
}

Side MainWindow::opposite(Side side){
    return side==Side::Left?Side::Right:Side::Left;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Keyboard events are handled by KeyRouter - only focus events here
    if (event->type() == QEvent::FocusIn) {
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

void MainWindow::onExternalToolClicked()
{
    QString toolPath = Config::instance().externalToolPath();

    // If no tool configured, open configuration dialog
    if (toolPath.isEmpty()) {
        configureExternalTool();
        return;
    }

    // Verify executable still exists and is executable
    QFileInfo info(toolPath);
    if (!info.exists() || !info.isExecutable()) {
        QMessageBox::warning(this,
                           tr("External Tool"),
                           tr("The configured tool no longer exists or is not executable: %1")
                             .arg(toolPath));
        configureExternalTool();
        return;
    }

    // Get working directories from current and opposite panels
    QString currentDir;
    if (auto* pane = currentPane()) {
        currentDir = pane->currentPath();
    }
    if (currentDir.isEmpty()) {
        currentDir = QDir::homePath();
    }

    QString oppositeDir;
    if (auto* oppPanel = oppositeFilePanel()) {
        oppositeDir = oppPanel->currentPath;
    }
    if (oppositeDir.isEmpty()) {
        oppositeDir = currentDir; // fallback to current if opposite not available
    }

    // Launch the tool with both directories as arguments
    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(currentDir);
    proc->start(toolPath, QStringList() << currentDir << oppositeDir);

    if (!proc->waitForStarted(1000)) {
        QMessageBox::warning(this,
                           tr("External Tool"),
                           tr("Failed to start: %1").arg(toolPath));
        proc->deleteLater();
    }
}

void MainWindow::onExternalToolContextMenu(const QPoint& pos)
{
    QMenu menu(this);

    QAction* editAction = menu.addAction(tr("Configure Tool..."));
    connect(editAction, &QAction::triggered, this, &MainWindow::configureExternalTool);

    QString toolPath = Config::instance().externalToolPath();
    if (!toolPath.isEmpty()) {
        menu.addSeparator();
        QAction* clearAction = menu.addAction(tr("Clear Tool"));
        connect(clearAction, &QAction::triggered, this, [this]() {
            Config::instance().setExternalToolPath(QString());
            Config::instance().save();
            updateExternalToolButton();
        });
    }

    // Convert local pos to global - pos is relative to the QToolButton
    if (QToolButton* btn = qobject_cast<QToolButton*>(sender())) {
        menu.exec(btn->mapToGlobal(pos));
    }
}

void MainWindow::configureExternalTool()
{
    QString currentPath = Config::instance().externalToolPath();
    QString startDir = currentPath.isEmpty() ? "/usr/bin" : currentPath;

    QString selectedFile = QFileDialog::getOpenFileName(
        this,
        tr("Select External Tool"),
        startDir,
        tr("Executable files (*)"),
        nullptr,
        QFileDialog::DontUseNativeDialog
    );

    if (selectedFile.isEmpty())
        return; // User cancelled

    // Verify the selected file is executable
    QFileInfo info(selectedFile);
    if (!info.isExecutable()) {
        QMessageBox::warning(this,
                           tr("External Tool"),
                           tr("The selected file is not executable: %1")
                             .arg(selectedFile));
        return;
    }

    // Save to config
    Config::instance().setExternalToolPath(selectedFile);
    Config::instance().save();

    // Update button icon and tooltip
    updateExternalToolButton();
}

void MainWindow::updateExternalToolButton()
{
    if (!m_externalToolAction)
        return;

    QString toolPath = Config::instance().externalToolPath();

    if (toolPath.isEmpty()) {
        // No tool configured - show placeholder
        m_externalToolAction->setIcon(QIcon(":/icons/file_executable.svg"));
        m_externalToolAction->setToolTip(tr("Click to configure external tool"));
        m_externalToolAction->setText(tr("Tool"));
        return;
    }

    // Tool is configured
    QFileInfo info(toolPath);
    QString baseName = info.fileName();
    m_externalToolAction->setText(baseName);
    m_externalToolAction->setToolTip(tr("Launch %1\nRight-click to configure")
                                      .arg(toolPath));

    // Try to find and extract icon from .desktop file
    QString desktopFile = findDesktopFile(toolPath);
    if (!desktopFile.isEmpty()) {
        QString iconName = extractIconFromDesktop(desktopFile);
        if (!iconName.isEmpty()) {
            QIcon icon = QIcon::fromTheme(iconName);
            if (!icon.isNull()) {
                m_externalToolAction->setIcon(icon);
                return;
            }
        }
    }

    // Fallback to generic executable icon
    m_externalToolAction->setIcon(QIcon(":/icons/file_executable.svg"));
}

QString MainWindow::findDesktopFile(const QString& executablePath)
{
    QFileInfo info(executablePath);
    QString baseName = info.fileName();

    // Common locations for .desktop files
    QStringList searchPaths = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        QDir::homePath() + "/.local/share/applications"
    };

    for (const QString& searchPath : searchPaths) {
        QDir dir(searchPath);
        if (!dir.exists())
            continue;

        // Try exact match: baseName.desktop
        QString exactMatch = dir.filePath(baseName + ".desktop");
        if (QFile::exists(exactMatch)) {
            // Verify it contains the executable we're looking for
            QFile f(exactMatch);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                QString content = in.readAll();
                // Check if Exec line contains our executable name
                if (content.contains(QRegularExpression(
                    "^Exec=.*" + QRegularExpression::escape(baseName),
                    QRegularExpression::MultilineOption))) {
                    return exactMatch;
                }
            }
        }

        // Search all .desktop files in directory
        QStringList desktopFiles = dir.entryList(
            QStringList() << "*.desktop",
            QDir::Files
        );

        for (const QString& filename : desktopFiles) {
            QString fullPath = dir.filePath(filename);
            QFile f(fullPath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&f);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith("Exec=")) {
                    // Extract command from Exec line (may have args like "%F")
                    QString execLine = line.mid(5).trimmed();
                    // Remove arguments and env variables
                    QStringList parts = execLine.split(QRegularExpression("\\s+"));
                    for (const QString& part : parts) {
                        if (part.isEmpty() || part.startsWith("%"))
                            continue;
                        if (part.contains("=")) // env variable like GDK_BACKEND=x11
                            continue;

                        // Extract just the executable name
                        QFileInfo execInfo(part);
                        if (execInfo.fileName() == baseName) {
                            return fullPath;
                        }
                        break; // Only check first real argument
                    }
                }
            }
        }
    }

    return QString(); // Not found
}

QString MainWindow::extractIconFromDesktop(const QString& desktopFilePath)
{
    QFile f(desktopFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("Icon=")) {
            QString iconName = line.mid(5).trimmed();
            // Icon name can be:
            // 1. Just a name (e.g., "audacity") - use with QIcon::fromTheme
            // 2. Absolute path (e.g., "/usr/share/pixmaps/app.png")
            // We'll return the name and let QIcon::fromTheme handle it
            return iconName;
        }
    }

    return QString();
}

void MainWindow::createMountsToolbar()
{
    m_mountsToolBar = addToolBar(tr("Mounts"));
    m_mountsToolBar->setMovable(true);

    // Initial population will happen after UDisks manager starts
    // in the constructor
    refreshMountsToolbar();
}

void MainWindow::copyFromPanel(FilePanel* srcPanel, bool inPlace)
{
    if (!srcPanel)
        return;

    // Get destination panel info
    Side srcSide = srcPanel->side();
    Side dstSide = opposite(srcSide);
    FilePanel* dstPanel = filePanelForSide(dstSide);
    QString targetDir = dstPanel ? dstPanel->currentPath : srcPanel->currentPath;

    // Check for marked files
    QStringList markedNames = srcPanel->getMarkedNames();
    bool hasMarked = !markedNames.isEmpty();

    // Build suggested path
    QString suggested;
    QString currentName;

    if (hasMarked) {
        // Multiple files: propose directory only (or empty for inPlace)
        if (inPlace) {
            suggested = QString();
        } else {
            suggested = targetDir;
        }
    } else {
        // Single file: get current item (use getRowRelPath for Branch mode compatibility)
        QModelIndex currentIndex = srcPanel->currentIndex();
        if (!currentIndex.isValid())
            return;

        currentName = srcPanel->getRowRelPath(currentIndex.row());
        if (currentName.isEmpty())
            return; // [..]

        if (inPlace) {
            suggested = currentName;
        } else {
            QDir dstDir(targetDir);
            suggested = dstDir.filePath(currentName);
        }
    }

    // Show dialog
    bool ok = false;
    QString destInput = QInputDialog::getText(
        this,
        tr("Copy"),
        hasMarked ? tr("Copy %1 items to:").arg(markedNames.size()) : tr("Copy to:"),
        QLineEdit::Normal,
        suggested,
        &ok
    );

    if (!ok || destInput.isEmpty())
        return;

    // Resolve relative paths - always relative to source panel (where file currently is)
    QString baseDirForRelative = srcPanel->currentPath;

    QString dstPath;
    if (QDir::isAbsolutePath(destInput)) {
        dstPath = destInput;
    } else {
        QDir baseDir(baseDirForRelative);
        dstPath = baseDir.absoluteFilePath(destInput);
    }

    QFileInfo dstInfo(dstPath);

    if (hasMarked) {
        // Multiple files: destination is ALWAYS treated as directory
        // Check if exists
        if (!dstInfo.exists()) {
            auto reply = QMessageBox::question(
                this,
                tr("Create Directory"),
                tr("Directory '%1' does not exist.\nCreate it?").arg(dstPath),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                QMessageBox::Yes
            );
            if (reply == QMessageBox::Cancel)
                return;
            if (reply == QMessageBox::Yes) {
                QDir().mkpath(dstPath);
            } else {
                return;
            }
        } else if (!dstInfo.isDir()) {
            QMessageBox::warning(this, tr("Error"),
                tr("'%1' exists but is not a directory.").arg(dstPath));
            return;
        }

        // Copy all marked files
        QDir srcDir(srcPanel->currentPath);
        for (const QString& name : markedNames) {
            QString srcPath = srcDir.absoluteFilePath(name);
            QString dstFilePath = QDir(dstPath).filePath(name);
            QFileInfo srcInfo(srcPath);

            if (srcInfo.isFile()) {
                if (QFileInfo::exists(dstFilePath)) {
                    auto reply = QMessageBox::question(
                        this, tr("Overwrite"),
                        tr("File '%1' already exists.\nOverwrite?").arg(name),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,

                        QMessageBox::Yes
                    );
                    if (reply == QMessageBox::Cancel)
                        break;
                    if (reply != QMessageBox::Yes)
                        continue;
                    QFile::remove(dstFilePath);
                }
                if (!QFile::copy(srcPath, dstFilePath)) {
                    QMessageBox::warning(this, tr("Error"),
                        tr("Failed to copy '%1'").arg(name));
                } else {
                    finalizeCopiedFile(srcPath, dstFilePath);
                }
            } else if (srcInfo.isDir()) {
                FilePanel::CopyStats stats;
                bool statsOk = false;
                FilePanel::collectCopyStats(srcPath, stats, statsOk);

                QProgressDialog progress(
                    tr("Copying %1...").arg(name), tr("Cancel"), 0,
                    static_cast<int>(qMin<quint64>(stats.totalBytes, std::numeric_limits<int>::max())),
                    this
                );
                progress.setWindowModality(Qt::ApplicationModal);
                progress.setMinimumDuration(0);
                progress.show();

                quint64 bytesCopied = 0;
                bool userAbort = false;
                FilePanel::copyDirectoryRecursive(srcPath, dstFilePath, stats, progress, bytesCopied, userAbort);
                if (userAbort)
                    break;
            }
        }

        // Refresh panels
        srcPanel->loadDirectory();
        if (dstPanel)
            dstPanel->loadDirectory();
        return;
    }

    // Single file copy
    QDir srcDir(srcPanel->currentPath);
    QString srcPath = srcDir.absoluteFilePath(currentName);
    QFileInfo srcInfo(srcPath);

    // Determine if destination is directory:
    // - ends with '/' OR exists as directory
    bool destIsDir = destInput.endsWith('/')
                     || destInput == "." || destInput == ".."
                     || destInput.endsWith("/.")  || destInput.endsWith("/..")
                     || (dstInfo.exists() && dstInfo.isDir());

    QString finalDstPath;
    if (destIsDir) {
        // Copy into directory with original name
        QDir dstDir(dstPath);
        finalDstPath = dstDir.filePath(currentName);
    } else {
        // Destination is new filename
        finalDstPath = dstPath;
    }

    QFileInfo finalDstInfo(finalDstPath);

    if (srcInfo.isFile()) {
        if (finalDstInfo.exists()) {
            auto reply = QMessageBox::question(
                this, tr("Overwrite"),
                tr("File '%1' already exists.\nOverwrite?").arg(finalDstInfo.fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );
            if (reply != QMessageBox::Yes)
                return;
            QFile::remove(finalDstPath);
        }

        // Ensure parent directory exists
        QDir parentDir = QFileInfo(finalDstPath).absoluteDir();
        if (!parentDir.exists()) {
            parentDir.mkpath(".");
        }

        if (!QFile::copy(srcPath, finalDstPath)) {
            QMessageBox::warning(this, tr("Error"),
                tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, finalDstPath));
            return;
        }
        finalizeCopiedFile(srcPath, finalDstPath);

        if (dstPanel) {
            dstPanel->loadDirectory();
            dstPanel->selectEntryByName(QFileInfo(finalDstPath).fileName());
        }
        return;
    }

    if (srcInfo.isDir()) {
        QString dstRoot = finalDstPath;
        if (finalDstInfo.exists() && finalDstInfo.isDir()) {
            QDir base(finalDstPath);
            dstRoot = base.filePath(srcInfo.fileName());
        }

        QDir checkDir(dstRoot);
        if (checkDir.exists()) {
            const QStringList entries = checkDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
            if (!entries.isEmpty()) {
                QMessageBox::warning(this, tr("Copy"),
                    tr("Destination directory '%1' already exists and is not empty.\n"
                       "Recursive copy into non-empty directories is not supported yet.")
                        .arg(dstRoot));
                return;
            }
        }

        FilePanel::CopyStats stats;
        bool statsOk = false;
        FilePanel::collectCopyStats(srcPath, stats, statsOk);
        if (!statsOk || stats.totalFiles == 0) {
            QMessageBox::warning(this, tr("Copy"),
                tr("No files to copy in '%1'.").arg(srcPath));
            return;
        }

        QProgressDialog progress(
            tr("Copying %1 files (%2 bytes)...")
                .arg(stats.totalFiles)
                .arg(static_cast<qulonglong>(stats.totalBytes)),
            tr("Cancel"), 0,
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

        if (!okCopy && userAbort)
            return;
        if (!okCopy)
            return;

        if (dstPanel) {
            dstPanel->loadDirectory();
            dstPanel->selectEntryByName(QFileInfo(dstRoot).fileName());
        }
    }
}

void MainWindow::moveFromPanel(FilePanel* srcPanel, bool inPlace)
{
    if (!srcPanel)
        return;

    // Get destination panel info
    Side srcSide = srcPanel->side();
    Side dstSide = opposite(srcSide);
    FilePanel* dstPanel = filePanelForSide(dstSide);
    QString targetDir = dstPanel ? dstPanel->currentPath : srcPanel->currentPath;

    // Check for marked files
    QStringList markedNames = srcPanel->getMarkedNames();
    bool hasMarked = !markedNames.isEmpty();

    // Build suggested path
    QString suggested;
    QString currentName;

    if (hasMarked) {
        if (inPlace) {
            suggested = QString();
        } else {
            suggested = targetDir;
        }
    } else {
        // Single file: get current item (use getRowRelPath for Branch mode compatibility)
        QModelIndex currentIndex = srcPanel->currentIndex();
        if (!currentIndex.isValid())
            return;

        currentName = srcPanel->getRowRelPath(currentIndex.row());
        if (currentName.isEmpty())
            return;

        if (inPlace) {
            suggested = currentName;
        } else {
            QDir dstDir(targetDir);
            suggested = dstDir.filePath(currentName);
        }
    }

    // Show dialog
    bool ok = false;
    QString destInput = QInputDialog::getText(
        this,
        tr("Move/Rename"),
        hasMarked ? tr("Move %1 items to:").arg(markedNames.size()) : tr("Move to:"),
        QLineEdit::Normal,
        suggested,
        &ok
    );

    if (!ok || destInput.isEmpty())
        return;

    // Resolve relative paths - always relative to source panel (where file currently is)
    QString baseDirForRelative = srcPanel->currentPath;

    QString dstPath;
    if (QDir::isAbsolutePath(destInput)) {
        dstPath = destInput;
    } else {
        QDir baseDir(baseDirForRelative);
        dstPath = baseDir.absoluteFilePath(destInput);
    }

    QFileInfo dstInfo(dstPath);

    if (hasMarked) {
        // Multiple files: destination is ALWAYS treated as directory
        if (!dstInfo.exists()) {
            auto reply = QMessageBox::question(
                this,
                tr("Create Directory"),
                tr("Directory '%1' does not exist.\nCreate it?").arg(dstPath),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                QMessageBox::Yes
            );
            if (reply == QMessageBox::Cancel)
                return;
            if (reply == QMessageBox::Yes) {
                QDir().mkpath(dstPath);
            } else {
                return;
            }
        } else if (!dstInfo.isDir()) {
            QMessageBox::warning(this, tr("Error"),
                tr("'%1' exists but is not a directory.").arg(dstPath));
            return;
        }

        // Move all marked files
        QDir srcDir(srcPanel->currentPath);
        for (const QString& name : markedNames) {
            QString srcPath = srcDir.absoluteFilePath(name);
            QString dstFilePath = QDir(dstPath).filePath(name);

            if (QFileInfo::exists(dstFilePath)) {
                auto reply = QMessageBox::question(
                    this, tr("Overwrite"),
                    tr("'%1' already exists.\nOverwrite?").arg(name),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    QMessageBox::Yes
                );
                if (reply == QMessageBox::Cancel)
                    break;
                if (reply != QMessageBox::Yes)
                    continue;
                // Remove existing
                QFileInfo existingInfo(dstFilePath);
                if (existingInfo.isDir()) {
                    QDir(dstFilePath).removeRecursively();
                } else {
                    QFile::remove(dstFilePath);
                }
            }

            QFile file(srcPath);
            if (!file.rename(dstFilePath)) {
                QMessageBox::warning(this, tr("Error"),
                    tr("Failed to move '%1'").arg(name));
            }
        }

        // Refresh panels
        srcPanel->loadDirectory();
        if (dstPanel)
            dstPanel->loadDirectory();
        return;
    }

    // Single file move
    QDir srcDir(srcPanel->currentPath);
    QString srcPath = srcDir.absoluteFilePath(currentName);
    QFileInfo srcInfo(srcPath);

    // Determine if destination is directory
    bool destIsDir = destInput.endsWith('/')
                     || destInput == "." || destInput == ".."
                     || destInput.endsWith("/.")  || destInput.endsWith("/..")
                     || (dstInfo.exists() && dstInfo.isDir());

    QString finalDstPath;
    if (destIsDir) {
        QDir dstDir(dstPath);
        finalDstPath = dstDir.filePath(currentName);
    } else {
        finalDstPath = dstPath;
    }

    QFileInfo finalDstInfo(finalDstPath);

    if (finalDstInfo.exists()) {
        auto reply = QMessageBox::question(
            this, tr("Overwrite"),
            tr("'%1' already exists.\nOverwrite?").arg(finalDstInfo.fileName()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );
        if (reply != QMessageBox::Yes)
            return;
        if (finalDstInfo.isDir()) {
            QDir(finalDstPath).removeRecursively();
        } else {
            QFile::remove(finalDstPath);
        }
    }

    // Ensure parent directory exists
    QDir parentDir = QFileInfo(finalDstPath).absoluteDir();
    if (!parentDir.exists()) {
        parentDir.mkpath(".");
    }

    QFile file(srcPath);
    if (!file.rename(finalDstPath)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to move:\n%1\nto\n%2").arg(srcPath, finalDstPath));
        return;
    }

    // Refresh panels
    srcPanel->loadDirectory();
    if (dstPanel) {
        dstPanel->loadDirectory();
        dstPanel->selectEntryByName(QFileInfo(finalDstPath).fileName());
    }
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
    // Refresh panels showing this directory
    // Skip panels in branchMode - they use incremental updates
    FilePanel* leftPanel = filePanelForSide(Side::Left);
    FilePanel* rightPanel = filePanelForSide(Side::Right);

    if (leftPanel && leftPanel->currentPath == path && !leftPanel->branchMode) {
        leftPanel->loadDirectory();
    }
    if (rightPanel && rightPanel->currentPath == path && !rightPanel->branchMode) {
        rightPanel->loadDirectory();
    }

    // Re-add path to watcher (some systems like Linux inotify remove it after change)
    // Check if already watched to avoid unnecessary call
    if (m_dirWatcher && QDir(path).exists()) {
        if (!m_dirWatcher->directories().contains(path)) {
            m_dirWatcher->addPath(path);
        }
    }
}

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

        // Show mount status in tooltip
        QString tooltip = QString("%1: %2 (%3)")
            .arg(dev.device)
            .arg(dev.isMounted ? dev.mountPoint : tr("Not mounted"))
            .arg(dev.fsType);

        QAction* act = new QAction(label, m_mountsToolBar);
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
}

void MainWindow::onDeviceUnmounted(const QString &objectPath)
{
    Q_UNUSED(objectPath)
    qDebug() << "Device unmounted";
    refreshMountsToolbar();
}

#include "MainWindow_impl.inc"
