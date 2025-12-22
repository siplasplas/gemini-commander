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
            case ColumnName:
                return result.name;
            case ColumnDir:
                return result.dir;
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
            case ColumnDir: return tr("Dir");
            case ColumnName: return tr("Name");
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
    // Split path into directory and name
    QFileInfo info(path);
    QString dir = info.path();
    QString name = info.fileName();

    // Add to raw data without sorting - just append
    int newRow = m_results.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_results.append({dir, name, size, modifiedTimestamp});
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
                case ColumnName:
                    less = ra.name < rb.name;
                    break;
                case ColumnDir:
                    less = ra.dir < rb.dir;
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
    , m_hasResults(false)
{
    ObjectRegistry::add(this, "SearchDialog");
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

    connect(m_resultsView, &QTableView::activated, this, [this](const QModelIndex& index) {
        onResultActivated(index.row());
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

    // File name row with Not checkbox
    auto* fileNameLayout = new QHBoxLayout();
    fileNameLayout->addWidget(new QLabel(tr("File name:")), 0);
    m_fileNameEdit = new QLineEdit(criteriaGroup);
    m_fileNameEdit->setPlaceholderText(tr("e.g., test or *.txt"));
    fileNameLayout->addWidget(m_fileNameEdit, 1);

    // Not checkbox for filename
    m_negateFileNameCheck = new QCheckBox(tr("Not"), criteriaGroup);
    m_negateFileNameCheck->setToolTip(tr("Invert filename match"));
    fileNameLayout->addWidget(m_negateFileNameCheck, 0);

    criteriaLayout->addLayout(fileNameLayout);

    // File name options row: Case sensitive, Part of name
    auto* fileNameOptionsLayout = new QHBoxLayout();
    fileNameOptionsLayout->addSpacing(20);
    m_fileNameCaseSensitiveCheck = new QCheckBox(tr("Case sensitive"), criteriaGroup);
    m_partOfNameCheck = new QCheckBox(tr("Part of name"), criteriaGroup);
    m_partOfNameCheck->setChecked(true);  // default ON

    fileNameOptionsLayout->addWidget(m_fileNameCaseSensitiveCheck);
    fileNameOptionsLayout->addWidget(m_partOfNameCheck);
    fileNameOptionsLayout->addStretch();

    criteriaLayout->addLayout(fileNameOptionsLayout);

    // Item type row (replaces "Directories only")
    auto* itemTypeLayout = new QHBoxLayout();
    itemTypeLayout->addSpacing(20);
    itemTypeLayout->addWidget(new QLabel(tr("Item type:"), criteriaGroup));

    m_itemTypeCombo = new QComboBox(criteriaGroup);
    m_itemTypeCombo->addItem(tr("Files and directories"));  // index 0
    m_itemTypeCombo->addItem(tr("Files only"));             // index 1
    m_itemTypeCombo->addItem(tr("Directories only"));       // index 2
    m_itemTypeCombo->setCurrentIndex(0);  // default: Files and directories

    itemTypeLayout->addWidget(m_itemTypeCombo);
    itemTypeLayout->addStretch();

    criteriaLayout->addLayout(itemTypeLayout);

    // Hide "Part of name" when filename contains wildcard (*)
    connect(m_fileNameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool hasWildcard = text.contains('*');
        m_partOfNameCheck->setVisible(!hasWildcard);
        if (hasWildcard) {
            m_partOfNameCheck->setChecked(false);  // disable part-of-name matching when using wildcards
        }
    });

    criteriaLayout->addSpacing(10);

    // Containing text row with Not checkbox
    auto* textLayout = new QHBoxLayout();
    textLayout->addWidget(new QLabel(tr("Containing text:")), 0);
    m_containingTextEdit = new QLineEdit(criteriaGroup);
    m_containingTextEdit->setPlaceholderText(tr("Text to search in files (optional)"));
    textLayout->addWidget(m_containingTextEdit, 1);

    // Not checkbox for text content
    m_negateContainingTextCheck = new QCheckBox(tr("Not"), criteriaGroup);
    m_negateContainingTextCheck->setToolTip(tr("Invert text match"));
    textLayout->addWidget(m_negateContainingTextCheck, 0);

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

    // Search in results checkbox (shown only when results exist)
    m_searchInResultsCheck = new QCheckBox(tr("Search in results"), m_standardTab);
    m_searchInResultsCheck->setToolTip(tr("Filter existing results instead of full filesystem search"));
    m_searchInResultsCheck->setVisible(false);  // Initially hidden
    layout->addWidget(m_searchInResultsCheck);

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

    // File size (existing)
    auto* sizeGroup = new QGroupBox(tr("File size:"), m_advancedTab);
    auto* sizeLayout = new QFormLayout(sizeGroup);

    m_minSizeEdit = new QLineEdit(sizeGroup);
    m_minSizeEdit->setPlaceholderText(tr("Leave empty for no limit"));
    sizeLayout->addRow(tr("Minimum (bytes):"), m_minSizeEdit);

    m_maxSizeEdit = new QLineEdit(sizeGroup);
    m_maxSizeEdit->setPlaceholderText(tr("Leave empty for no limit"));
    sizeLayout->addRow(tr("Maximum (bytes):"), m_maxSizeEdit);

    layout->addWidget(sizeGroup);

    // File type (NEW)
    auto* fileTypeGroup = new QGroupBox(tr("File type:"), m_advancedTab);
    auto* fileTypeLayout = new QVBoxLayout(fileTypeGroup);

    // Text file row
    auto* textFileLayout = new QHBoxLayout();
    m_textFileCheck = new QCheckBox(tr("Text file"), fileTypeGroup);
    m_negateTextFileCheck = new QCheckBox(tr("Not"), fileTypeGroup);
    textFileLayout->addWidget(m_textFileCheck);
    textFileLayout->addWidget(m_negateTextFileCheck);
    textFileLayout->addStretch();
    fileTypeLayout->addLayout(textFileLayout);

    // ELF binary row
    auto* elfLayout = new QHBoxLayout();
    m_elfBinaryCheck = new QCheckBox(tr("ELF binary"), fileTypeGroup);
    m_negateElfBinaryCheck = new QCheckBox(tr("Not"), fileTypeGroup);
    elfLayout->addWidget(m_elfBinaryCheck);
    elfLayout->addWidget(m_negateElfBinaryCheck);
    elfLayout->addStretch();
    fileTypeLayout->addLayout(elfLayout);

    layout->addWidget(fileTypeGroup);

    // File attributes (NEW)
    auto* attributesGroup = new QGroupBox(tr("File attributes:"), m_advancedTab);
    auto* attributesLayout = new QFormLayout(attributesGroup);

    m_executableBitsCombo = new QComboBox(attributesGroup);
    m_executableBitsCombo->addItem(tr("Not specified"));                    // index 0
    m_executableBitsCombo->addItem(tr("Executable"));                       // index 1
    m_executableBitsCombo->addItem(tr("Not executable"));                   // index 2
    m_executableBitsCombo->addItem(tr("Owner+Group+Other executable"));    // index 3
    m_executableBitsCombo->setCurrentIndex(0);  // default: Not specified

    attributesLayout->addRow(tr("Executable bits:"), m_executableBitsCombo);

    layout->addWidget(attributesGroup);

    // Script detection (NEW)
    auto* scriptGroup = new QGroupBox(tr("Script detection:"), m_advancedTab);
    auto* scriptLayout = new QHBoxLayout(scriptGroup);

    m_shebangCheck = new QCheckBox(tr("Has shebang (#!)"), scriptGroup);
    m_negateShebangCheck = new QCheckBox(tr("Not"), scriptGroup);

    scriptLayout->addWidget(m_shebangCheck);
    scriptLayout->addWidget(m_negateShebangCheck);
    scriptLayout->addStretch();

    layout->addWidget(scriptGroup);

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
    m_resultsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsView->setSortingEnabled(true);  // Allow user to sort by clicking headers

    // Set column widths: Dir, Name, Size, Modified
    m_resultsView->setColumnWidth(0, 250);  // Dir
    m_resultsView->setColumnWidth(1, 200);  // Name
    m_resultsView->setColumnWidth(2, 90);   // Size
    m_resultsView->setColumnWidth(3, 130);  // Modified

    layout->addWidget(m_resultsView);

    // Sync selection with current index after sorting (layout changes)
    connect(m_resultsModel, &QAbstractItemModel::layoutChanged, this, [this]() {
        QModelIndex current = m_resultsView->currentIndex();
        if (current.isValid()) {
            m_resultsView->selectionModel()->select(
                current,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
            );
        }
    });

    // Bottom row: status label and feed button
    auto* bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Ready"), m_resultsTab);
    bottomLayout->addWidget(m_statusLabel, 1);

    m_feedToListboxButton = new QPushButton(tr("Feed to listbox"), m_resultsTab);
    m_feedToListboxButton->setEnabled(false);
    bottomLayout->addWidget(m_feedToListboxButton, 0);

    layout->addLayout(bottomLayout);

    connect(m_feedToListboxButton, &QPushButton::clicked, this, [this]() {
        int count = m_resultsModel->resultCount();
        if (count == 0)
            return;

        QVector<SearchResult> results;
        results.reserve(count);
        for (int i = 0; i < count; ++i) {
            results.append(m_resultsModel->resultAt(i));
        }

        hide();
        emit requestFeedToListbox(results, m_searchInEdit->text());
    });
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

    // Check if "Search in results" mode
    bool searchInResults = m_searchInResultsCheck->isVisible() &&
                          m_searchInResultsCheck->isChecked();

    // Collect previous results if in search-in-results mode
    QVector<QString> previousResults;
    if (searchInResults) {
        int count = m_resultsModel->resultCount();
        previousResults.reserve(count);
        for (int i = 0; i < count; ++i) {
            const SearchResult& result = m_resultsModel->resultAt(i);
            previousResults.append(result.dir + "/" + result.name);
        }
    }

    // Clear results only if not searching in results
    if (!searchInResults) {
        m_resultsModel->clear();
    }
    m_foundCount = 0;

    // ─────────────────────────────────────────────────────────
    // Prepare search criteria
    // ─────────────────────────────────────────────────────────
    SearchCriteria criteria;
    criteria.searchPath = searchPath;

    // File name pattern
    criteria.fileNamePattern = m_fileNameEdit->text().trimmed();
    if (criteria.fileNamePattern.isEmpty())
        criteria.fileNamePattern = "*";
    criteria.fileNameCaseSensitive = m_fileNameCaseSensitiveCheck->isChecked();
    criteria.partOfName = m_partOfNameCheck->isChecked();
    criteria.negateFileName = m_negateFileNameCheck->isChecked();

    // Containing text
    criteria.containingText = m_containingTextEdit->text();
    criteria.textCaseSensitive = m_textCaseSensitiveCheck->isChecked();
    criteria.wholeWords = m_wholeWordsCheck->isChecked();
    criteria.negateContainingText = m_negateContainingTextCheck->isChecked();

    // File size
    bool ok;
    if (!m_minSizeEdit->text().isEmpty()) {
        criteria.minSize = m_minSizeEdit->text().toLongLong(&ok);
        if (!ok) criteria.minSize = -1;
    }
    if (!m_maxSizeEdit->text().isEmpty()) {
        criteria.maxSize = m_maxSizeEdit->text().toLongLong(&ok);
        if (!ok) criteria.maxSize = -1;
    }

    // Item type filter (replaces directoriesOnly)
    switch (m_itemTypeCombo->currentIndex()) {
        case 0:
            criteria.itemTypeFilter = ItemTypeFilter::FilesAndDirectories;
            break;
        case 1:
            criteria.itemTypeFilter = ItemTypeFilter::FilesOnly;
            break;
        case 2:
            criteria.itemTypeFilter = ItemTypeFilter::DirectoriesOnly;
            break;
    }

    // File type filters
    criteria.filterTextFiles = m_textFileCheck->isChecked();
    criteria.negateTextFiles = m_negateTextFileCheck->isChecked();
    criteria.filterELFBinaries = m_elfBinaryCheck->isChecked();
    criteria.negateELFBinaries = m_negateElfBinaryCheck->isChecked();

    // Executable bits filter
    switch (m_executableBitsCombo->currentIndex()) {
        case 0:
            criteria.executableBits = SearchCriteria::ExecutableBitsFilter::NotSpecified;
            break;
        case 1:
            criteria.executableBits = SearchCriteria::ExecutableBitsFilter::Executable;
            break;
        case 2:
            criteria.executableBits = SearchCriteria::ExecutableBitsFilter::NotExecutable;
            break;
        case 3:
            criteria.executableBits = SearchCriteria::ExecutableBitsFilter::AllExecutable;
            break;
    }

    // Shebang filter
    criteria.filterShebang = m_shebangCheck->isChecked();
    criteria.negateShebang = m_negateShebangCheck->isChecked();

    // Search in results mode
    criteria.searchInResults = searchInResults;
    criteria.previousResultPaths = previousResults;

    // ─────────────────────────────────────────────────────────
    // Create worker and thread
    // ─────────────────────────────────────────────────────────
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

    if (searchInResults) {
        m_statusLabel->setText(tr("Filtering results..."));
    } else {
        m_statusLabel->setText(tr("Searching..."));
    }

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

    // Show "Search in results" checkbox and enable "Feed to listbox" if we have results
    m_hasResults = (finalCount > 0);
    m_searchInResultsCheck->setVisible(m_hasResults);
    m_feedToListboxButton->setEnabled(m_hasResults);

    m_searchWorker = nullptr;
    m_searchThread = nullptr;
}

void SearchDialog::onResultActivated(int row)
{
    if (row < 0 || row >= m_resultsModel->resultCount())
        return;

    const SearchResult& result = m_resultsModel->resultAt(row);
    hide();
    emit requestGoToFile(result.dir, result.name);
}

QString SearchDialog::currentPanelPath() const
{
    QModelIndex idx = m_resultsView->currentIndex();
    if (!idx.isValid())
        return {};

    int row = idx.row();
    if (row < 0 || row >= m_resultsModel->resultCount())
        return {};

    const SearchResult& result = m_resultsModel->resultAt(row);
    return result.dir + "/" + result.name;
}

bool SearchDialog::doEdit(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    QString fullPath = currentPanelPath();
    if (fullPath.isEmpty())
        return true;

    QFileInfo info(fullPath);
    if (!info.isFile())
        return true;

    emit requestEdit(fullPath);
    return true;
}

bool SearchDialog::doView(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    QString fullPath = currentPanelPath();
    if (fullPath.isEmpty())
        return true;

    QFileInfo info(fullPath);
    if (!info.isFile())
        return true;

    emit requestView(fullPath);
    return true;
}

