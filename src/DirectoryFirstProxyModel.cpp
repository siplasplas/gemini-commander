#include "DirectoryFirstProxyModel.h"
#include <QFileSystemModel>

DirectoryFirstProxyModel::DirectoryFirstProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

bool DirectoryFirstProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    const QFileSystemModel *fsModel = static_cast<const QFileSystemModel*>(sourceModel());
    if (!fsModel) {
        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }

    QString leftName = fsModel->fileName(source_left);
    QString rightName = fsModel->fileName(source_right);

    bool leftIsParent = leftName == "..";
    bool rightIsParent = rightName == "..";

    bool isAsc = sortOrder() == Qt::AscendingOrder;

    if (leftIsParent && !rightIsParent) {
        return isAsc;
    }
    if (!leftIsParent && rightIsParent) {
        return !isAsc;
    }

    bool leftIsDir = fsModel->isDir(source_left);
    bool rightIsDir = fsModel->isDir(source_right);

    if (leftIsDir && !rightIsDir) {
        return isAsc;
    }
    if (!leftIsDir && rightIsDir) {
        return !isAsc;
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

bool DirectoryFirstProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    const QFileSystemModel *fsModel = static_cast<const QFileSystemModel*>(sourceModel());
    if (!fsModel) {
        return true;
    }

    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    QString name = fsModel->fileName(idx);

    if (name == ".") {
        return false;
    }

    if (name == ".." && fsModel->fileInfo(source_parent).isRoot()) {
        return false;
    }

    return true;
}

QVariant DirectoryFirstProxyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    QModelIndex sourceIdx = mapToSource(index);

    if (role == Qt::DisplayRole && index.column() == 0) {
        const QFileSystemModel *fsModel = static_cast<const QFileSystemModel*>(sourceModel());
        QString name = fsModel->fileName(sourceIdx);
        if (name == "..") {
            return "[..]";
        }
    }

    return QSortFilterProxyModel::data(index, role);
}