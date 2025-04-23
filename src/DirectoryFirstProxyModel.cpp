#include "DirectoryFirstProxyModel.h"
#include <QFileSystemModel>

DirectoryFirstProxyModel::DirectoryFirstProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

bool DirectoryFirstProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    // Get the source model (we assume it's a QFileSystemModel)
    // In more complex code, dynamic_cast or qobject_cast would be useful
    const QFileSystemModel *fsModel = static_cast<const QFileSystemModel*>(sourceModel());
    if (!fsModel) {
        // If the source model is not set or has the wrong type, use the default comparison
        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }

    // Check if the elements are directories
    bool leftIsDir = fsModel->isDir(source_left);
    bool rightIsDir = fsModel->isDir(source_right);

    // Logic “Directories first”
    if (leftIsDir && !rightIsDir) {
        // Left is directory, right is file - left is “smaller” (goes to top)
        return true;
    }
    if (!leftIsDir && rightIsDir) {
        // Left is file, right is directory - left is “bigger” (goes down)
        return false;
    }

    // If both are of the same type (files or directories),
    // use the default comparison implemented in QSortFilterProxyModel,
    // which will take into account the sortColumn() and sortOrder() set for the proxy.
    return QSortFilterProxyModel::lessThan(source_left, source_right);
}