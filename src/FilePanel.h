#ifndef PANEL_H
#define PANEL_H

#include "Archives.h"
#include "PanelFilter.h"

#include <QAbstractTableModel>
#include <QDir>
#include <QTableView>
#include <QTimer>
#include <qfileinfo.h>
#include <QFileIconProvider>
#include <QMimeDatabase>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QProgressDialog>
#include <QDateTime>
#include <QSet>
#include <QTemporaryDir>
#include <QVector>

#include <memory>

struct SearchResult;
QT_BEGIN_NAMESPACE
class QTableView;
QT_END_NAMESPACE


enum class Side {
    Left  = 0,
    Right = 1
};

enum class EntryContentState {
    NotDirectory,
    DirEmpty,
    DirNotEmpty,
    DirUnknown
};

enum class TotalSizeStatus {
    Unknown,
    InPogress,
    Has,
};


struct PanelEntry {
    QFileInfo info;
    QString branch;  // Relative path from base directory (used in Branch Mode)
                     // For archive mode: full path inside archive
    // Marks are the panel's (FilePanel::isMarked), not the entry's.
    EntryContentState contentState = EntryContentState::NotDirectory;
    std::size_t totalSizeBytes = 0;
    TotalSizeStatus hasTotalSize = TotalSizeStatus::Unknown;
    QDateTime archiveModTime;  // Modification time for archive entries
    // Set by FilePanel::sortEntries() for the Date column: the modification
    // time once per entry, not a QDateTime (with time zone) per comparison.
    qint64 sortTimeMs = 0;
    PanelEntry() = default;
    explicit PanelEntry(const QFileInfo& fi, const QString& branchPath = QString())
        : info(fi), branch(branchPath)
    {
        if (info.isDir())
            contentState = EntryContentState::DirUnknown;
        else
            contentState = EntryContentState::NotDirectory;
    }
};

// The entries of one directory listing, in the order they were read,
// shared by all panels and tabs showing the same directory with the same
// Show filter: each panel keeps only its own display order and marks.
// Reading the directory again (FilePanel::loadDirectory) updates it for all
// of them (reloaded()). The branch view, archives and search results have
// a private listing, not shared.
class DirListing : public QObject
{
    Q_OBJECT
public:
    QList<PanelEntry> entries;
    QDateTime dirTime;  // the directory's modification time when read

    // The shared listing for `key` (path and filter) that a panel holds, or
    // nullptr.
    static std::shared_ptr<DirListing> find(const QString& key);
    // A new listing, shared under `key`.
    static std::shared_ptr<DirListing> create(const QString& key);

signals:
    void reloaded();
};

// Column names are now strings: "Name", "Ext", "Size", "Date", "Attr"
// Each panel can have different columns in any order

// Forward declaration
class FilePanel;

// Virtual model that reads directly from FilePanel::entries()
// No data copying - only displays visible rows
class FilePanelModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit FilePanelModel(FilePanel* panel, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Call after sorting or changing entries
    void refresh();

    // Call after marking/unmarking single row
    void refreshRow(int row);

    // Helper: convert model row to entry index (-1 if [..] row)
    int rowToEntryIndex(int row) const;

    // Helper: convert entry index to model row
    int entryIndexToRow(int entryIndex) const;

    // Check if row 0 is the [..] entry
    bool hasParentEntry() const;

private:
    FilePanel* m_panel;
};

class MarkedItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};

class FilePanel : public QTableView
{
    Q_OBJECT
public:
#include "FilePanel_decl.inc"
    FilePanelModel* model = nullptr;
    QString currentPath;
    // The entries in the order they were read (the directory listing, shared
    // with other panels showing the directory); never sorted. The display
    // order is m_order (indices into entries), which sortEntries() sorts.
    // Rows/positions go through entryAt(); a loop over all entries whose
    // order does not matter (counts, sums) may use entries() directly.
    QList<PanelEntry>& entries() { return m_listing->entries; }
    const QList<PanelEntry>& entries() const { return m_listing->entries; }
    // Entry at display position `pos` (0 = first entry, [..] not counted).
    PanelEntry& entryAt(int pos) { return entries()[orderIndex(pos)]; }
    const PanelEntry& entryAt(int pos) const { return entries()[orderIndex(pos)]; }
    int entryCount() const { return int(entries().size()); }
    // Ctrl+T: the same directory, listing, sorting and Show filter as
    // `other`, without reading the directory or sorting again.
    void forkFrom(const FilePanel& other);

    // Marks are names (branch/name in the branch view), so a reload of the
    // same directory keeps them; another directory starts without marks.
    bool isMarked(const PanelEntry& entry) const;
    void setMarked(const PanelEntry& entry, bool marked);
    bool branchMode = false;  // Branch View mode (flat list of files from subdirectories)

    // Archive browsing mode
    bool insideArchive = false;
    QString archiveFilePath;      // Path to the archive file
    QString archiveCurrentDir;    // Current directory inside archive (empty = root)
    ArchiveContents archiveContents;

    QString sortColumn = "Date";  // Column name to sort by
    Qt::SortOrder sortOrder = Qt::DescendingOrder;

    // Column configuration - loaded from Config
    QStringList columns() const { return m_columns; }
    QVector<double> columnProportions() const { return m_columnProportions; }
    int columnIndex(const QString& name) const { return m_columns.indexOf(name); }
    void setColumns(const QStringList& columns, const QVector<double>& proportions);

    FilePanel(Side side, QWidget* parent = nullptr);
    ~FilePanel() override;

    void loadDirectory();
    // Reloads only when the directory was not loaded yet or changed on disk
    // since (its modification time: entries added, removed or renamed), e.g.
    // when its tab is shown again. A directory of 140,000 files takes ~0.3 s.
    void refreshIfStale();

    QString getRowName(int row) const;
    QString currentRelPath() const;
    QString getRowRelPath(int row) const;
    void selectEntryByName(const QString& fullName);
    void selectEntryByRelPath(const QString& relPath);
    void selectFirstEntry();
    void navigateToPath(const QString& path);
    // A path from outside (command line, D-Bus Open): a directory ending in
    // '/' (or `/`, `.`, `..`) is entered; one without it is selected in its
    // parent directory, as a file is.
    void openPath(const QString& path);
    void trigger(const QString &name);
    void run(const QString &name);
    void sortEntriesApplyModel();
    void trigger(const QModelIndex &index);
    void triggerCurrentEntry();
    void renameOrMoveEntry(QWidget* dialogParent = nullptr,
                       const QString& defaultTargetDir = QString());
    void toggleMarkOnCurrent(bool advanceRow);
    QStringList getMarkedNames() const;
    QStringList getMarkedPaths() const;
    QStringList getMarkedRelPaths() const;
    bool hasMarkedEntries() const;
    void rememberSelection();
    void restoreSelectionFromMemory();
    void styleActive();
    void styleInactive();

    std::pair<PanelEntry*, int> currentEntryRow();
    Side side() {return m_side;}
    void setSide(Side side) { m_side = side; }
    void feedSearchResults(const QVector<SearchResult>& results, const QString& searchPath);

    // Branch mode incremental updates (avoid full reload)
    bool removeEntryByRelPath(const QString& relPath);
    bool renameEntry(const QString& oldRelPath, const QString& newRelPath);
    bool updateEntryBranch(const QString& relPath, const QString& newBranch);
    bool addEntryFromPath(const QString& fullPath, const QString& branch = QString());

    // Show menu: which files the panel shows (and counts in directory sizes)
    const PanelFilter& filter() const { return m_filter; }
    void setFilter(const PanelFilter& filter);
    /// Keeps the entries the filter shows, for iterating below the panel directory.
    std::function<bool(const QFileInfo&)> filterPredicate() const;

    // Archive browsing mode
    void enterArchive(const QString& archivePath);
    void exitArchive();
    void loadArchiveDirectory();
    // F3 on a file inside the archive: the start of it (at most
    // kArchivePreviewBytes) in a temporary file, removed when leaving the
    // archive. Empty with `error` set when it failed.
    static constexpr qint64 kArchivePreviewBytes = 1024 * 1024;
    QString archivePreviewFile(const PanelEntry& entry, QString& error);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onHeaderSectionClicked(int logicalIndex);
public slots:
    void updateSearch(const QString& text);
    void nextMatch();
    void prevMatch();
    void jumpWithControl(int direction);

private:
    Side m_side;
    int m_lastSearchRow = -1;
    QString m_lastSearchText;
    void updateRowMarking(int row, bool marked);
    int m_lastSelectedRow = -1;
    bool m_cancelOperation = false;  // Flag for canceling long-running operations
    // What loadDirectory() listed last, for refreshIfStale().
    QString m_loadedPath;
    QDateTime m_loadedDirTime;
    PanelFilter m_filter;
    void reloadKeepingCurrent();

    // The listing shown (shared, see DirListing) and this panel's display
    // order: indices into its entries (4 bytes per entry), sorted by
    // sortEntries() instead of the entries themselves.
    std::shared_ptr<DirListing> m_listing = std::make_shared<DirListing>();
    QVector<int> m_order;
    int orderIndex(int pos) const {
        // Entries filled without sortEntries() yet: read order.
        return m_order.size() == entries().size() ? m_order[pos] : pos;
    }
    // Key of the shared listing: path and Show filter (it changes the entries).
    QString listingKey() const;
    // Shows `listing`, following its reloads by other panels.
    void attachListing(std::shared_ptr<DirListing> listing);
    // A new listing of this panel only (branch view, archive, search results).
    QList<PanelEntry>& privateEntries();
    // Another panel read the shared directory again: sort, keep the marks of
    // names still there and the current entry.
    void onListingReloaded();
    bool m_readingListing = false;  // this panel is the one reading it
    // Marked entries by name, see isMarked().
    QSet<QString> m_marked;
    static QString markKey(const PanelEntry& entry);
    // Keeps only the marks of names still listed (after a reload).
    void pruneMarks();
    // The previews of files inside the archive (archivePreviewFile).
    std::unique_ptr<QTemporaryDir> m_archivePreviewDir;

signals:
    void selectionChanged();
    void directoryChanged(const QString& path);
    void searchRequested(const QString& initialText);
    void goBackRequested();
    void goForwardRequested();
    void visibleFilesChanged(Side side, const QStringList& paths);

private:
    static QIcon getIconForEntry(const QFileInfo& info, EntryContentState contentState);
    EntryContentState ensureContentState(PanelEntry& entry) const;
    bool mixedHidden = true;  // filenames with dot, are between others
    // Search UI and logic
    QDir *dir = nullptr;
    void sortEntries();
    QString normalizeForSearch(const QString& s) const;

    // Shared pattern history for select/unselect group
    static QStringList s_patternHistory;
    static QString showPatternDialog(QWidget* parent, const QString& title, const QString& label);

    // Visible files tracking for file watcher
    QTimer* m_visibilityDebounceTimer = nullptr;
    void scheduleVisibleFilesUpdate();
    void emitVisibleFiles();
    QStringList getVisibleFilePaths() const;

    // Column names and proportional widths
    QStringList m_columns;  // e.g. {"Name", "Ext", "Size", "Date"}
    QVector<double> m_columnProportions;
    void updateColumnProportions();

    // FilePanelModel needs access to private members
    friend class FilePanelModel;
    friend class MainWindow;

protected:
    void scrollContentsBy(int dx, int dy) override;
    void resizeEvent(QResizeEvent* event) override;

public:
    // Update single entry info (for file watcher)
    bool refreshEntryByPath(const QString& filePath);
};

#endif //PANEL_H
