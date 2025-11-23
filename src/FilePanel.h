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
    void styleActive(QWidget *widget);
    void styleInactive(QWidget *widget);
private slots:
    void onPanelActivated(const QModelIndex &index);
    void onHeaderSectionClicked(int logicalIndex);
protected:
    void startDrag(Qt::DropActions supportedActions) override;
public:
    QStandardItemModel *model;
    QString currentPath;

    int sortColumn = COLUMN_NAME;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;

    explicit FilePanel(QSplitter *splitter);
    ~FilePanel() override;
    void active(bool active);
    void loadDirectory();
    QString getRowName(int row) const;
    // Select row by full file name (base + extension)
    bool selectEntryByName(const QString& fullName);
};

#endif //PANEL_H
