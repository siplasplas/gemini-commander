#ifndef PANEL_H
#define PANEL_H

#include <QModelIndex>
#include <QSplitter>
#include <QString>

QT_BEGIN_NAMESPACE
class QTableView;
class QStandardItemModel;
QT_END_NAMESPACE

class Panel: public QObject
{
    Q_OBJECT
    void styleActive(QWidget *widget);
    void styleInactive(QWidget *widget);

    int sortColumn = 0;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
private slots:
    void onPanelActivated(const QModelIndex &index);
    void onHeaderSectionClicked(int logicalIndex);
public:
    QTableView *tableView;
    QStandardItemModel *model;
    QString currentPath;
    explicit Panel(QSplitter *splitter);
    ~Panel() override;
    void active(bool active);
    void loadDirectory();
};



#endif //PANEL_H
