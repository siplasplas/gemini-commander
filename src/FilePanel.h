#ifndef PANEL_H
#define PANEL_H

#include <QModelIndex>
#include <QSplitter>
#include <QString>
#include <QTableView>

QT_BEGIN_NAMESPACE
class QTableView;
class QStandardItemModel;
QT_END_NAMESPACE


enum Columns {
        COLUMN_ID = 0,
        COLUMN_NAME = 1,
        COLUMN_EXT = 2,
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

    explicit FilePanel(QSplitter* splitter);
    ~FilePanel() override;

    void active(bool active);
    void loadDirectory();

    QString getRowName(int row) const;
    bool selectEntryByName(const QString& fullName);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onPanelActivated(const QModelIndex &index);
    void onHeaderSectionClicked(int logicalIndex);
    void onSearchTextChanged(const QString& text);

private:
    void styleActive();
    void styleInactive();

    // Search UI and logic
    QLineEdit* searchEdit = nullptr;
    QString lastSearchText;

    void initSearchEdit();
    QString normalizeForSearch(const QString& s) const;
    bool findAndSelectPattern(const QString& pattern,
                              bool forward,
                              bool wrap,
                              int startRow);
};

#endif //PANEL_H
