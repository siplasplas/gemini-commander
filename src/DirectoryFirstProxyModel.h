#ifndef DIRECTORYFIRSTPROXYMODEL_H
#define DIRECTORYFIRSTPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QFileSystemModel> // Needed to check isDir in .cpp file

class DirectoryFirstProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT // Although we do not use signals/slots, good practice

public:
    explicit DirectoryFirstProxyModel(QObject *parent = nullptr);

protected:
    // Key method to override
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;
};

#endif // DIRECTORYFIRSTPROXYMODEL_H