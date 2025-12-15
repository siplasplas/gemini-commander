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
#include <QLocale>
#include <algorithm>

#include "keys/ObjectRegistry.h"

// ============================================================================
// SearchResultsModel implementation
// ============================================================================

SearchResultsModel::SearchResultsModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int SearchResultsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_isSorted ? m_sortedIndices.size() : m_results.size();
}

int SearchResultsModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant SearchResultsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount() || index.column() >= ColumnCount)
        return QVariant();

    // Get actual result index (sorted or direct)
    int resultIndex = m_isSorted ? m_sortedIndices[index.row()] : index.row();

    // Bounds check for resultIndex
    if (resultIndex < 0 || resultIndex >= m_results.size())
        return QVariant();

    const SearchResult& result = m_results[resultIndex];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColumnPath:
                return result.path;
            case ColumnSize:
                return formatSize(result.size);
            case ColumnModified:
                return formatDateTime(result.modifiedTimestamp);
        }
    }

    return QVariant();
}

QVariant SearchResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case ColumnPath: return tr("Path");
            case ColumnSize: return tr("Size");
            case ColumnModified: return tr("Modified");
        }
    }

    return QVariant();
}

void SearchResultsModel::sort(int column, Qt::SortOrder order)
{
    if (m_results.isEmpty())
        return;

    // Optimization: if same column, just reverse the order instead of re-sorting
    bool sameColumn = (m_sortColumn == column && m_isSorted);
    bool justReverse = sameColumn && (m_sortOrder != order);

    // Store persistent indices and map them to actual data indices BEFORE sorting
    QModelIndexList oldPersistentIndexes = persistentIndexList();
    QVector<int> oldDataIndices;  // Map to actual data indices

    // Only process persistent indices if there are any (optimization for large datasets)
    if (!oldPersistentIndexes.isEmpty()) {
        oldDataIndices.reserve(oldPersistentIndexes.size());
        for (const QModelIndex& idx : oldPersistentIndexes) {
            int row = idx.row();
            if (row >= 0 && row < rowCount()) {
                // Get the actual data index BEFORE we modify m_sortedIndices
                int dataIndex = m_isSorted ? m_sortedIndices[row] : row;
                oldDataIndices.append(dataIndex);
            } else {
                oldDataIndices.append(-1);  // Invalid
            }
        }
    }

    emit layoutAboutToBeChanged();

    m_sortColumn = column;
    m_sortOrder = order;

    // Fast path: just reverse if same column, different order
    if (justReverse) {
        std::reverse(m_sortedIndices.begin(), m_sortedIndices.end());
    } else {
        updateSortedIndices();
    }

    // Update persistent indices only if there are any
    if (!oldPersistentIndexes.isEmpty()) {
        // Create reverse map: dataIndex -> newRow (for fast lookup)
        QHash<int, int> dataIndexToNewRow;
        dataIndexToNewRow.reserve(m_sortedIndices.size());
        for (int newRow = 0; newRow < m_sortedIndices.size(); ++newRow) {
            dataIndexToNewRow[m_sortedIndices[newRow]] = newRow;
        }

        // Update persistent indices to reflect new positions
        QModelIndexList newPersistentIndexes;
        newPersistentIndexes.reserve(oldPersistentIndexes.size());

        for (int i = 0; i < oldPersistentIndexes.size(); ++i) {
            const QModelIndex& oldIdx = oldPersistentIndexes[i];
            int dataIndex = oldDataIndices[i];

            if (dataIndex < 0 || dataIndex >= m_results.size())
                continue;

            // Find where this data item is in the NEW sorted order (O(1) lookup)
            auto it = dataIndexToNewRow.find(dataIndex);
            if (it != dataIndexToNewRow.end()) {
                int newRow = it.value();
                newPersistentIndexes.append(index(newRow, oldIdx.column()));
            }
        }

        changePersistentIndexList(oldPersistentIndexes, newPersistentIndexes);
    }

    emit layoutChanged();
}

void SearchResultsModel::addResult(const QString& path, qint64 size, qint64 modifiedTimestamp)
{
    // Add to raw data without sorting - just append
    int newRow = m_results.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_results.append({path, size, modifiedTimestamp});
    endInsertRows();
}

void SearchResultsModel::clear()
{
    beginResetModel();
    m_results.clear();
    m_sortedIndices.clear();
    m_sortColumn = -1;
    m_isSorted = false;
    endResetModel();
}

const SearchResult& SearchResultsModel::resultAt(int row) const
{
    int resultIndex = m_isSorted ? m_sortedIndices[row] : row;
    return m_results[resultIndex];
}

void SearchResultsModel::updateSortedIndices()
{
    // Initialize indices array (0, 1, 2, ...)
    m_sortedIndices.resize(m_results.size());
    for (int i = 0; i < m_results.size(); ++i)
        m_sortedIndices[i] = i;

    // Sort indices based on column data
    std::sort(m_sortedIndices.begin(), m_sortedIndices.end(),
        [this](int a, int b) {
            // Bounds check - protect against invalid indices
            if (a < 0 || a >= m_results.size() || b < 0 || b >= m_results.size()) {
                return a < b;  // Fallback comparison
            }

            const SearchResult& ra = m_results[a];
            const SearchResult& rb = m_results[b];

            bool less = false;
            switch (m_sortColumn) {
                case ColumnPath:
                    less = ra.path < rb.path;
                    break;
                case ColumnSize:
                    less = ra.size < rb.size;
                    break;
                case ColumnModified:
                    less = ra.modifiedTimestamp < rb.modifiedTimestamp;
                    break;
                default:
                    return a < b;  // Stable fallback
            }

            return m_sortOrder == Qt::AscendingOrder ? less : !less;
        });

    m_isSorted = true;
}

QString SearchResultsModel::formatSize(qint64 size) const
{
    // Format with thousands separators: 123'456'789
    QString numStr = QString::number(size);
    QString result;
    int count = 0;

    // Iterate from right to left, add separator every 3 digits
    for (int i = numStr.length() - 1; i >= 0; --i) {
        if (count == 3) {
            result.prepend('\'');
            count = 0;
        }
        result.prepend(numStr[i]);
        count++;
    }

    return result;
}

QString SearchResultsModel::formatDateTime(qint64 timestamp) const
{
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
    return dt.toString("yyyy-MM-dd hh:mm:ss");
}

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

    // Create model
    m_resultsModel = new SearchResultsModel(this);

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
    ObjectRegistry::add(m_tabWidget, "Tabs");
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

    connect(m_resultsView, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        onResultDoubleClicked(index.row(), index.column());
    });
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

    // Search type
    auto* typeGroup = new QGroupBox(tr("Search type:"), m_advancedTab);
    auto* typeLayout = new QVBoxLayout(typeGroup);

    m_directoriesOnlyCheck = new QCheckBox(tr("Directories only"), typeGroup);
    typeLayout->addWidget(m_directoriesOnlyCheck);

    layout->addWidget(typeGroup);
    layout->addStretch();
}

void SearchDialog::createResultsTab()
{
    m_resultsTab = new QWidget();
    auto* layout = new QVBoxLayout(m_resultsTab);

    // Results view with model
    m_resultsView = new QTableView(m_resultsTab);
    m_resultsView->setModel(m_resultsModel);
    m_resultsView->horizontalHeader()->setStretchLastSection(true);
    m_resultsView->horizontalHeader()->setSortIndicatorShown(true);
    m_resultsView->horizontalHeader()->setSectionsClickable(true);
    m_resultsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsView->setSortingEnabled(true);  // Allow user to sort by clicking headers

    // Set column widths
    m_resultsView->setColumnWidth(0, 400);
    m_resultsView->setColumnWidth(1, 100);
    m_resultsView->setColumnWidth(2, 150);

    layout->addWidget(m_resultsView);

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
    m_resultsModel->clear();
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
    criteria.directoriesOnly = m_directoriesOnlyCheck->isChecked();

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
    // Add result to model - just raw data, no sorting yet
    m_resultsModel->addResult(path, size, modified.toMSecsSinceEpoch());
}

void SearchDialog::onProgressUpdate(int searchedFiles, int foundFiles)
{
    m_statusLabel->setText(tr("Searched %1 files, found %2").arg(searchedFiles).arg(foundFiles));
}

void SearchDialog::onSearchFinished()
{
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);

    // Get final count from model
    int finalCount = m_resultsModel->resultCount();
    m_statusLabel->setText(tr("Search finished. Found %1 file(s).").arg(finalCount));

    m_searchWorker = nullptr;
    m_searchThread = nullptr;
}

void SearchDialog::onResultDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    if (row < 0 || row >= m_resultsModel->resultCount())
        return;

    // Get file path from model
    const SearchResult& result = m_resultsModel->resultAt(row);
    QFileInfo info(result.path);

    if (info.exists()) {
        // Open file with default application
        QDesktopServices::openUrl(QUrl::fromLocalFile(result.path));
    }
}