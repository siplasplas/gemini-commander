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
private slots:
    void onPanelActivated(const QModelIndex &index);
public:
    QTableView *tableView;
    QStandardItemModel *model;
    QString currentPath;
    explicit Panel(QSplitter *splitter);
    ~Panel();
    void active(bool active);
    void loadDirectory();
};



#endif //PANEL_H
