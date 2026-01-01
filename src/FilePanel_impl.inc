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
    model->refresh();
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
    emit goBackRequested();
    return true;
}

bool FilePanel::doGoForward(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    emit goForwardRequested();
    return true;
}

bool FilePanel::doGoToFirstRow(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    int rows = model->rowCount();
    if (rows > 0) {
        QModelIndex idx = model->index(0, 0);
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
        QModelIndex idx = model->index(rows - 1, 0);
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
    emit selectionChanged();
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

bool FilePanel::doOccupiedSpace(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    qDebug() << "handler: doOccupiedSpace";
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
    selectEntryByRelPath(currentName);

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
    emit selectionChanged();
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
    emit selectionChanged();
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
    QString currentRelPath = getRowRelPath(currentIndex().row());
    if (sortColumn == "Date") {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
    } else {
        sortColumn = "Date";
        sortOrder = Qt::DescendingOrder;  // newest first by default
    }
    horizontalHeader()->setSortIndicator(columnIndex(sortColumn), sortOrder);
    sortEntriesApplyModel();
    selectEntryByRelPath(currentRelPath);
    return true;
}

bool FilePanel::doSortByExt(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    QString currentRelPath = getRowRelPath(currentIndex().row());
    if (sortColumn == "Ext") {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
    } else {
        sortColumn = "Ext";
        sortOrder = Qt::AscendingOrder;
    }
    horizontalHeader()->setSortIndicator(columnIndex(sortColumn), sortOrder);
    sortEntriesApplyModel();
    selectEntryByRelPath(currentRelPath);
    return true;
}

bool FilePanel::doSortByName(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    QString currentRelPath = getRowRelPath(currentIndex().row());
    if (sortColumn == "Name") {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
    } else {
        sortColumn = "Name";
        sortOrder = Qt::AscendingOrder;
    }
    horizontalHeader()->setSortIndicator(columnIndex(sortColumn), sortOrder);
    sortEntriesApplyModel();
    selectEntryByRelPath(currentRelPath);
    return true;
}

bool FilePanel::doSortBySize(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    QString currentRelPath = getRowRelPath(currentIndex().row());
    if (sortColumn == "Size") {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
    } else {
        sortColumn = "Size";
        sortOrder = Qt::DescendingOrder;  // largest first by default
    }
    horizontalHeader()->setSortIndicator(columnIndex(sortColumn), sortOrder);
    sortEntriesApplyModel();
    selectEntryByRelPath(currentRelPath);
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
            FileOperations::CopyStats stats;
            bool ok = false;
            m_cancelOperation = false;  // Reset cancel flag

            // Block all keys except ESC during operation
            KeyRouter::instance().setOperationInProgress(true);

            entry->hasTotalSize = TotalSizeStatus::InPogress;
            model->refreshRow(row);
            FileOperations::collectCopyStats(entry->info.absoluteFilePath(), stats, ok, &m_cancelOperation);
            if (ok) {
                entry->totalSizeBytes = static_cast<std::size_t>(stats.totalBytes);
                entry->hasTotalSize = TotalSizeStatus::Has;
            } else {
                entry->hasTotalSize = TotalSizeStatus::Unknown;
            }
            model->refreshRow(row);

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

    m_cancelOperation = false;
    KeyRouter::instance().setOperationInProgress(true);

    const int count = model->rowCount();
    for (int row = 0; row < count; ++row) {
        if (m_cancelOperation)
            break;

        if (row >= entries.size())
            break;

        PanelEntry& entry = entries[row];

        // Skip non-directories
        if (!entry.info.isDir())
            continue;

        // Skip [..] entry
        if (entry.info.fileName() == "..")
            continue;

        // Skip already marked entries
        if (entry.isMarked)
            continue;

        // Skip if already has total size
        if (entry.hasTotalSize == TotalSizeStatus::Has)
            continue;

        // Calculate size
        FileOperations::CopyStats stats;
        bool ok = false;

        entry.hasTotalSize = TotalSizeStatus::InPogress;
        model->refreshRow(row);
        QApplication::processEvents();

        collectCopyStats(entry.info.absoluteFilePath(), stats, ok, &m_cancelOperation);

        if (m_cancelOperation) {
            entry.hasTotalSize = TotalSizeStatus::Unknown;
            model->refreshRow(row);
            break;
        }

        if (ok) {
            entry.totalSizeBytes = static_cast<std::size_t>(stats.totalBytes);
            entry.hasTotalSize = TotalSizeStatus::Has;
        } else {
            entry.hasTotalSize = TotalSizeStatus::Unknown;
        }
        model->refreshRow(row);

        // Mark the entry after calculating size
        entry.isMarked = true;
        updateRowMarking(row, true);
        model->refreshRow(row);
        viewport()->update();
        QApplication::processEvents();
    }

    KeyRouter::instance().setOperationInProgress(false);
    emit selectionChanged();
    return true;
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
    emit selectionChanged();
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
    emit selectionChanged();
    return true;
}
