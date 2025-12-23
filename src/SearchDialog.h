#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QThread>
#include <QAbstractTableModel>
#include <QVector>

class SearchWorker;

// Raw search result data
struct SearchResult {
    QString dir;   // directory path
    QString name;  // file/directory name
    qint64 size;
    qint64 modifiedTimestamp;  // QDateTime as msecsSinceEpoch for efficient sorting
};

// Custom model for search results - memory efficient, sortable
class SearchResultsModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColumnDir = 0,
        ColumnName = 1,
        ColumnSize = 2,
        ColumnModified = 3,
        ColumnCount = 4
    };

    explicit SearchResultsModel(QObject* parent = nullptr);

    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Sorting support
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // Data management
    void addResult(const QString& path, qint64 size, qint64 modifiedTimestamp);
    void clear();
    int resultCount() const { return m_results.size(); }
    const SearchResult& resultAt(int row) const;

private:
    QVector<SearchResult> m_results;
    QVector<int> m_sortedIndices;  // Indices into m_results (for sorting without moving data)
    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    bool m_isSorted = false;

    void updateSortedIndices();
    QString formatSize(qint64 size) const;
    QString formatDateTime(qint64 timestamp) const;
};

class SearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit SearchDialog(const QString& startPath, QWidget* parent = nullptr);
    ~SearchDialog();
    Q_INVOKABLE bool doView(QObject *obj, QKeyEvent *keyEvent);
    Q_INVOKABLE bool doEdit(QObject *obj, QKeyEvent *keyEvent);

    // Get currently selected file path (dir + name)
    QString currentPanelPath() const;

signals:
    void requestEdit(const QString& filePath);
    void requestView(const QString& filePath);
    void requestGoToFile(const QString& dir, const QString& name);
    void requestFeedToListbox(const QVector<SearchResult>& results, const QString& searchPath);

private slots:
    void onStartSearch();
    void onStopSearch();
    void onResultFound(const QString& path, qint64 size, const QDateTime& modified);
    void onSearchFinished();
    void onProgressUpdate(int searchedFiles, int foundFiles);
    void onResultActivated(int row);

private:
    void setupUi();
    void createStandardTab();
    void createAdvancedTab();
    void createResultsTab();

    // UI elements
    QTabWidget* m_tabWidget;

    // Standard tab
    QWidget* m_standardTab;
    QLineEdit* m_searchInEdit;
    QPushButton* m_browseButton;
    QLineEdit* m_fileNameEdit;
    QCheckBox* m_fileNameCaseSensitiveCheck;
    QComboBox* m_itemTypeCombo;
    QCheckBox* m_partOfNameCheck;
    QCheckBox* m_negateFileNameCheck;
    QLineEdit* m_containingTextEdit;
    QCheckBox* m_textCaseSensitiveCheck;
    QCheckBox* m_wholeWordsCheck;
    QCheckBox* m_negateContainingTextCheck;
    QCheckBox* m_searchInResultsCheck;

    // Advanced tab
    QWidget* m_advancedTab;
    QLineEdit* m_minSizeEdit;
    QLineEdit* m_maxSizeEdit;

    // File content filter (unified combobox)
    QComboBox* m_fileContentFilterCombo;

    // File attributes group
    QComboBox* m_executableBitsCombo;

    // Results tab
    QWidget* m_resultsTab;
    QTableView* m_resultsView;
    SearchResultsModel* m_resultsModel;
    QLabel* m_statusLabel;
    QPushButton* m_feedToListboxButton;

    // Control buttons
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_closeButton;

    // Search worker
    QThread* m_searchThread;
    SearchWorker* m_searchWorker;

    QString m_startPath;
    int m_foundCount;
    bool m_hasResults;  // Track if we have previous results (for search-in-results mode)
};