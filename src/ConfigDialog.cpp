#include "ConfigDialog.h"
#include "Config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QFileDialog>
#include <QMessageBox>

ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Configuration"));
    resize(700, 500);

    setupUi();
    loadSettings();
}

void ConfigDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Top section: category list + pages
    auto* contentLayout = new QHBoxLayout();

    // Category list on the left
    m_categoryList = new QListWidget(this);
    m_categoryList->setMaximumWidth(150);
    m_categoryList->addItem(tr("Window"));
    m_categoryList->addItem(tr("Panels"));
    m_categoryList->addItem(tr("History"));
    m_categoryList->addItem(tr("General"));
    m_categoryList->setCurrentRow(0);

    contentLayout->addWidget(m_categoryList);

    // Stacked widget for pages
    m_pagesStack = new QStackedWidget(this);

    createWindowPage();
    createPanelsPage();
    createHistoryPage();
    createGeneralPage();

    contentLayout->addWidget(m_pagesStack, 1);

    mainLayout->addLayout(contentLayout, 1);

    // Buttons at the bottom
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton = new QPushButton(tr("OK"), this);
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_applyButton = new QPushButton(tr("Apply"), this);

    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_applyButton);

    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_categoryList, &QListWidget::currentRowChanged,
            this, &ConfigDialog::onCategoryChanged);
    connect(m_okButton, &QPushButton::clicked, this, &ConfigDialog::onOk);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_applyButton, &QPushButton::clicked, this, &ConfigDialog::onApply);
}

void ConfigDialog::onCategoryChanged(int index)
{
    m_pagesStack->setCurrentIndex(index);
}

void ConfigDialog::createWindowPage()
{
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    // Main window size
    auto* sizeGroup = new QGroupBox(tr("Main Window Size"), page);
    auto* sizeLayout = new QFormLayout(sizeGroup);

    m_windowWidth = new QSpinBox(sizeGroup);
    m_windowWidth->setRange(400, 4000);
    m_windowWidth->setSuffix(" px");
    sizeLayout->addRow(tr("Width:"), m_windowWidth);

    m_windowHeight = new QSpinBox(sizeGroup);
    m_windowHeight->setRange(300, 3000);
    m_windowHeight->setSuffix(" px");
    sizeLayout->addRow(tr("Height:"), m_windowHeight);

    // Wayland warning
    if (isWayland()) {
        m_waylandWarning = new QLabel(
            tr("<b>Note:</b> On Wayland, window size changes require application restart."),
            sizeGroup);
        m_waylandWarning->setWordWrap(true);
        m_waylandWarning->setStyleSheet("QLabel { color: #b58900; padding: 5px; }");
        sizeLayout->addRow(m_waylandWarning);
    } else {
        m_waylandWarning = nullptr;
    }

    layout->addWidget(sizeGroup);

    // Main window position (X11 only)
    auto* posGroup = new QGroupBox(tr("Main Window Position"), page);
    auto* posLayout = new QFormLayout(posGroup);

    m_positionEnabled = new QCheckBox(tr("Remember window position"), posGroup);
    posLayout->addRow(m_positionEnabled);

    m_windowX = new QSpinBox(posGroup);
    m_windowX->setRange(-10000, 10000);
    m_windowX->setSuffix(" px");
    posLayout->addRow(tr("X:"), m_windowX);

    m_windowY = new QSpinBox(posGroup);
    m_windowY->setRange(-10000, 10000);
    m_windowY->setSuffix(" px");
    posLayout->addRow(tr("Y:"), m_windowY);

    if (isWayland()) {
        posGroup->setEnabled(false);
        posGroup->setTitle(tr("Main Window Position (not available on Wayland)"));
    }

    layout->addWidget(posGroup);

    // Editor window geometry
    auto* editorGroup = new QGroupBox(tr("Editor Window (relative to main)"), page);
    auto* editorLayout = new QFormLayout(editorGroup);

    m_editorWidth = new QSpinBox(editorGroup);
    m_editorWidth->setRange(200, 4000);
    m_editorWidth->setSuffix(" px");
    editorLayout->addRow(tr("Width:"), m_editorWidth);

    m_editorHeight = new QSpinBox(editorGroup);
    m_editorHeight->setRange(150, 3000);
    m_editorHeight->setSuffix(" px");
    editorLayout->addRow(tr("Height:"), m_editorHeight);

    m_editorX = new QSpinBox(editorGroup);
    m_editorX->setRange(-10000, 10000);
    m_editorX->setSuffix(" px");
    editorLayout->addRow(tr("X offset:"), m_editorX);

    m_editorY = new QSpinBox(editorGroup);
    m_editorY->setRange(-10000, 10000);
    m_editorY->setSuffix(" px");
    editorLayout->addRow(tr("Y offset:"), m_editorY);

    layout->addWidget(editorGroup);

    // Viewer window geometry
    auto* viewerGroup = new QGroupBox(tr("Viewer Window (relative to main)"), page);
    auto* viewerLayout = new QFormLayout(viewerGroup);

    m_viewerWidth = new QSpinBox(viewerGroup);
    m_viewerWidth->setRange(200, 4000);
    m_viewerWidth->setSuffix(" px");
    viewerLayout->addRow(tr("Width:"), m_viewerWidth);

    m_viewerHeight = new QSpinBox(viewerGroup);
    m_viewerHeight->setRange(150, 3000);
    m_viewerHeight->setSuffix(" px");
    viewerLayout->addRow(tr("Height:"), m_viewerHeight);

    m_viewerX = new QSpinBox(viewerGroup);
    m_viewerX->setRange(-10000, 10000);
    m_viewerX->setSuffix(" px");
    viewerLayout->addRow(tr("X offset:"), m_viewerX);

    m_viewerY = new QSpinBox(viewerGroup);
    m_viewerY->setRange(-10000, 10000);
    m_viewerY->setSuffix(" px");
    viewerLayout->addRow(tr("Y offset:"), m_viewerY);

    layout->addWidget(viewerGroup);

    layout->addStretch();

    // Connect position checkbox
    connect(m_positionEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        m_windowX->setEnabled(checked);
        m_windowY->setEnabled(checked);
    });

    m_pagesStack->addWidget(page);
}

void ConfigDialog::createPanelsPage()
{
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    // Starting directories
    auto* startDirGroup = new QGroupBox(tr("Starting Directories"), page);
    auto* startDirLayout = new QFormLayout(startDirGroup);

    auto* leftDirLayout = new QHBoxLayout();
    m_leftPanelStartDir = new QLineEdit(startDirGroup);
    m_leftPanelStartDir->setPlaceholderText(tr("Default: current directory"));
    auto* leftBrowseBtn = new QPushButton(tr("..."), startDirGroup);
    leftBrowseBtn->setMaximumWidth(30);
    leftDirLayout->addWidget(m_leftPanelStartDir);
    leftDirLayout->addWidget(leftBrowseBtn);
    startDirLayout->addRow(tr("Left panel:"), leftDirLayout);

    auto* rightDirLayout = new QHBoxLayout();
    m_rightPanelStartDir = new QLineEdit(startDirGroup);
    m_rightPanelStartDir->setPlaceholderText(tr("Default: current directory"));
    auto* rightBrowseBtn = new QPushButton(tr("..."), startDirGroup);
    rightBrowseBtn->setMaximumWidth(30);
    rightDirLayout->addWidget(m_rightPanelStartDir);
    rightDirLayout->addWidget(rightBrowseBtn);
    startDirLayout->addRow(tr("Right panel:"), rightDirLayout);

    auto* noteLabel = new QLabel(
        tr("Note: Command line arguments have higher priority than these settings."),
        startDirGroup);
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    startDirLayout->addRow(noteLabel);

    layout->addWidget(startDirGroup);

    // Default sorting
    auto* sortGroup = new QGroupBox(tr("Default Sorting"), page);
    auto* sortLayout = new QFormLayout(sortGroup);

    // Left panel sorting
    auto* leftSortLayout = new QHBoxLayout();
    m_leftSortColumn = new QComboBox(sortGroup);
    m_leftSortColumn->addItem(tr("Name"));
    m_leftSortColumn->addItem(tr("Extension"));
    m_leftSortColumn->addItem(tr("Size"));
    m_leftSortColumn->addItem(tr("Date"));
    m_leftSortOrder = new QComboBox(sortGroup);
    m_leftSortOrder->addItem(tr("Ascending"));
    m_leftSortOrder->addItem(tr("Descending"));
    leftSortLayout->addWidget(m_leftSortColumn);
    leftSortLayout->addWidget(m_leftSortOrder);
    sortLayout->addRow(tr("Left panel:"), leftSortLayout);

    // Right panel sorting
    auto* rightSortLayout = new QHBoxLayout();
    m_rightSortColumn = new QComboBox(sortGroup);
    m_rightSortColumn->addItem(tr("Name"));
    m_rightSortColumn->addItem(tr("Extension"));
    m_rightSortColumn->addItem(tr("Size"));
    m_rightSortColumn->addItem(tr("Date"));
    m_rightSortOrder = new QComboBox(sortGroup);
    m_rightSortOrder->addItem(tr("Ascending"));
    m_rightSortOrder->addItem(tr("Descending"));
    rightSortLayout->addWidget(m_rightSortColumn);
    rightSortLayout->addWidget(m_rightSortOrder);
    sortLayout->addRow(tr("Right panel:"), rightSortLayout);

    layout->addWidget(sortGroup);

    layout->addStretch();

    // Browse button connections
    connect(leftBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Left Panel Start Directory"),
            m_leftPanelStartDir->text());
        if (!dir.isEmpty())
            m_leftPanelStartDir->setText(dir);
    });

    connect(rightBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Right Panel Start Directory"),
            m_rightPanelStartDir->text());
        if (!dir.isEmpty())
            m_rightPanelStartDir->setText(dir);
    });

    m_pagesStack->addWidget(page);
}

void ConfigDialog::createHistoryPage()
{
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    auto* historyGroup = new QGroupBox(tr("Directory Navigation History"), page);
    auto* historyLayout = new QFormLayout(historyGroup);

    m_historyDescription = new QLabel(
        tr("The application keeps a history of directories you have visited. "
           "Use Alt+Left/Right to navigate back and forward through this history."),
        historyGroup);
    m_historyDescription->setWordWrap(true);
    historyLayout->addRow(m_historyDescription);

    m_maxHistorySize = new QSpinBox(historyGroup);
    m_maxHistorySize->setRange(5, 1000);
    m_maxHistorySize->setSuffix(tr(" entries"));
    historyLayout->addRow(tr("Maximum history size:"), m_maxHistorySize);

    layout->addWidget(historyGroup);
    layout->addStretch();

    m_pagesStack->addWidget(page);
}

void ConfigDialog::createGeneralPage()
{
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    auto* behaviorGroup = new QGroupBox(tr("Behavior"), page);
    auto* behaviorLayout = new QVBoxLayout(behaviorGroup);

    m_confirmExit = new QCheckBox(tr("Confirm before exit"), behaviorGroup);
    behaviorLayout->addWidget(m_confirmExit);

    m_showFunctionBar = new QCheckBox(tr("Show function key bar (F1-F10)"), behaviorGroup);
    behaviorLayout->addWidget(m_showFunctionBar);

    layout->addWidget(behaviorGroup);

    auto* toolsGroup = new QGroupBox(tr("External Tools"), page);
    auto* toolsLayout = new QFormLayout(toolsGroup);

    auto* toolPathLayout = new QHBoxLayout();
    m_externalToolPath = new QLineEdit(toolsGroup);
    m_externalToolPath->setPlaceholderText(tr("Path to external tool"));
    auto* toolBrowseBtn = new QPushButton(tr("..."), toolsGroup);
    toolBrowseBtn->setMaximumWidth(30);
    toolPathLayout->addWidget(m_externalToolPath);
    toolPathLayout->addWidget(toolBrowseBtn);
    toolsLayout->addRow(tr("External tool:"), toolPathLayout);

    layout->addWidget(toolsGroup);

    layout->addStretch();

    // Browse button connection
    connect(toolBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(
            this, tr("Select External Tool"),
            m_externalToolPath->text());
        if (!file.isEmpty())
            m_externalToolPath->setText(file);
    });

    m_pagesStack->addWidget(page);
}

void ConfigDialog::loadSettings()
{
    const auto& cfg = Config::instance();

    // Window page
    m_windowWidth->setValue(cfg.windowWidth());
    m_windowHeight->setValue(cfg.windowHeight());
    m_windowX->setValue(cfg.windowX());
    m_windowY->setValue(cfg.windowY());

    // Remember initial values to detect changes
    m_initialWidth = cfg.windowWidth();
    m_initialHeight = cfg.windowHeight();

    bool hasPosition = (cfg.windowX() >= 0 && cfg.windowY() >= 0);
    m_positionEnabled->setChecked(hasPosition);
    m_windowX->setEnabled(hasPosition);
    m_windowY->setEnabled(hasPosition);

    m_editorWidth->setValue(cfg.editorWidth());
    m_editorHeight->setValue(cfg.editorHeight());
    m_editorX->setValue(cfg.editorX());
    m_editorY->setValue(cfg.editorY());

    m_viewerWidth->setValue(cfg.viewerWidth());
    m_viewerHeight->setValue(cfg.viewerHeight());
    m_viewerX->setValue(cfg.viewerX());
    m_viewerY->setValue(cfg.viewerY());

    // Panels page - new options, will have defaults
    // TODO: Load from config when implemented
    m_leftPanelStartDir->clear();
    m_rightPanelStartDir->clear();
    m_leftSortColumn->setCurrentIndex(0);  // Name
    m_leftSortOrder->setCurrentIndex(0);   // Ascending
    m_rightSortColumn->setCurrentIndex(0);
    m_rightSortOrder->setCurrentIndex(0);

    // History page
    m_maxHistorySize->setValue(cfg.maxHistorySize());

    // General page
    m_confirmExit->setChecked(cfg.confirmExit());
    m_showFunctionBar->setChecked(cfg.showFunctionBar());
    m_externalToolPath->setText(cfg.externalToolPath());
}

void ConfigDialog::saveSettings()
{
    auto& cfg = Config::instance();

    // Window settings
    int x = m_positionEnabled->isChecked() ? m_windowX->value() : -1;
    int y = m_positionEnabled->isChecked() ? m_windowY->value() : -1;
    cfg.setWindowGeometry(x, y, m_windowWidth->value(), m_windowHeight->value());

    cfg.setEditorGeometry(m_editorX->value(), m_editorY->value(),
                          m_editorWidth->value(), m_editorHeight->value());

    cfg.setViewerGeometry(m_viewerX->value(), m_viewerY->value(),
                          m_viewerWidth->value(), m_viewerHeight->value());

    // Panels - TODO: implement in Config when ready
    // cfg.setLeftPanelStartDir(m_leftPanelStartDir->text());
    // cfg.setRightPanelStartDir(m_rightPanelStartDir->text());

    // History
    cfg.setMaxHistorySize(m_maxHistorySize->value());

    // General
    cfg.setConfirmExit(m_confirmExit->isChecked());
    cfg.setShowFunctionBar(m_showFunctionBar->isChecked());
    cfg.setExternalToolPath(m_externalToolPath->text());

    // Save to file
    cfg.save();
}

void ConfigDialog::onApply()
{
    saveSettings();
    emit settingsApplied();
    showRestartWarningIfNeeded();
}

void ConfigDialog::onOk()
{
    saveSettings();
    emit settingsApplied();
    showRestartWarningIfNeeded();
    accept();
}

void ConfigDialog::showRestartWarningIfNeeded()
{
    if (!isWayland())
        return;

    // Check if window size changed
    bool sizeChanged = (m_windowWidth->value() != m_initialWidth ||
                        m_windowHeight->value() != m_initialHeight);

    if (sizeChanged) {
        QMessageBox::information(this, tr("Settings Applied"),
            tr("Window size changes will take effect after restarting the application."));

        // Update initial values so the warning is shown only once per change
        m_initialWidth = m_windowWidth->value();
        m_initialHeight = m_windowHeight->value();
    }
}

bool ConfigDialog::isWayland() const
{
    return QGuiApplication::platformName() == QLatin1String("wayland");
}