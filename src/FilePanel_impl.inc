bool FilePanel::doBranchViewWithSubdirs(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    // Toggle: if already in branch mode, exit it
    if (branchMode) {
        // Remember current selection
        QModelIndex idx = currentIndex();
        QString relPath = idx.isValid() ? getRowRelPath(idx.row()) : QString();

        // Get current entry to check if we should navigate to its branch directory
        auto p = currentEntryRow();
        if (p.first && !p.first->branch.isEmpty()) {
            // Navigate to the branch directory
            currentPath = currentPath + "/" + p.first->branch;
            QDir d(currentPath);
            currentPath = d.canonicalPath();  // Normalize path
        }

        branchMode = false;
        loadDirectory();
        selectEntryByRelPath(relPath);
        return true;
    }

    // Remember current selection before entering branch mode
    QModelIndex idx = currentIndex();
    QString relPath = idx.isValid() ? getRowRelPath(idx.row()) : QString();

    // Enter branch mode: scan all files recursively
    m_cancelOperation = false;
    KeyRouter::instance().setOperationInProgress(true);

    entries.clear();
    QString basePath = currentPath;
    if (!basePath.endsWith('/'))
        basePath += '/';

    QDirIterator it(currentPath,
                    QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);

    int count = 0;
    while (it.hasNext() && !m_cancelOperation) {
        it.next();
        QFileInfo info = it.fileInfo();

        // Calculate branch (relative path from currentPath to file's directory)
        QString filePath = info.absolutePath();
        QString branch;
        if (filePath.startsWith(basePath)) {
            branch = filePath.mid(basePath.length());
        }

        entries.append(PanelEntry(info, branch));

        // Process events periodically to allow ESC handling
        if (++count % 100 == 0) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
    }

    KeyRouter::instance().setOperationInProgress(false);

    if (m_cancelOperation) {
        // Cancelled - revert to normal mode
        entries.clear();
        loadDirectory();
        return true;
    }

    // Successfully scanned - enter branch mode
    branchMode = true;
    sortEntries();
    model->removeRows(0, model->rowCount());
    // No [..] entry in branch mode
    addEntries();
    setRootIndex(QModelIndex());
    selectEntryByRelPath(relPath);
    emit selectionChanged();

    return true;
}

bool FilePanel::doCancelOperation(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    // Only handle ESC if an operation is in progress
    // Check if any entry has InProgress status
    bool operationInProgress = false;
    for (const auto& entry : entries) {
        if (entry.hasTotalSize == TotalSizeStatus::InPogress) {
            operationInProgress = true;
            break;
        }
    }

    if (operationInProgress) {
        m_cancelOperation = true;
        qDebug() << "Cancel operation requested";
        return true;  // Consume ESC event
    }

    return false;  // Let ESC propagate to default handler
}

bool FilePanel::doCDTree(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doCDTree";
    return false;
}

bool FilePanel::doDeletePermanently(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    QModelIndex idx = currentIndex();
    if (!idx.isValid())
        return true;

    QString name = getRowRelPath(idx.row());
    if (name.isEmpty())
        return true;

    QDir dir(currentPath);
    QString fullPath = dir.absoluteFilePath(name);
    QFileInfo info(fullPath);
    if (!info.exists())
        return true;

    QString question = tr("PERMANENTLY delete '%1'? This cannot be undone!").arg(name);
    auto reply = QMessageBox::warning(
        window(), tr("Permanent Delete"), question,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );
    if (reply != QMessageBox::Yes)
        return true;

    bool ok = false;
    if (info.isDir()) {
        QDir d(fullPath);
        ok = d.removeRecursively();
    } else {
        ok = QFile::remove(fullPath);
    }

    if (!ok) {
        QMessageBox::warning(window(), tr("Error"),
            tr("Could not delete '%1'").arg(name));
    } else {
        int rowToSelect = idx.row();
        loadDirectory();
        // Select same index or last row if we deleted the last item
        int maxRow = model->rowCount() - 1;
        if (maxRow >= 0) {
            selectRow(qMin(rowToSelect, maxRow));
        }
    }
    return true;
}

bool FilePanel::doDeleteToTrash(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    QModelIndex idx = currentIndex();
    if (!idx.isValid())
        return true;

    QString name = getRowRelPath(idx.row());
    if (name.isEmpty())
        return true;

    QDir dir(currentPath);
    QString fullPath = dir.absoluteFilePath(name);
    QFileInfo info(fullPath);
    if (!info.exists())
        return true;

    QString question;
    bool useTrash = true;

    if (info.isDir()) {
        QDir d(fullPath);
        bool empty = d.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        if (empty) {
            question = tr("Delete selected empty dir '%1'?").arg(name);
            useTrash = false;
        } else {
            question = tr("Delete selected non empty dir '%1' into trashcan?").arg(name);
        }
    } else {
        question = tr("Delete selected '%1' into trashcan?").arg(name);
    }

    auto reply = QMessageBox::question(
        window(), tr("Delete"), question,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (reply != QMessageBox::Yes)
        return true;

    bool ok = false;
    if (info.isDir() && !useTrash) {
        ok = dir.rmdir(name);
    } else {
        ok = QFile::moveToTrash(fullPath);
    }

    if (!ok) {
        QMessageBox::warning(window(), tr("Error"),
            tr("Could not delete '%1'").arg(name));
    } else {
        int rowToSelect = idx.row();
        loadDirectory();
        // Select same index or last row if we deleted the last item
        int maxRow = model->rowCount() - 1;
        if (maxRow >= 0) {
            selectRow(qMin(rowToSelect, maxRow));
        }
    }
    return true;
}

bool FilePanel::doDirUp(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    QDir dir(currentPath);
    // If we are in the root directory – do nothing
    if (dir.isRoot()) {
        return true;
    }
    // Remember the name of the current directory (as in onPanelActivated)
    QString selectedName = dir.dirName();
    // Go up one directory
    dir.cdUp();
    currentPath = dir.absolutePath();
    // Reload panel content
    loadDirectory();
    selectEntryByName(selectedName);
    return true;
}


bool FilePanel::doFileProperties(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doFileProperties";
    return false;
}

bool FilePanel::doGoBack(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doGoBack";
    return false;
}

bool FilePanel::doGoForward(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doGoForward";
    return false;
}

bool FilePanel::doGoToFirstRow(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    int rows = model->rowCount();
    if (rows > 0) {
        QModelIndex idx = model->index(0, COLUMN_NAME);
        setCurrentIndex(idx);
        scrollTo(idx, QAbstractItemView::PositionAtTop);
    }
    return true;
}

bool FilePanel::doGoToLastRow(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    int rows = model->rowCount();
    if (rows > 0) {
        QModelIndex idx = model->index(rows - 1, COLUMN_NAME);
        setCurrentIndex(idx);
        scrollTo(idx, QAbstractItemView::PositionAtBottom);
    }
    return true;
}

bool FilePanel::doInvertSelection(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    const bool isRoot = dir && dir->isRoot();
    const int offset = isRoot ? 0 : 1;

    for (int i = 0; i < entries.size(); ++i) {
        entries[i].isMarked = !entries[i].isMarked;
        updateRowMarking(i + offset, entries[i].isMarked);
    }
    return true;
}

bool FilePanel::doJumpDownToFiles(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    jumpWithControl(+1);
    return true;
}

bool FilePanel::doJumpUpToDirectories(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    jumpWithControl(-1);
    return true;
}

bool FilePanel::doMakeDirectory(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    createNewDirectory(window());
    return true;
}

bool FilePanel::doOccupiedSpace(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doOccupiedSpace";
    return false;
}

bool FilePanel::doQuickView(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doQuickView";
    return false;
}

bool FilePanel::doRefresh(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    // Remember current position
    QModelIndex idx = currentIndex();
    int currentRow = idx.isValid() ? idx.row() : 0;
    QString currentName = idx.isValid() ? getRowRelPath(currentRow) : QString();

    // Reload directory
    loadDirectory();

    // Try to select by name first
    if (!currentName.isEmpty()) {
        QModelIndex start = model->index(0, COLUMN_NAME);
        QModelIndexList matches = model->match(
            start,
            Qt::UserRole,
            currentName,
            1,
            Qt::MatchExactly
        );
        if (!matches.isEmpty()) {
            QModelIndex newIdx = matches.first();
            setCurrentIndex(newIdx);
            scrollTo(newIdx);
            return true;
        }
    }

    // Name not found - fall back to same row index
    int maxRow = model->rowCount() - 1;
    if (maxRow >= 0) {
        int targetRow = qMin(currentRow, maxRow);
        QModelIndex newIdx = model->index(targetRow, COLUMN_NAME);
        setCurrentIndex(newIdx);
        scrollTo(newIdx);
    }

    return true;
}

bool FilePanel::doRestoreSelection(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doRestoreSelection";
    return false;
}

bool FilePanel::doSelectAll(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    const bool isRoot = dir && dir->isRoot();
    const int offset = isRoot ? 0 : 1;

    for (int i = 0; i < entries.size(); ++i) {
        entries[i].isMarked = true;
        updateRowMarking(i + offset, true);
    }
    return true;
}

bool FilePanel::doSelectGroup(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    QString pattern = showPatternDialog(
        window(),
        tr("Select Group"),
        tr("Select files matching pattern:")
    );
    if (pattern.isEmpty())
        return true;

    QRegularExpression regex(
        QRegularExpression::wildcardToRegularExpression(pattern),
        QRegularExpression::CaseInsensitiveOption
    );

    const bool isRoot = dir && dir->isRoot();
    const int offset = isRoot ? 0 : 1;

    for (int i = 0; i < entries.size(); ++i) {
        if (regex.match(entries[i].info.fileName()).hasMatch()) {
            entries[i].isMarked = true;
            updateRowMarking(i + offset, true);
        }
    }
    return true;
}

bool FilePanel::doSelectSameExtension(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doSelectSameExtension";
    return false;
}

bool FilePanel::doSortByDate(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    if (sortColumn == COLUMN_DATE) {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
    } else {
        sortColumn = COLUMN_DATE;
        sortOrder = Qt::DescendingOrder;  // newest first by default
    }
    horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    addAllEntries();
    return true;
}

bool FilePanel::doSortByExt(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    if (sortColumn == COLUMN_EXT) {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
    } else {
        sortColumn = COLUMN_EXT;
        sortOrder = Qt::AscendingOrder;
    }
    horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    addAllEntries();
    return true;
}

bool FilePanel::doSortByName(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    if (sortColumn == COLUMN_NAME) {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
    } else {
        sortColumn = COLUMN_NAME;
        sortOrder = Qt::AscendingOrder;
    }
    horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    addAllEntries();
    return true;
}

bool FilePanel::doSortBySize(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    if (sortColumn == COLUMN_SIZE) {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
    } else {
        sortColumn = COLUMN_SIZE;
        sortOrder = Qt::DescendingOrder;  // largest first by default
    }
    horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    addAllEntries();
    return true;
}

bool FilePanel::doToggleMarkDown(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    toggleMarkOnCurrent(true);
    return true;
}

bool FilePanel::doToggleMarkTotalSize(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    auto p = currentEntryRow();
    PanelEntry* entry = p.first;
    int row = p.second;

    // no entry or [..]
    if (!entry) {
        toggleMarkOnCurrent(false);
        return true;
    }

    // only directories are counted; file -> behavior as before
    if (entry->info.isDir()) {
        if (entry->hasTotalSize != TotalSizeStatus::Has) {
            CopyStats stats;
            bool ok = false;
            m_cancelOperation = false;  // Reset cancel flag

            // Block all keys except ESC during operation
            KeyRouter::instance().setOperationInProgress(true);

            entry->hasTotalSize = TotalSizeStatus::InPogress;
            updateColumn(row, *entry);
            collectCopyStats(entry->info.absoluteFilePath(), stats, ok, &m_cancelOperation);
            if (ok) {
                entry->totalSizeBytes = static_cast<std::size_t>(stats.totalBytes);
                entry->hasTotalSize = TotalSizeStatus::Has;
            } else {
                entry->hasTotalSize = TotalSizeStatus::Unknown;
            }
            updateColumn(row, *entry);

            // Unblock keys after operation
            KeyRouter::instance().setOperationInProgress(false);
        }
    }

    toggleMarkOnCurrent(false);
    return true;
}

bool FilePanel::doTotalSizes(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doTotalSizes";
    return false;
}

bool FilePanel::doUnselectAll(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    const bool isRoot = dir && dir->isRoot();
    const int offset = isRoot ? 0 : 1;

    for (int i = 0; i < entries.size(); ++i) {
        entries[i].isMarked = false;
        updateRowMarking(i + offset, false);
    }
    return true;
}

bool FilePanel::doUnselectGroup(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    QString pattern = showPatternDialog(
        window(),
        tr("Unselect Group"),
        tr("Unselect files matching pattern:")
    );
    if (pattern.isEmpty())
        return true;

    QRegularExpression regex(
        QRegularExpression::wildcardToRegularExpression(pattern),
        QRegularExpression::CaseInsensitiveOption
    );

    const bool isRoot = dir && dir->isRoot();
    const int offset = isRoot ? 0 : 1;

    for (int i = 0; i < entries.size(); ++i) {
        if (regex.match(entries[i].info.fileName()).hasMatch()) {
            entries[i].isMarked = false;
            updateRowMarking(i + offset, false);
        }
    }
    return true;
}
