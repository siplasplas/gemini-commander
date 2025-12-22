#ifndef PANEL_H
#define PANEL_H

#include <QDir>
#include <QTableView>
#include <qfileinfo.h>
#include <QFileIconProvider>
#include <QMimeDatabase>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QProgressDialog>
#include <QVector>

class QStandardItem;
struct SearchResult;
QT_BEGIN_NAMESPACE
class QTableView;
class QStandardItemModel;
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

enum class FileType {
    Executable,  // ELF, scripts with shebang
    Text,        // .txt, .md, .cpp, .h, source code
    Image,       // .png, .jpg, .svg, .gif
    Archive,     // .zip, .tar.gz, .7z, .rar
    Audio,       // .mp3, .wav, .flac, .ogg
    Video,       // .mp4, .mkv, .avi, .webm
    Document,    // .pdf, .doc, .odt, .xls
    Hidden,      // files starting with .
    Unknown      // fallback
};


struct PanelEntry {
    QFileInfo info;
    QString branch;  // Relative path from base directory (used in Branch Mode)
    bool isMarked = false;
    EntryContentState contentState = EntryContentState::NotDirectory;
    std::size_t totalSizeBytes = 0;
    TotalSizeStatus hasTotalSize = TotalSizeStatus::Unknown;
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

enum Columns {
        COLUMN_ID = 0,
        COLUMN_NAME = 1,
        COLUMN_EXT =  2,
        COLUMN_SIZE = 3,
        COLUMN_DATE = 4,
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
    QStandardItemModel* model = nullptr;
    QString currentPath;
    QList<PanelEntry> entries;
    bool branchMode = false;  // Branch View mode (flat list of files from subdirectories)

    int sortColumn = COLUMN_NAME;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;

    FilePanel(Side side, QWidget* parent = nullptr);
    ~FilePanel() override;

    void loadDirectory();

    QString getRowName(int row) const;
    void selectEntryByName(const QString& fullName);
    void selectFirstEntry();
    void trigger(const QString &name);
    void activate(const QString &name);
    void addAllEntries();
    void trigger(const QModelIndex &index);
    void triggerCurrentEntry();
    void createNewDirectory(QWidget*dialogParent);
    void renameOrMoveEntry(QWidget* dialogParent = nullptr,
                       const QString& defaultTargetDir = QString());
    void toggleMarkOnCurrent(bool advanceRow);
    QStringList getMarkedNames() const;
    bool hasMarkedEntries() const;
    void rememberSelection();
    void restoreSelectionFromMemory();
    void styleActive();
    void styleInactive();

    struct CopyStats {
        quint64 totalBytes = 0;
        quint64 totalFiles = 0;
        quint64 totalDirs  = 0;
    };

    static void collectCopyStats(const QString &srcPath, CopyStats &stats, bool &ok, bool* cancelFlag = nullptr);
    static bool copyDirectoryRecursive(const QString &srcRoot, const QString &dstRoot, const CopyStats &stats,
                                QProgressDialog &progress, quint64 &bytesCopied, bool &userAbort);
    std::pair<PanelEntry*, int> currentEntryRow();
    void updateColumn(int row, PanelEntry& entry);
    QList<QStandardItem*> entryToRow(PanelEntry& entry);
    Side side() {return m_side;}
    void feedSearchResults(const QVector<SearchResult>& results, const QString& searchPath);

protected:
    void startDrag(Qt::DropActions supportedActions) override;

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

signals:
    void selectionChanged();
    void directoryChanged(const QString& path);
    void searchRequested(const QString& initialText);

private:
    static QIcon getIconForEntry(const QFileInfo& info, EntryContentState contentState);
    static FileType classifyFileType(const QFileInfo& info);
    static QIcon getIconForFileType(FileType type);
    EntryContentState ensureContentState(PanelEntry&entry)const;
    bool mixedHidden = true;//filenames with dot, are between others
    // Search UI and logic
    QDir *dir = nullptr;
    void sortEntries();
    void addFirstEntry(bool isRoot);
    void addEntries();
    QString normalizeForSearch(const QString& s) const;

    // Shared pattern history for select/unselect group
    static QStringList s_patternHistory;
    static QString showPatternDialog(QWidget* parent, const QString& title, const QString& label);
};

#endif //PANEL_H
