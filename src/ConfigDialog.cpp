#include "ConfigDialog.h"
#include "Config.h"
#include "SizeFormat.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QMenu>

// ============================================================================
// ColumnListWidget implementation
// ============================================================================

int ColumnListWidget::defaultWidth(const QString& column)
{
    static const QMap<QString, int> defaults = {
        {"Name", 40}, {"Ext", 10}, {"Size", 24}, {"Date", 26}, {"Attr", 16}
    };
    return defaults.value(column, 20);
}

ColumnListWidget::ColumnListWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Table widget
    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("Column"), tr("Width %")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(1, 70);
    m_table->verticalHeader()->setVisible(false);
    m_table->setMaximumHeight(150);

    layout->addWidget(m_table, 1);

    // Buttons on the right
    auto* btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(2);

    m_upBtn = new QToolButton(this);
    m_upBtn->setArrowType(Qt::UpArrow);
    m_upBtn->setToolTip(tr("Move up"));
    btnLayout->addWidget(m_upBtn);

    m_downBtn = new QToolButton(this);
    m_downBtn->setArrowType(Qt::DownArrow);
    m_downBtn->setToolTip(tr("Move down"));
    btnLayout->addWidget(m_downBtn);

    btnLayout->addSpacing(10);

    m_addBtn = new QToolButton(this);
    m_addBtn->setText("+");
    m_addBtn->setStyleSheet("QToolButton { color: green; font-weight: bold; }");
    m_addBtn->setToolTip(tr("Add column"));
    btnLayout->addWidget(m_addBtn);

    m_removeBtn = new QToolButton(this);
    m_removeBtn->setText("-");
    m_removeBtn->setStyleSheet("QToolButton { color: red; font-weight: bold; }");
    m_removeBtn->setToolTip(tr("Remove column"));
    btnLayout->addWidget(m_removeBtn);

    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // Connections
    connect(m_upBtn, &QToolButton::clicked, this, &ColumnListWidget::onMoveUp);
    connect(m_downBtn, &QToolButton::clicked, this, &ColumnListWidget::onMoveDown);
    connect(m_addBtn, &QToolButton::clicked, this, &ColumnListWidget::onAdd);
    connect(m_removeBtn, &QToolButton::clicked, this, &ColumnListWidget::onRemove);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ColumnListWidget::onSelectionChanged);

    updateButtonStates();
}

void ColumnListWidget::setColumns(const QStringList& columns, const QVector<double>& proportions)
{
    m_table->setRowCount(columns.size());
    for (int i = 0; i < columns.size(); ++i) {
        auto* nameItem = new QTableWidgetItem(columns[i]);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 0, nameItem);

        int width = (i < proportions.size()) ? qRound(proportions[i] * 100) : defaultWidth(columns[i]);
        auto* widthItem = new QTableWidgetItem(QString::number(width));
        m_table->setItem(i, 1, widthItem);
    }
    updateButtonStates();
}

QStringList ColumnListWidget::columns() const
{
    QStringList result;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (auto* item = m_table->item(i, 0))
            result.append(item->text());
    }
    return result;
}

QVector<double> ColumnListWidget::proportions() const
{
    QVector<double> result;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (auto* item = m_table->item(i, 1)) {
            bool ok;
            int val = item->text().toInt(&ok);
            result.append(ok ? val / 100.0 : 0.25);
        } else {
            result.append(0.25);
        }
    }
    return result;
}

void ColumnListWidget::onMoveUp()
{
    int row = m_table->currentRow();
    if (row <= 0)
        return;

    // Swap with row above
    for (int col = 0; col < m_table->columnCount(); ++col) {
        QTableWidgetItem* current = m_table->takeItem(row, col);
        QTableWidgetItem* above = m_table->takeItem(row - 1, col);
        m_table->setItem(row - 1, col, current);
        m_table->setItem(row, col, above);
    }
    m_table->setCurrentCell(row - 1, 0);
    updateButtonStates();
}

void ColumnListWidget::onMoveDown()
{
    int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount() - 1)
        return;

    // Swap with row below
    for (int col = 0; col < m_table->columnCount(); ++col) {
        QTableWidgetItem* current = m_table->takeItem(row, col);
        QTableWidgetItem* below = m_table->takeItem(row + 1, col);
        m_table->setItem(row + 1, col, current);
        m_table->setItem(row, col, below);
    }
    m_table->setCurrentCell(row + 1, 0);
    updateButtonStates();
}

void ColumnListWidget::onRemove()
{
    if (m_table->rowCount() <= 1) {
        QMessageBox::warning(this, tr("Cannot Remove"),
            tr("At least one column must remain."));
        return;
    }

    int row = m_table->currentRow();
    if (row >= 0) {
        m_table->removeRow(row);
        updateButtonStates();
    }
}

void ColumnListWidget::onAdd()
{
    // Get currently used columns
    QStringList used = columns();

    // Get available columns
    QStringList available = Config::availableColumns();

    // Build menu with available columns
    QMenu menu(this);
    for (const QString& col : available) {
        QAction* act = menu.addAction(col);
        // Mark already used columns (but still allow adding - user requested no duplicate check for now)
        if (used.contains(col)) {
            act->setText(col + " (*)");
        }
    }

    QAction* chosen = menu.exec(m_addBtn->mapToGlobal(QPoint(0, m_addBtn->height())));
    if (!chosen)
        return;

    QString colName = chosen->text().remove(" (*)");
    int defWidth = defaultWidth(colName);

    int newRow = m_table->rowCount();
    m_table->insertRow(newRow);

    auto* nameItem = new QTableWidgetItem(colName);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(newRow, 0, nameItem);

    auto* widthItem = new QTableWidgetItem(QString::number(defWidth));
    m_table->setItem(newRow, 1, widthItem);

    m_table->setCurrentCell(newRow, 0);
    updateButtonStates();
}

void ColumnListWidget::onSelectionChanged()
{
    updateButtonStates();
}

void ColumnListWidget::updateButtonStates()
{
    int row = m_table->currentRow();
    int count = m_table->rowCount();

    m_upBtn->setEnabled(row > 0);
    m_downBtn->setEnabled(row >= 0 && row < count - 1);
    m_removeBtn->setEnabled(count > 1 && row >= 0);
    m_addBtn->setEnabled(true);
}

// ============================================================================
// ConfigDialog implementation
// ============================================================================

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

    // Wayland: position offsets don't work
    if (isWayland()) {
        m_editorX->setEnabled(false);
        m_editorY->setEnabled(false);
    }

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

    // Wayland: position offsets don't work
    if (isWayland()) {
        m_viewerX->setEnabled(false);
        m_viewerY->setEnabled(false);
    }

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

    // Panel columns configuration
    auto* columnsGroup = new QGroupBox(tr("Panel Columns"), page);
    auto* columnsLayout = new QFormLayout(columnsGroup);

    m_leftColumns = new ColumnListWidget(columnsGroup);
    columnsLayout->addRow(tr("Left panel:"), m_leftColumns);

    m_rightColumns = new ColumnListWidget(columnsGroup);
    columnsLayout->addRow(tr("Right panel:"), m_rightColumns);

    layout->addWidget(columnsGroup);

    // Default sorting
    auto* sortGroup = new QGroupBox(tr("Default Sorting"), page);
    auto* sortLayout = new QFormLayout(sortGroup);

    // Left panel sorting
    auto* leftSortLayout = new QHBoxLayout();
    m_leftSortColumn = new QComboBox(sortGroup);
    m_leftSortColumn->addItem("Name");
    m_leftSortColumn->addItem("Ext");
    m_leftSortColumn->addItem("Size");
    m_leftSortColumn->addItem("Date");
    m_leftSortOrder = new QComboBox(sortGroup);
    m_leftSortOrder->addItem(tr("Ascending"));
    m_leftSortOrder->addItem(tr("Descending"));
    leftSortLayout->addWidget(m_leftSortColumn);
    leftSortLayout->addWidget(m_leftSortOrder);
    sortLayout->addRow(tr("Left panel:"), leftSortLayout);

    // Right panel sorting
    auto* rightSortLayout = new QHBoxLayout();
    m_rightSortColumn = new QComboBox(sortGroup);
    m_rightSortColumn->addItem("Name");
    m_rightSortColumn->addItem("Ext");
    m_rightSortColumn->addItem("Size");
    m_rightSortColumn->addItem("Date");
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

    // Tab limit settings
    auto* tabsGroup = new QGroupBox(tr("Tab Management"), page);
    auto* tabsLayout = new QFormLayout(tabsGroup);

    auto* tabsDescription = new QLabel(
        tr("Limit the number of unpinned tabs. When the limit is exceeded, "
           "the least recently used unpinned tabs are automatically closed."),
        tabsGroup);
    tabsDescription->setWordWrap(true);
    tabsLayout->addRow(tabsDescription);

    m_maxUnpinnedTabs = new QSpinBox(tabsGroup);
    m_maxUnpinnedTabs->setRange(1, 100);
    m_maxUnpinnedTabs->setSuffix(tr(" tabs"));
    tabsLayout->addRow(tr("Maximum unpinned tabs:"), m_maxUnpinnedTabs);

    layout->addWidget(tabsGroup);
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

    auto* sizeFormatLayout = new QHBoxLayout();
    auto* sizeFormatLabel = new QLabel(tr("Size display format:"), behaviorGroup);
    m_sizeFormat = new QComboBox(behaviorGroup);
    m_sizeFormat->addItem(tr("Decimal (1.5 M)"), 1);     // SizeFormat::Decimal
    m_sizeFormat->addItem(tr("Binary (1.5 Mi)"), 2);     // SizeFormat::Binary
    m_sizeFormat->addItem(tr("Precise (1'500'000)"), 0); // SizeFormat::Precise
    sizeFormatLayout->addWidget(sizeFormatLabel);
    sizeFormatLayout->addWidget(m_sizeFormat);
    sizeFormatLayout->addStretch();
    behaviorLayout->addLayout(sizeFormatLayout);

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

    // Panels page
    m_leftPanelStartDir->clear();
    m_rightPanelStartDir->clear();

    // Load columns from config
    m_leftColumns->setColumns(cfg.leftPanelColumns(), cfg.leftPanelProportions());
    m_rightColumns->setColumns(cfg.rightPanelColumns(), cfg.rightPanelProportions());

    // Load sorting from config (column name -> find in combo)
    int leftColIdx = m_leftSortColumn->findText(cfg.leftSortColumn());
    if (leftColIdx >= 0)
        m_leftSortColumn->setCurrentIndex(leftColIdx);
    m_leftSortOrder->setCurrentIndex(cfg.leftSortOrder());

    int rightColIdx = m_rightSortColumn->findText(cfg.rightSortColumn());
    if (rightColIdx >= 0)
        m_rightSortColumn->setCurrentIndex(rightColIdx);
    m_rightSortOrder->setCurrentIndex(cfg.rightSortOrder());

    // Remember initial sorting values to detect changes
    m_initialLeftSortColumn = cfg.leftSortColumn();
    m_initialLeftSortOrder = cfg.leftSortOrder();
    m_initialRightSortColumn = cfg.rightSortColumn();
    m_initialRightSortOrder = cfg.rightSortOrder();

    // History page
    m_maxHistorySize->setValue(cfg.maxHistorySize());
    m_maxUnpinnedTabs->setValue(cfg.maxUnpinnedTabs());

    // General page
    m_confirmExit->setChecked(cfg.confirmExit());
    m_showFunctionBar->setChecked(cfg.showFunctionBar());
    m_externalToolPath->setText(cfg.externalToolPath());

    // Size format: find index by data value
    int sizeFormatIdx = m_sizeFormat->findData(static_cast<int>(cfg.sizeFormat()));
    if (sizeFormatIdx >= 0)
        m_sizeFormat->setCurrentIndex(sizeFormatIdx);
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

    // Panels - sorting (use column name from combobox)
    QString newLeftCol = m_leftSortColumn->currentText();
    int newLeftOrd = m_leftSortOrder->currentIndex();
    QString newRightCol = m_rightSortColumn->currentText();
    int newRightOrd = m_rightSortOrder->currentIndex();

    cfg.setLeftSort(newLeftCol, newLeftOrd);
    cfg.setRightSort(newRightCol, newRightOrd);

    // Save panel columns
    QStringList leftCols = m_leftColumns->columns();
    QVector<double> leftProps = m_leftColumns->proportions();
    cfg.setLeftPanelColumns(leftCols, leftProps);
    emit columnsChanged(0, leftCols, leftProps);  // 0 = Left

    QStringList rightCols = m_rightColumns->columns();
    QVector<double> rightProps = m_rightColumns->proportions();
    cfg.setRightPanelColumns(rightCols, rightProps);
    emit columnsChanged(1, rightCols, rightProps);  // 1 = Right

    // Emit signals if sorting changed (column index for signal)
    if (newLeftCol != m_initialLeftSortColumn || newLeftOrd != m_initialLeftSortOrder) {
        emit sortingChanged(0, m_leftSortColumn->currentIndex(), newLeftOrd);  // 0 = Left
        m_initialLeftSortColumn = newLeftCol;
        m_initialLeftSortOrder = newLeftOrd;
    }
    if (newRightCol != m_initialRightSortColumn || newRightOrd != m_initialRightSortOrder) {
        emit sortingChanged(1, m_rightSortColumn->currentIndex(), newRightOrd);  // 1 = Right
        m_initialRightSortColumn = newRightCol;
        m_initialRightSortOrder = newRightOrd;
    }

    // History
    cfg.setMaxHistorySize(m_maxHistorySize->value());
    cfg.setMaxUnpinnedTabs(m_maxUnpinnedTabs->value());

    // General
    cfg.setConfirmExit(m_confirmExit->isChecked());
    cfg.setShowFunctionBar(m_showFunctionBar->isChecked());
    cfg.setExternalToolPath(m_externalToolPath->text());

    // Size format
    int sizeFormatValue = m_sizeFormat->currentData().toInt();
    cfg.setSizeFormat(static_cast<SizeFormat::SizeKind>(sizeFormatValue));

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