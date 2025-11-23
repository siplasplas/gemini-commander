#ifndef PANEL_H
#define PANEL_H

#include <QModelIndex>
#include <QSplitter>
#include <QString>

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

class Panel: public QObject
{
    Q_OBJECT
    void styleActive(QWidget *widget);
    void styleInactive(QWidget *widget);
private slots:
    void onPanelActivated(const QModelIndex &index);
    void onHeaderSectionClicked(int logicalIndex);
public:
    QTableView *tableView;
    QStandardItemModel *model;
    QString currentPath;

    int sortColumn = COLUMN_NAME;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;

    explicit Panel(QSplitter *splitter);
    ~Panel() override;
    void active(bool active);
    void loadDirectory();
};

#endif //PANEL_H
