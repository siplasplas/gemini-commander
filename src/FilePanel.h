#ifndef PANEL_H
#define PANEL_H

#include <QTableView>
#include <qfileinfo.h>
#include <QFileIconProvider>
#include <QMimeDatabase>
#include <QStyle>

QT_BEGIN_NAMESPACE
class QTableView;
class QStandardItemModel;
QT_END_NAMESPACE


enum Columns {
        COLUMN_ID = 0,
        COLUMN_NAME = 1,
        COLUMN_TYPE = 2,
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

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onPanelActivated(const QModelIndex &index);
    void onHeaderSectionClicked(int logicalIndex);
    void onSearchTextChanged(const QString& text);

signals:
    void selectionChanged();
    void directoryChanged(const QString& path);

private:
   static QIcon iconForEntry(const QFileInfo& info);
   QIcon iconForExtension(const QString &ext, bool isDir);

void styleActive();
    void styleInactive();

    bool mixedHidden = true;//filenames with dot, are between others
    // Search UI and logic
    QLineEdit* searchEdit = nullptr;
    QString lastSearchText;
    QFileInfoList entries;
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
