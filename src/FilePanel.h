#ifndef PANEL_H
#define PANEL_H

#include <QDir>
#include <QTableView>
#include <qfileinfo.h>
#include <QFileIconProvider>
#include <QMimeDatabase>
#include <QStyle>

QT_BEGIN_NAMESPACE
class QTableView;
class QStandardItemModel;
QT_END_NAMESPACE


enum class EntryContentState {
    NotDirectory,
    DirEmpty,
    DirNotEmpty,
    DirUnknown
};

struct PanelEntry {
    QFileInfo info;
    bool isMarked = false;
    EntryContentState contentState;
    PanelEntry() = default;
    explicit PanelEntry(const QFileInfo& fi)
        : info(fi)
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

class FilePanel : public QTableView
{
    Q_OBJECT
public:
    QStandardItemModel* model = nullptr;
    QString currentPath;

    int sortColumn = COLUMN_NAME;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;

    explicit FilePanel(QWidget* parent = nullptr);
    ~FilePanel() override;

    void active(bool active);
    void loadDirectory();

    QString getRowName(int row) const;
    void selectEntryByName(const QString& fullName);
    void addAllEntries();
    void triggerCurrentEntry();
    void createNewDirectory(QWidget*dialogParent);
    void renameOrMoveEntry(QWidget* dialogParent = nullptr,
                       const QString& defaultTargetDir = QString());

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onPanelActivated(const QModelIndex &index);
    void onHeaderSectionClicked(int logicalIndex);
    void onSearchTextChanged(const QString& text);
public slots:
    void updateSearch(const QString& text);
    void nextMatch();
    void prevMatch();
    void jumpWithControl(int direction);

private:
    int m_lastSearchRow = -1;
    QString m_lastSearchText;


signals:
    void selectionChanged();
    void directoryChanged(const QString& path);
    void searchRequested(const QString& initialText);

private:
   static QIcon iconForEntry(const QFileInfo& info);
   QIcon iconForExtension(const QString &ext, EntryContentState contentState);
   EntryContentState ensureContentState(PanelEntry&entry)const;

void styleActive();
    void styleInactive();

    bool mixedHidden = true;//filenames with dot, are between others
    // Search UI and logic
    QLineEdit* searchEdit = nullptr;
    QString lastSearchText;
    QList<PanelEntry> entries;
    QDir *dir = nullptr;
    void sortEntries();
    void addFirstEntry(bool isRoot);
    void addEntries();
    void initSearchEdit();
    void updateSearchGeometry();
    QString normalizeForSearch(const QString& s) const;
    bool findAndSelectPattern(const QString& pattern,
                              bool forward,
                              bool wrap,
                              int startRow);
};

#endif //PANEL_H
