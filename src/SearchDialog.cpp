#include "SearchDialog.h"
#include "SearchWorker.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

#include "keys/ObjectRegistry.h"

SearchDialog::SearchDialog(const QString& startPath, QWidget* parent)
    : QDialog(parent)
    , m_startPath(startPath)
    , m_foundCount(0)
    , m_searchThread(nullptr)
    , m_searchWorker(nullptr)
{
    ObjectRegistry::add(this, "SearchGlobal");
    setWindowTitle(tr("Find Files"));
    resize(700, 500);

    setupUi();

    // Set start path
    m_searchInEdit->setText(m_startPath);
}

SearchDialog::~SearchDialog()
{
    if (m_searchThread) {
        if (m_searchWorker)
            m_searchWorker->stopSearch();
        m_searchThread->quit();
        m_searchThread->wait();
    }
}

void SearchDialog::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    createStandardTab();
    createAdvancedTab();
    createResultsTab();

    m_tabWidget->addTab(m_standardTab, tr("Standard"));
    m_tabWidget->addTab(m_advancedTab, tr("Advanced"));
    m_tabWidget->addTab(m_resultsTab, tr("Results"));

    mainLayout->addWidget(m_tabWidget, 1);

    // Control buttons - vertical layout on the right
    auto* buttonLayout = new QVBoxLayout();

    m_startButton = new QPushButton(tr("Start"), this);
    m_stopButton = new QPushButton(tr("Stop"), this);
    m_closeButton = new QPushButton(tr("Close"), this);

    m_stopButton->setEnabled(false);

    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(m_closeButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout, 0);

    // Connections
    connect(m_startButton, &QPushButton::clicked, this, &SearchDialog::onStartSearch);
    connect(m_stopButton, &QPushButton::clicked, this, &SearchDialog::onStopSearch);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_resultsTable, &QTableWidget::cellDoubleClicked,
            this, &SearchDialog::onResultDoubleClicked);
}

void SearchDialog::createStandardTab()
{
    m_standardTab = new QWidget();
    auto* layout = new QVBoxLayout(m_standardTab);

    // Search in
    auto* pathGroup = new QGroupBox(tr("Search in:"), m_standardTab);
    auto* pathLayout = new QHBoxLayout(pathGroup);

    m_searchInEdit = new QLineEdit(pathGroup);
    m_browseButton = new QPushButton(tr("Browse..."), pathGroup);

    pathLayout->addWidget(m_searchInEdit);
    pathLayout->addWidget(m_browseButton);

    layout->addWidget(pathGroup);

    // Search criteria
    auto* criteriaGroup = new QGroupBox(tr("Search criteria:"), m_standardTab);
    auto* criteriaLayout = new QVBoxLayout(criteriaGroup);

    // File name row
    auto* fileNameLayout = new QHBoxLayout();
    fileNameLayout->addWidget(new QLabel(tr("File name:")), 0);
    m_fileNameEdit = new QLineEdit(criteriaGroup);
    m_fileNameEdit->setPlaceholderText(tr("e.g., test or *.txt"));
    fileNameLayout->addWidget(m_fileNameEdit, 1);
    criteriaLayout->addLayout(fileNameLayout);

    // File name options row
    auto* fileNameOptionsLayout = new QHBoxLayout();
    fileNameOptionsLayout->addSpacing(20);
    m_partOfNameCheck = new QCheckBox(tr("Part of name"), criteriaGroup);
    m_partOfNameCheck->setChecked(true);  // default ON
    m_fileNameCaseSensitiveCheck = new QCheckBox(tr("Case sensitive"), criteriaGroup);
    fileNameOptionsLayout->addWidget(m_partOfNameCheck);
    fileNameOptionsLayout->addWidget(m_fileNameCaseSensitiveCheck);
    fileNameOptionsLayout->addStretch();
    criteriaLayout->addLayout(fileNameOptionsLayout);

    criteriaLayout->addSpacing(10);

    // Containing text row
    auto* textLayout = new QHBoxLayout();
    textLayout->addWidget(new QLabel(tr("Containing text:")), 0);
    m_containingTextEdit = new QLineEdit(criteriaGroup);
    m_containingTextEdit->setPlaceholderText(tr("Text to search in files (optional)"));
    textLayout->addWidget(m_containingTextEdit, 1);
    criteriaLayout->addLayout(textLayout);

    // Text options row
    auto* textOptionsLayout = new QHBoxLayout();
    textOptionsLayout->addSpacing(20);
    m_textCaseSensitiveCheck = new QCheckBox(tr("Case sensitive"), criteriaGroup);
    m_wholeWordsCheck = new QCheckBox(tr("Whole words only"), criteriaGroup);
    textOptionsLayout->addWidget(m_textCaseSensitiveCheck);
    textOptionsLayout->addWidget(m_wholeWordsCheck);
    textOptionsLayout->addStretch();
    criteriaLayout->addLayout(textOptionsLayout);

    layout->addWidget(criteriaGroup);
    layout->addStretch();

    // Browse button connection
    connect(m_browseButton, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this,
            tr("Select Directory"),
            m_searchInEdit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (!dir.isEmpty())
            m_searchInEdit->setText(dir);
    });
}

void SearchDialog::createAdvancedTab()
{
    m_advancedTab = new QWidget();
    auto* layout = new QVBoxLayout(m_advancedTab);

    // File size
    auto* sizeGroup = new QGroupBox(tr("File size:"), m_advancedTab);
    auto* sizeLayout = new QFormLayout(sizeGroup);

    m_minSizeEdit = new QLineEdit(sizeGroup);
    m_minSizeEdit->setPlaceholderText(tr("Leave empty for no limit"));
    sizeLayout->addRow(tr("Minimum (bytes):"), m_minSizeEdit);

    m_maxSizeEdit = new QLineEdit(sizeGroup);
    m_maxSizeEdit->setPlaceholderText(tr("Leave empty for no limit"));
    sizeLayout->addRow(tr("Maximum (bytes):"), m_maxSizeEdit);

    layout->addWidget(sizeGroup);
    layout->addStretch();
}

void SearchDialog::createResultsTab()
{
    m_resultsTab = new QWidget();
    auto* layout = new QVBoxLayout(m_resultsTab);

    // Results table
    m_resultsTable = new QTableWidget(m_resultsTab);
    m_resultsTable->setColumnCount(3);
    m_resultsTable->setHorizontalHeaderLabels({tr("Path"), tr("Size"), tr("Modified")});
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSortingEnabled(true);

    // Set column widths
    m_resultsTable->setColumnWidth(0, 400);
    m_resultsTable->setColumnWidth(1, 100);
    m_resultsTable->setColumnWidth(2, 150);

    layout->addWidget(m_resultsTable);

    // Status label
    m_statusLabel = new QLabel(tr("Ready"), m_resultsTab);
    layout->addWidget(m_statusLabel);
}

void SearchDialog::onStartSearch()
{
    // Validate input
    QString searchPath = m_searchInEdit->text().trimmed();
    if (searchPath.isEmpty()) {
        QMessageBox::warning(this, tr("Search"), tr("Please specify a directory to search in."));
        return;
    }

    QDir dir(searchPath);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("Search"), tr("The specified directory does not exist."));
        return;
    }

    // Clear previous results
    m_resultsTable->setRowCount(0);
    m_foundCount = 0;

    // Prepare search criteria
    SearchCriteria criteria;
    criteria.searchPath = searchPath;
    criteria.fileNamePattern = m_fileNameEdit->text().trimmed();
    if (criteria.fileNamePattern.isEmpty())
        criteria.fileNamePattern = "*";
    criteria.fileNameCaseSensitive = m_fileNameCaseSensitiveCheck->isChecked();
    criteria.partOfName = m_partOfNameCheck->isChecked();
    criteria.containingText = m_containingTextEdit->text();
    criteria.textCaseSensitive = m_textCaseSensitiveCheck->isChecked();
    criteria.wholeWords = m_wholeWordsCheck->isChecked();

    // Advanced criteria
    bool ok;
    if (!m_minSizeEdit->text().isEmpty()) {
        criteria.minSize = m_minSizeEdit->text().toLongLong(&ok);
        if (!ok) criteria.minSize = -1;
    }
    if (!m_maxSizeEdit->text().isEmpty()) {
        criteria.maxSize = m_maxSizeEdit->text().toLongLong(&ok);
        if (!ok) criteria.maxSize = -1;
    }

    // Create worker and thread
    m_searchThread = new QThread(this);
    m_searchWorker = new SearchWorker(criteria);
    m_searchWorker->moveToThread(m_searchThread);

    // Connect signals
    connect(m_searchThread, &QThread::started, m_searchWorker, &SearchWorker::startSearch);
    connect(m_searchWorker, &SearchWorker::resultFound, this, &SearchDialog::onResultFound);
    connect(m_searchWorker, &SearchWorker::searchFinished, this, &SearchDialog::onSearchFinished);
    connect(m_searchWorker, &SearchWorker::progressUpdate, this, &SearchDialog::onProgressUpdate);

    // Cleanup on finish
    connect(m_searchWorker, &SearchWorker::searchFinished, m_searchThread, &QThread::quit);
    connect(m_searchThread, &QThread::finished, m_searchWorker, &QObject::deleteLater);
    connect(m_searchThread, &QThread::finished, m_searchThread, &QObject::deleteLater);

    // Update UI
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText(tr("Searching..."));

    // Switch to results tab
    m_tabWidget->setCurrentWidget(m_resultsTab);

    // Start search
    m_searchThread->start();
}

void SearchDialog::onStopSearch()
{
    if (m_searchWorker)
        m_searchWorker->stopSearch();
}

void SearchDialog::onResultFound(const QString& path, qint64 size, const QDateTime& modified)
{
    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);

    // Path
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(path));

    // Size - store numeric value for proper sorting
    auto* sizeItem = new QTableWidgetItem();
    sizeItem->setData(Qt::DisplayRole, QString::number(size));
    sizeItem->setData(Qt::UserRole, size);  // Store as qint64 for numeric sorting
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_resultsTable->setItem(row, 1, sizeItem);

    // Modified date - store as timestamp for proper sorting
    auto* dateItem = new QTableWidgetItem();
    dateItem->setData(Qt::DisplayRole, modified.toString("yyyy-MM-dd hh:mm:ss"));
    dateItem->setData(Qt::UserRole, modified);  // Store as QDateTime for sorting
    m_resultsTable->setItem(row, 2, dateItem);
}

void SearchDialog::onProgressUpdate(int searchedFiles, int foundFiles)
{
    m_statusLabel->setText(tr("Searched %1 files, found %2").arg(searchedFiles).arg(foundFiles));
}

void SearchDialog::onSearchFinished()
{
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);

    // Get final count from table
    int finalCount = m_resultsTable->rowCount();
    m_statusLabel->setText(tr("Search finished. Found %1 file(s).").arg(finalCount));

    m_searchWorker = nullptr;
    m_searchThread = nullptr;
}

void SearchDialog::onResultDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    QTableWidgetItem* item = m_resultsTable->item(row, 0);
    if (!item)
        return;

    QString filePath = item->text();
    QFileInfo info(filePath);

    if (info.exists()) {
        // Open file with default application
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }
}