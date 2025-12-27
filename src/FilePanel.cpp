#include <algorithm>
#include <unicode/translit.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QDrag>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QStorageInfo>
#include <QUrl>
#include <QVBoxLayout>
#include "Config.h"
#include "FilePanel.h"
#include "FileIconResolver.h"
#include "quitls.h"

#include <QProcess>
#include <QStandardPaths>
#include "SearchDialog.h"
#include "SizeFormat.h"
#include "SortedDirIterator.h"
#include "keys/KeyRouter.h"

namespace {

// Parse command line into program and arguments, handling quotes
// Returns: {program, arguments}
std::pair<QString, QStringList> parseCommandLine(const QString &cmdLine) {
    QStringList parts;
    QString current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;

    for (int i = 0; i < cmdLine.length(); ++i) {
        QChar c = cmdLine[i];

        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\' && !inSingleQuote) {
            escaped = true;
            continue;
        }

        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            continue;
        }

        if (c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }

        if (c.isSpace() && !inSingleQuote && !inDoubleQuote) {
            if (!current.isEmpty()) {
                parts.append(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.isEmpty()) {
        parts.append(current);
    }

    if (parts.isEmpty()) {
        return {{}, {}};
    }

    QString program = parts.takeFirst();
    return {program, parts};
}

// Find executable in PATH or return absolute path if it exists
QString resolveExecutable(const QString &program, const QString &workingDir) {
    // If program contains path separator, treat as path
    if (program.contains('/')) {
        QString path = program;
        // Handle relative paths (e.g., ./run.sh)
        if (!QDir::isAbsolutePath(path)) {
            path = QDir(workingDir).absoluteFilePath(program);
        }
        QFileInfo info(path);
        if (info.exists() && info.isExecutable()) {
            return info.absoluteFilePath();
        }
        return {};
    }

    // Search in PATH using Qt's standard mechanism
    QString found = QStandardPaths::findExecutable(program);
    if (!found.isEmpty()) {
        return found;
    }

    return {};
}

// Check if file is executable (has execute permission)
bool isExecutableFile(const QString &path) {
    QFileInfo info(path);
    return info.exists() && info.isFile() && info.isExecutable();
}

} // anonymous namespace

// Static pattern history shared between select/unselect group dialogs
QStringList FilePanel::s_patternHistory = {"*"};

QString FilePanel::showPatternDialog(QWidget *parent, const QString &title, const QString &label) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    auto *layout = new QVBoxLayout(&dialog);

    auto *labelWidget = new QLabel(label, &dialog);
    layout->addWidget(labelWidget);

    auto *comboBox = new QComboBox(&dialog);
    comboBox->setEditable(true);
    comboBox->addItems(s_patternHistory);
    comboBox->setCurrentIndex(0);
    comboBox->lineEdit()->selectAll();
    layout->addWidget(comboBox);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    comboBox->setFocus();

    if (dialog.exec() != QDialog::Accepted)
        return {};

    QString pattern = comboBox->currentText().trimmed();
    if (pattern.isEmpty())
        return {};

    // Add to history if not already present, move to front if exists
    int idx = s_patternHistory.indexOf(pattern);
    if (idx > 0) {
        s_patternHistory.move(idx, 0);
    } else if (idx < 0) {
        s_patternHistory.prepend(pattern);
        // Keep history limited
        while (s_patternHistory.size() > 20)
            s_patternHistory.removeLast();
    }

    return pattern;
}

QString stripLeadingDot(const QString &s) {
    if (!s.isEmpty() && s.startsWith('.'))
        return s.mid(1);
    return s;
}

void MarkedItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index); // takes into account QSS, selection, etc.

    // Get color from Qt::ForegroundRole (e.g., red for marked)
    QVariant fgVar = index.data(Qt::ForegroundRole);
    if (fgVar.canConvert<QBrush>()) {
        QBrush brush = qvariant_cast<QBrush>(fgVar);
        QColor col = brush.color();

        // Ustaw kolor tekstu zarówno dla normalnego, jak i zaznaczonego stanu
        opt.palette.setColor(QPalette::Text, col);
        opt.palette.setColor(QPalette::HighlightedText, col);
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

// ============================================================================
// FilePanelModel - virtual model reading directly from FilePanel::entries
// ============================================================================

FilePanelModel::FilePanelModel(FilePanel *panel, QObject *parent) : QAbstractTableModel(parent), m_panel(panel) {
}

bool FilePanelModel::hasParentEntry() const {
    // [..] entry exists when: not in branch mode AND not in root directory
    if (m_panel->branchMode)
        return false;
    if (!m_panel->dir)
        return false;
    return !m_panel->dir->isRoot();
}

int FilePanelModel::rowToEntryIndex(int row) const {
    if (hasParentEntry()) {
        if (row == 0)
            return -1; // [..] row
        return row - 1;
    }
    return row;
}

int FilePanelModel::entryIndexToRow(int entryIndex) const {
    if (hasParentEntry())
        return entryIndex + 1;
    return entryIndex;
}

int FilePanelModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    int count = m_panel->entries.size();
    if (hasParentEntry())
        count += 1; // +1 for [..] row
    return count;
}

int FilePanelModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return 5; // ID, Name, Ext, Size, Date
}

QVariant FilePanelModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return {};

    const int row = index.row();
    const int col = index.column();
    const int entryIdx = rowToEntryIndex(row);

    // Handle [..] row
    if (entryIdx < 0) {
        if (role == Qt::DisplayRole) {
            switch (col) {
                case COLUMN_ID:
                    return QString();
                case COLUMN_NAME:
                    return QStringLiteral("[..]");
                case COLUMN_EXT:
                    return QString();
                case COLUMN_SIZE:
                    return QStringLiteral("<DIR>");
                case COLUMN_DATE: {
                    QFileInfo info(".");
                    return info.lastModified().toString("yyyy-MM-dd hh:mm");
                }
            }
        }
        if (role == Qt::DecorationRole && col == COLUMN_NAME) {
            return m_panel->style()->standardIcon(QStyle::SP_FileDialogToParent);
        }
        if (role == Qt::UserRole && col == COLUMN_NAME) {
            return QString(); // empty fullName for [..]
        }
        return {};
    }

    // Normal entry
    if (entryIdx >= m_panel->entries.size())
        return {};

    PanelEntry &entry = m_panel->entries[entryIdx];

    if (role == Qt::DisplayRole) {
        auto [base, ext] = splitFileName(entry.info);
        switch (col) {
            case COLUMN_ID:
                return QString();
            case COLUMN_NAME:
                return base;
            case COLUMN_EXT:
                return ext;
            case COLUMN_SIZE: {
                if (!entry.info.isDir()) {
                    return QString::fromStdString(SizeFormat::formatSize(entry.info.size(), false));
                } else if (entry.hasTotalSize == TotalSizeStatus::Has) {
                    return QString::fromStdString(SizeFormat::formatSize(entry.totalSizeBytes, false));
                } else if (entry.hasTotalSize == TotalSizeStatus::InPogress) {
                    return QStringLiteral("....");
                }
                return QStringLiteral("<DIR>");
            }
            case COLUMN_DATE:
                return entry.info.lastModified().toString("yyyy-MM-dd hh:mm");
        }
    }

    if (role == Qt::DecorationRole && col == COLUMN_NAME) {
        EntryContentState state = m_panel->ensureContentState(entry);
        return FilePanel::getIconForEntry(entry.info, state);
    }

    if (role == Qt::UserRole && col == COLUMN_NAME) {
        // Full filename for selection/search
        auto [base, ext] = splitFileName(entry.info);
        if (ext.isEmpty())
            return base;
        return base + "." + ext;
    }

    if (role == Qt::ForegroundRole && entry.isMarked) {
        return QBrush(Qt::red);
    }

    return {};
}

QVariant FilePanelModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
        case COLUMN_ID:
            return QStringLiteral("id");
        case COLUMN_NAME:
            return QStringLiteral("Name");
        case COLUMN_EXT:
            return QStringLiteral("Ext");
        case COLUMN_SIZE:
            return QStringLiteral("Size");
        case COLUMN_DATE:
            return QStringLiteral("Date");
    }
    return {};
}

Qt::ItemFlags FilePanelModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    // Enable drag for all entries except [..]
    int entryIdx = rowToEntryIndex(index.row());
    if (entryIdx >= 0) {
        flags |= Qt::ItemIsDragEnabled;
    }

    return flags;
}

void FilePanelModel::refresh() {
    beginResetModel();
    endResetModel();
}

void FilePanelModel::refreshRow(int row) {
    if (row < 0 || row >= rowCount())
        return;
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

void FilePanel::sortEntries() {
    // --------------------------
    // Sorting (TC-like)
    // --------------------------
    std::sort(entries.begin(), entries.end(), [this](const PanelEntry &c, const PanelEntry &d) {
        auto a = c.info;
        auto b = d.info;
        const bool aDir = a.isDir();
        const bool bDir = b.isDir();

        // Directories always on top
        if (aDir != bDir)
            return aDir && !bDir;

        auto lessCI = [](const QString &x, const QString &y) { return x.compare(y, Qt::CaseInsensitive) < 0; };
        auto greaterCI = [](const QString &x, const QString &y) { return x.compare(y, Qt::CaseInsensitive) > 0; };

        const bool asc = (sortOrder == Qt::AscendingOrder);

        auto cmpNames = [&](bool ascLocal) {
            QString na = a.fileName();
            QString nb = b.fileName();
            if (mixedHidden) {
                na = stripLeadingDot(na);
                nb = stripLeadingDot(nb);
            }
            return ascLocal ? lessCI(na, nb) : greaterCI(na, nb);
        };

        switch (sortColumn) {

            case COLUMN_NAME:
                return cmpNames(asc);

            case COLUMN_EXT:
                if (aDir && bDir) {
                    return cmpNames(asc);
                } else if (!aDir && !bDir) {
                    // Use splitFileName to handle hidden files consistently
                    auto [baseA, ea] = splitFileName(a);
                    auto [baseB, eb] = splitFileName(b);
                    int cmp = ea.compare(eb, Qt::CaseInsensitive);
                    if (cmp != 0)
                        return asc ? (cmp < 0) : (cmp > 0);
                    return cmpNames(asc);
                } else {
                    return cmpNames(asc);
                }

            case COLUMN_SIZE:
                if (aDir && bDir) {
                    const bool aHas = c.hasTotalSize == TotalSizeStatus::Has;
                    const bool bHas = d.hasTotalSize == TotalSizeStatus::Has;

                    // 1) directories with calculated size always at the top,
                    // regardless of asc/desc
                    if (aHas != bHas) {
                        return aHas; // true < false → counted before uncountable
                    }

                    // 2) both have their size calculated → we sort by totalSizeBytes
                    if (aHas && bHas) {
                        if (c.totalSizeBytes != d.totalSizeBytes)
                            return asc ? (c.totalSizeBytes < d.totalSizeBytes) : (c.totalSizeBytes > d.totalSizeBytes);
                        return cmpNames(asc);
                    }

                    if (c.info.size() != d.info.size())
                        return asc ? (c.info.size() < d.info.size()) : (c.info.size() > d.info.size());
                    return cmpNames(asc);
                } else if (!aDir && !bDir) {
                    if (a.size() != b.size())
                        return asc ? (a.size() < b.size()) : (a.size() > b.size());
                    return cmpNames(asc);
                }
                return aDir; // dirs before files
            case COLUMN_DATE: {
                QDateTime da = a.lastModified();
                QDateTime db = b.lastModified();
                if (da != db)
                    return asc ? (da < db) : (da > db);
                return cmpNames(asc);
            }

            default:
                return cmpNames(asc);
        }
    });
}

void FilePanel::addAllEntries() {
    QString selectedName;
    auto rows = selectionModel()->selectedRows();
    if (!rows.isEmpty())
        selectedName = getRowName(rows.first().row());
    sortEntries();
    model->refresh();
    selectEntryByName(selectedName);
    scheduleVisibleFilesUpdate();
}

void FilePanel::trigger(const QModelIndex &index) {
    if (!index.isValid())
        return;
    trigger(getRowName(index.row()));
}

void FilePanel::triggerCurrentEntry() {
    auto rows = selectionModel()->selectedRows();
    trigger(rows.first());
}

void FilePanel::loadDirectory() {
    dir = new QDir(currentPath);
    if (!dir->exists()) {
        return;
    }
    entries.clear();
    QDirIterator it(currentPath, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();
        PanelEntry entry(info);
        entries.append(entry);
    }
    addAllEntries();
    emit directoryChanged(currentPath);
    emit selectionChanged();
}

QString FilePanel::getRowName(int row) const {
    if (row < 0 || row >= model->rowCount())
        return {};
    QModelIndex idx = model->index(row, COLUMN_NAME);
    return model->data(idx, Qt::UserRole).toString();
}

QString FilePanel::getRowRelPath(int row) const {
    if (!branchMode)
        return getRowName(row);

    // In branch mode, row maps directly to entry index
    if (row < 0 || row >= entries.size())
        return {};

    const PanelEntry &entry = entries[row];
    if (entry.branch.isEmpty())
        return entry.info.fileName();
    return entry.branch + "/" + entry.info.fileName();
}

void FilePanel::selectEntryByRelPath(const QString &relPath) {
    if (relPath.isEmpty()) {
        selectFirstEntry();
        return;
    }

    // Search entries for matching branch + filename
    for (int i = 0; i < entries.size(); ++i) {
        const PanelEntry &entry = entries[i];
        QString entryRelPath;
        if (entry.branch.isEmpty())
            entryRelPath = entry.info.fileName();
        else
            entryRelPath = entry.branch + "/" + entry.info.fileName();

        if (entryRelPath == relPath) {
            // In branch mode, row == entry index; otherwise account for [..]
            int row = branchMode ? i : i + 1;
            m_lastSelectedRow = row;
            if (hasFocus())
                restoreSelectionFromMemory();
            return;
        }
    }

    // Fallback: try to match just the filename
    QString fileName = relPath.section('/', -1);
    selectEntryByName(fileName);
}

void FilePanel::selectEntryByName(const QString &fullName) {
    m_lastSelectedRow = -1;
    // For "[..]" / going up we expect empty fullName
    if (fullName.isEmpty()) {
        if (currentPath != "/" && model->rowCount() > 0)
            m_lastSelectedRow = 0;
    } else {
        QModelIndex start = model->index(0, COLUMN_NAME);
        QModelIndexList matches = model->match(start,
                                               Qt::UserRole, // search fullName stored in UserRole
                                               fullName,
                                               1, // first match only
                                               Qt::MatchExactly);

        if (matches.isEmpty())
            return;

        QModelIndex selectIndex = matches.first();
        m_lastSelectedRow = selectIndex.row();
    }
    if (hasFocus())
        restoreSelectionFromMemory();
}

void FilePanel::selectFirstEntry() {
    if (model->rowCount() > 0) {
        m_lastSelectedRow = 0;
        if (hasFocus())
            restoreSelectionFromMemory();
    }
}

void FilePanel::navigateToPath(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists())
        return;

    if (info.isDir()) {
        // It's a directory - navigate to it
        currentPath = info.absoluteFilePath();
        loadDirectory();
        selectFirstEntry();
    } else if (info.isFile()) {
        // It's a file - navigate to parent directory and select the file
        currentPath = info.absolutePath();
        loadDirectory();
        selectEntryByName(info.fileName());
    }
}

void FilePanel::trigger(const QString &name) {
    // Handle branch mode navigation
    if (branchMode) {
        auto p = currentEntryRow();
        if (p.first) {
            PanelEntry *entry = p.first;
            if (entry->info.isDir()) {
                // Enter directory - exit branch mode and navigate there
                branchMode = false;
                currentPath = entry->info.absoluteFilePath();
                loadDirectory();
                selectFirstEntry();
            } else {
                // Open file with full path
                run(entry->info.absoluteFilePath());
            }
        }
        return;
    }

    // Normal mode navigation
    QString selectedName;
    QDir dir(currentPath);
    if (name.isEmpty()) {
        // Going up: select the previous directory name after load
        selectedName = dir.dirName(); // basename of current path
        dir.cdUp();
        currentPath = dir.absolutePath();
    } else {
        QFileInfo info(dir.absoluteFilePath(name));
        if (info.isDir()) {
            dir.cd(name);
            currentPath = dir.absolutePath();
            selectedName = ""; // select parent entry when going down
        } else {
            // Regular file: open with system default application
            selectedName = name;
            run(qEscapePathForShell(name));
        }
    }
    loadDirectory();
    selectEntryByName(selectedName);
}

void FilePanel::run(const QString &commandLine) {
    if (commandLine.isEmpty())
        return;

    auto [program, args] = parseCommandLine(commandLine);
    if (program.isEmpty())
        return;

    // Handle built-in shell command: cd
    if (program == "cd") {
        QString targetDir;
        if (args.isEmpty()) {
            // cd without argument = go to home directory
            targetDir = QDir::homePath();
        } else {
            targetDir = args.first();
            // Handle relative paths
            if (!QDir::isAbsolutePath(targetDir)) {
                targetDir = QDir(currentPath).absoluteFilePath(targetDir);
            }
        }

        QDir newDir(targetDir);
        if (newDir.exists()) {
            currentPath = newDir.absolutePath();
            loadDirectory();
            selectEntryByName({});
        }
        return;
    }

    // Try to resolve as executable (with path or in PATH)
    QString execPath = resolveExecutable(program, currentPath);

    if (!execPath.isEmpty()) {
        // It's an executable - run it without waiting
        QProcess::startDetached(execPath, args, currentPath);
        return;
    }

    // Not an executable command - treat as a file to open with default app
    // Build absolute path from currentPath if needed
    QString absPath;
    if (QDir::isAbsolutePath(program)) {
        absPath = program;
    } else {
        absPath = QDir(currentPath).absoluteFilePath(program);
    }

    QFileInfo info(absPath);
    if (info.exists()) {
        // Check if it's an executable file (even without execute permission set)
        if (info.isFile() && info.isExecutable()) {
            if (!QProcess::startDetached(absPath, args, currentPath))
                QDesktopServices::openUrl(QUrl::fromLocalFile(absPath)); // incorrectly set attribute executable
        } else {
            // Open with default application (documents, images, etc.)
            QDesktopServices::openUrl(QUrl::fromLocalFile(absPath));
        }
        // Note: loadDirectory/selectEntryByName is called by trigger() if needed
    }
}

FilePanel::FilePanel(Side side, QWidget *parent) : QTableView(parent), m_side(side) {
    model = new FilePanelModel(this, this);
    currentPath = QDir::currentPath();
    setModel(model);
    setItemDelegate(new MarkedItemDelegate(this));

    // Debounce timer for visible files tracking (file watcher)
    m_visibilityDebounceTimer = new QTimer(this);
    m_visibilityDebounceTimer->setSingleShot(true);
    m_visibilityDebounceTimer->setInterval(1000);
    connect(m_visibilityDebounceTimer, &QTimer::timeout, this, &FilePanel::emitVisibleFiles);

    hideColumn(COLUMN_ID);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    verticalHeader()->hide();

    QFont f("Ubuntu", 11);
    f.setStyleHint(QFont::SansSerif); // fallback if Ubuntu unavailable
    setFont(f);

    QFontMetrics fm(font());
    int rowHeight = fm.height();
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(rowHeight);

    // Note: Sorting is handled manually in loadDirectory, so disable view sorting
    setSortingEnabled(false);

    QHeaderView *header = horizontalHeader();
    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    header->setHighlightSections(false);

    // Allow user-resizing for all columns
    header->setSectionResizeMode(QHeaderView::Interactive);
    // Make the last visible column stretch to fill remaining space
    header->setStretchLastSection(true);

    // Optional: initial sizes
    setColumnWidth(COLUMN_NAME, 200);
    setColumnWidth(COLUMN_EXT, 80);
    setColumnWidth(COLUMN_SIZE, 100);
    setColumnWidth(COLUMN_DATE, 125);

    setIconSize(QSize(19, 19));

    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    header->setHighlightSections(false);

    // Show initial sort indicator
    header->setSortIndicator(sortColumn, sortOrder);

    connect(header, &QHeaderView::sectionClicked, this, &FilePanel::onHeaderSectionClicked);

    connect(this, &QTableView::doubleClicked, [this](const QModelIndex &index) { trigger(index); });

    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

FilePanel::~FilePanel() {
    delete dir;
    delete model;
}

QString FilePanel::normalizeForSearch(const QString &s) const {
    if (s.isEmpty())
        return {};

    QString tmp = s;

    // Treat names starting with dot as if the dot was not there (".git" -> "git")
    if (tmp.size() > 1 && tmp[0] == u'.')
        tmp.remove(0, 1);

    // QString (UTF-16) -> ICU UnicodeString (UTF-16)
    icu::UnicodeString ustr(reinterpret_cast<const UChar *>(tmp.utf16()), tmp.length());

    // Unicode case folding (full Unicode, not only ASCII)
    ustr.foldCase();

    // Create transliterator once (static) to strip diacritics:
    // "NFD; [:Nonspacing Mark:] Remove; NFC"
    static icu::Transliterator *accentStripper = nullptr;
    static bool translitInitTried = false;

    if (!translitInitTried) {
        translitInitTried = true;
        UErrorCode status = U_ZERO_ERROR;
        accentStripper =
                icu::Transliterator::createInstance("NFD; [:Nonspacing Mark:] Remove; NFC", UTRANS_FORWARD, status);
        if (U_FAILURE(status)) {
            accentStripper = nullptr; // fallback: no accent stripping
        }
    }

    if (accentStripper) {
        accentStripper->transliterate(ustr);
    }

    // ICU UnicodeString -> UTF-8 -> QString
    std::string utf8;
    ustr.toUTF8String(utf8);

    return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

void FilePanel::onHeaderSectionClicked(int logicalIndex) {
    if (sortColumn == logicalIndex) {
        // Toggle direction
        sortOrder = (sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        sortColumn = logicalIndex;
        // For Date and Size, default to Descending (newest/largest first)
        // For Name and Ext, default to Ascending (A-Z)
        if (logicalIndex == COLUMN_DATE || logicalIndex == COLUMN_SIZE) {
            sortOrder = Qt::DescendingOrder;
        } else {
            sortOrder = Qt::AscendingOrder;
        }
    }
    horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    addAllEntries();
}

void FilePanel::startDrag(Qt::DropActions supportedActions) {
    QModelIndexList selectedRows = selectionModel()->selectedRows(COLUMN_NAME);
    if (selectedRows.isEmpty())
        return;

    QList<QUrl> urls;
    urls.reserve(selectedRows.size());

    // Use first selected row to build drag pixmap
    QString firstName;
    {
        int row = selectedRows.first().row();
        firstName = getRowRelPath(row);
    }

    QDir currentDir(currentPath);
    for (const QModelIndex &idx: selectedRows) {
        int row = idx.row();
        QString name = getRowRelPath(row);
        if (name.isEmpty())
            continue; // skip parent entry "[..]" / empty name

        QString fullPath = currentDir.absoluteFilePath(name);
        urls.append(QUrl::fromLocalFile(fullPath));
    }

    if (urls.isEmpty())
        return;

    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urls); // text/uri-list

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    // --- Custom drag pixmap (big icon + filename) ---
    const int size = 96; // bigger icon
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Example: simple rounded rectangle as "badge"
    QRect rect(0, 0, size, size);
    QColor bg(30, 144, 255, 220); // semi-transparent blue
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect.adjusted(4, 4, -4, -4), 12, 12);

    // Draw file icon (you can use your own QIcon)
    QStyle *st = style();
    QIcon fileIcon = st->standardIcon(QStyle::SP_FileIcon);

    QPixmap iconPixmap = fileIcon.pixmap(48, 48);
    QPoint iconPos((size - iconPixmap.width()) / 2, (size - iconPixmap.height()) / 2 - 10);
    p.drawPixmap(iconPos, iconPixmap);

    // Draw file name (only base, trimmed)
    p.setPen(Qt::white);
    QFont f = font();
    f.setPointSize(f.pointSize() + 1);
    p.setFont(f);

    QString text = firstName;
    QFontMetrics fm(f);
    text = fm.elidedText(text, Qt::ElideRight, size - 10);

    p.drawText(QRect(5, size - fm.height() - 8, size - 10, fm.height() + 4), Qt::AlignCenter, text);

    p.end();

    drag->setPixmap(pixmap);
    // Hot spot near the middle, slightly above icon center
    drag->setHotSpot(QPoint(size / 2, size / 2));

    // --- Execute drag ---
    Qt::DropActions actions = supportedActions;
    if (!(actions & Qt::CopyAction))
        actions |= Qt::CopyAction;

    drag->exec(actions, Qt::CopyAction);
}

void FilePanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QTableView::dragEnterEvent(event);
    }
}

void FilePanel::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QTableView::dragMoveEvent(event);
    }
}

void FilePanel::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (!mimeData->hasUrls()) {
        QTableView::dropEvent(event);
        return;
    }

    QList<QUrl> urls = mimeData->urls();
    if (urls.isEmpty())
        return;

    // Use first URL - navigate to that path
    QString path = urls.first().toLocalFile();
    if (!path.isEmpty()) {
        navigateToPath(path);
        event->acceptProposedAction();
    }
}

QIcon FilePanel::getIconForEntry(const QFileInfo &info, EntryContentState contentState) {
    // --- folders ---
    if (contentState != EntryContentState::NotDirectory) {
        if (contentState == EntryContentState::DirEmpty) {
            static QIcon emptyIcon(":/icons/folder-empty.png");
            return emptyIcon;
        }
        static QIcon folderIcon(":/icons/folder.png");
        return folderIcon;
    }

    // --- files: use FileIconResolver ---
    return FileIconResolver::instance().getIconByName(info.fileName());
}

void FilePanel::updateSearch(const QString &text) {
    m_lastSearchText = text;

    if (text.isEmpty()) {
        m_lastSearchRow = -1;
        return;
    }

    const int rowCount = model->rowCount();
    if (rowCount == 0)
        return;

    // Start from current row if no previous match
    int startRow = m_lastSearchRow >= 0 ? m_lastSearchRow : currentIndex().row();

    int row = startRow;

    auto normalize = [&](const QString &s) {
        return normalizeForSearch(s); // Twoja istniejąca funkcja
    };

    const QString needle = normalize(text);

    do {
        row = (row + 1) % rowCount;

        QString name = getRowName(row);
        if (normalize(name).contains(needle)) {
            QModelIndex idx = model->index(row, COLUMN_NAME);
            setCurrentIndex(idx);
            scrollTo(idx);
            m_lastSearchRow = row;
            return;
        }

    } while (row != startRow);

    // No match found
}

void FilePanel::nextMatch() {
    if (m_lastSearchText.isEmpty())
        return;

    const int rowCount = model->rowCount();
    if (rowCount == 0)
        return;

    int row = (m_lastSearchRow >= 0) ? m_lastSearchRow : currentIndex().row();

    auto normalize = [&](const QString &s) { return normalizeForSearch(s); };

    QString needle = normalize(m_lastSearchText);

    int start = row;

    do {
        row = (row + 1) % rowCount;

        QString name = getRowName(row);
        if (normalize(name).contains(needle)) {
            QModelIndex idx = model->index(row, COLUMN_NAME);
            setCurrentIndex(idx);
            scrollTo(idx);
            m_lastSearchRow = row;
            return;
        }

    } while (row != start);
}

void FilePanel::prevMatch() {
    if (m_lastSearchText.isEmpty())
        return;

    const int rowCount = model->rowCount();
    if (rowCount == 0)
        return;

    int row = (m_lastSearchRow >= 0) ? m_lastSearchRow : currentIndex().row();

    auto normalize = [&](const QString &s) { return normalizeForSearch(s); };

    QString needle = normalize(m_lastSearchText);

    int start = row;

    do {
        row = (row - 1 + rowCount) % rowCount;

        QString name = getRowName(row);
        if (normalize(name).contains(needle)) {
            QModelIndex idx = model->index(row, COLUMN_NAME);
            setCurrentIndex(idx);
            scrollTo(idx);
            m_lastSearchRow = row;
            return;
        }

    } while (row != start);
}

void FilePanel::jumpWithControl(int direction) {
    // direction: +1 = Ctrl+Down, -1 = Ctrl+Up
    if (!model || model->rowCount() == 0)
        return;

    int rowCount = model->rowCount();

    // Find a range of directories and files
    int lastDirRow = -1;
    int firstFileRow = rowCount;

    for (int r = 0; r < rowCount; ++r) {
        QModelIndex idx = model->index(r, COLUMN_SIZE);
        QString size = model->data(idx, Qt::DisplayRole).toString();
        bool isDir = (size == "<DIR>");
        if (isDir)
            lastDirRow = r;
        else if (firstFileRow == rowCount)
            firstFileRow = r;
    }

    // current position
    int row = currentIndex().row();

    // ────────────────────────────────────────────────
    // CTRL + DOWN
    // ────────────────────────────────────────────────
    if (direction > 0) {

        // if we are in directories → go to the first file
        if (row <= lastDirRow) {
            if (firstFileRow < rowCount) {
                row = firstFileRow;
            } else {
                row = rowCount - 1; // no files
            }
        } else {
            // we are in the files → go to the bottom
            row = rowCount - 1;
        }
    }

    // ────────────────────────────────────────────────
    // CTRL + UP
    // ────────────────────────────────────────────────
    else {

        // if we are in files → go to the last directory
        if (row >= firstFileRow) {
            if (lastDirRow >= 0)
                row = lastDirRow;
            else
                row = 0; // no directories
        } else {
            // we are in the directories → go to the top
            row = 0;
        }
    }
    QModelIndex idx = model->index(row, COLUMN_NAME);

    QItemSelectionModel *sm = selectionModel();
    if (sm) {
        sm->clearSelection();
        sm->setCurrentIndex(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    } else {
        setCurrentIndex(idx);
    }

    scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

EntryContentState FilePanel::ensureContentState(PanelEntry &entry) const {
    if (entry.contentState != EntryContentState::DirUnknown)
        return entry.contentState;

    if (!entry.info.isDir()) {
        entry.contentState = EntryContentState::NotDirectory;
        return entry.contentState;
    }

    QDir dir(entry.info.absoluteFilePath());
    entry.contentState = dir.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)
                                 ? EntryContentState::DirEmpty
                                 : EntryContentState::DirNotEmpty;

    return entry.contentState;
}

void FilePanel::createNewDirectory(QWidget *dialogParent) {
    if (!dir)
        return;

    QModelIndex current_index = currentIndex();
    QString suggestedName;

    if (current_index.isValid()) {
        QString fullName = getRowName(current_index.row());
        if (!fullName.isEmpty())
            suggestedName = fullName; // poprawna nazwa pliku/katalogu
    }

    QWidget *parent = dialogParent ? dialogParent : this;

    bool ok = false;
    QString name = QInputDialog::getText(parent, tr("Create new directory"), tr("Input new name:"), QLineEdit::Normal,
                                         suggestedName, &ok);

    if (!ok || name.isEmpty())
        return;

    if (!dir->mkpath(name)) {
        QMessageBox::warning(parent, tr("Error"), tr("Failed to create directory."));
        return;
    }

    loadDirectory();
    auto firstPart = name.section('/', 0, 0);
    selectEntryByName(firstPart);
}

void FilePanel::renameOrMoveEntry(QWidget *dialogParent, const QString &defaultTargetDir) {
    if (!dir)
        return;

    QModelIndex current_index = currentIndex();
    if (!current_index.isValid())
        return;

    const QString fullName = getRowName(current_index.row());
    if (fullName.isEmpty()) {
        return;
    }

    QWidget *parent = dialogParent ? dialogParent : this;

    QString suggested = fullName;

    if (!defaultTargetDir.isEmpty()) {
        QDir dstDir(defaultTargetDir);
        suggested = dstDir.filePath(fullName);
    }

    bool ok = false;
    QString newName = QInputDialog::getText(parent, tr("Rename / move"), tr("New name or path:"), QLineEdit::Normal,
                                            suggested, &ok);

    if (!ok || newName.isEmpty() || newName == fullName)
        return;

    QDir currentDir(currentPath);
    const QString srcPath = currentDir.absoluteFilePath(fullName);

    QString dstPath;
    if (QDir::isAbsolutePath(newName)) {
        dstPath = newName;
    } else {
        dstPath = currentDir.absoluteFilePath(newName);
    }

    // Sprawdzenie czy źródło i cel są na tym samym urządzeniu
    QStorageInfo srcInfo(srcPath);
    QStorageInfo dstInfo(defaultTargetDir);

    if (!srcInfo.isValid() || !dstInfo.isValid()) {
        QMessageBox::warning(parent, tr("Error"), tr("Cannot determine storage devices for source or destination."));
        return;
    }

    if (srcInfo.device() != dstInfo.device()) {
        QMessageBox::warning(parent, tr("Error"),
                             tr("Source and destination are on different devices.\n"
                                "Move operation is not supported in this mode."));
        return;
    }

    // Próba rename/move w obrębie jednego filesystemu
    QFile file(srcPath);
    if (!file.rename(dstPath)) {
        QMessageBox::warning(parent, tr("Error"), tr("Failed to rename/move:\n%1\nto\n%2").arg(srcPath, dstPath));
        return;
    }

    // Odśwież panel
    loadDirectory();

    // Spróbuj zaznaczyć nową nazwę (tylko ostatni segment)
    const QString leafName = QFileInfo(dstPath).fileName();
    if (!leafName.isEmpty())
        selectEntryByName(leafName);
}

void FilePanel::updateRowMarking(int row, bool marked) {
    Q_UNUSED(marked);
    if (!model || row < 0 || row >= model->rowCount())
        return;
    // Model returns ForegroundRole based on entry.isMarked
    // Just signal that the row data changed
    model->refreshRow(row);
}

std::pair<PanelEntry *, int> FilePanel::currentEntryRow() {
    QModelIndex idx = currentIndex();
    if (!idx.isValid())
        return {nullptr, -1};

    const int row = idx.row();

    // In branch mode there's no [..] entry
    if (branchMode) {
        if (row < 0 || row >= entries.size())
            return {nullptr, row};
        return {&entries[row], row};
    }

    if (!dir)
        return {nullptr, row};

    const bool isRoot = dir->isRoot();
    const int offset = isRoot ? 0 : 1; // 0: no [..], 1:first row is [..]
    const int entryIndex = row - offset;

    if (entryIndex < 0 || entryIndex >= entries.size())
        return {nullptr, row}; // np. [..]

    return {&entries[entryIndex], row};
}

void FilePanel::toggleMarkOnCurrent(bool advanceRow) {
    auto p = currentEntryRow();
    if (!p.first)
        return;
    auto entry = p.first;
    entry->isMarked = !entry->isMarked;

    updateRowMarking(p.second, entry->isMarked);

    if (advanceRow) {
        int nextRow = p.second + 1;
        if (nextRow < model->rowCount()) {
            QModelIndex nextIdx = model->index(nextRow, COLUMN_NAME);
            setCurrentIndex(nextIdx);
            scrollTo(nextIdx);
        }
    }

    emit selectionChanged();
}

QStringList FilePanel::getMarkedNames() const {
    QStringList result;
    for (const auto &entry: entries) {
        if (entry.isMarked) {
            result << entry.info.fileName();
        }
    }
    return result;
}

QStringList FilePanel::getMarkedRelPaths() const {
    QStringList result;
    for (const auto &entry: entries) {
        if (entry.isMarked) {
            if (entry.branch.isEmpty())
                result << entry.info.fileName();
            else
                result << entry.branch + "/" + entry.info.fileName();
        }
    }
    return result;
}

bool FilePanel::hasMarkedEntries() const {
    for (const auto &entry: entries) {
        if (entry.isMarked)
            return true;
    }
    return false;
}

void FilePanel::rememberSelection() {
    QModelIndex idx = currentIndex();
    if (idx.isValid())
        m_lastSelectedRow = idx.row();
}

void FilePanel::restoreSelectionFromMemory() {
    if (!model)
        return;

    if (m_lastSelectedRow < 0 || m_lastSelectedRow >= model->rowCount())
        return;

    QModelIndex idx = model->index(m_lastSelectedRow, COLUMN_NAME);
    if (!idx.isValid())
        return;

    setCurrentIndex(idx);
    if (selectionModel()) {
        selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    scrollTo(idx);
}

void FilePanel::styleActive() {
    setStyleSheet("QTableView::item {"
                  "    background-color: white;"
                  "}"
                  "QTableView::item:selected {"
                  "    background-color: #3584E4;"
                  "}");
}

void FilePanel::styleInactive() {
    setStyleSheet("QTableView::item {"
                  "    background-color: white;"
                  "}"
                  "QTableView::item:selected {"
                  "    background-color: #a0a0a0;"
                  "}");
}

void FilePanel::collectCopyStats(const QString &srcPath, CopyStats &stats, bool &ok, bool *cancelFlag) {
    ok = true;

    QFileInfo rootInfo(srcPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        ok = false;
        return;
    }

    // liczymy katalog root też
    stats.totalDirs += 1;

    SortedDirIterator it(srcPath, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

    int counter = 0;
    while (it.hasNext()) {
        // Check for cancellation every 100 iterations
        if (cancelFlag && *cancelFlag) {
            ok = false;
            return;
        }

        // Process events every 100 iterations to allow UI to respond
        if (++counter % 100 == 0) {
            QCoreApplication::processEvents();
        }

        it.next();
        const QFileInfo fi = it.fileInfo();

        if (fi.isDir()) {
            stats.totalDirs += 1;
        } else if (fi.isFile()) {
            stats.totalFiles += 1;
            stats.totalBytes += static_cast<quint64>(fi.size());
        }
        // inne typy (symlinki itp.) na razie pomijamy
    }
}

bool FilePanel::copyDirectoryRecursive(const QString &srcRoot, const QString &dstRoot, const CopyStats &stats,
                                       QProgressDialog &progress, quint64 &bytesCopied, bool &userAbort) {
    if (userAbort)
        return false;

    QFileInfo srcInfo(srcRoot);
    if (!srcInfo.exists() || !srcInfo.isDir())
        return false;

    QDir dstDir;
    if (!dstDir.mkpath(dstRoot)) {
        QMessageBox::warning(nullptr, tr("Error"), tr("Failed to create directory:\n%1").arg(dstRoot));
        return false;
    }

    QDir dir(srcRoot);
    const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::NoSort);

    for (const QFileInfo &fi: entries) {
        if (userAbort)
            return false;

        // obsługa Cancel
        if (progress.wasCanceled()) {
            auto reply = QMessageBox::question(nullptr, tr("Cancel copy"),
                                               tr("Do you really want to cancel the copy operation?"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (reply == QMessageBox::Yes) {
                userAbort = true;
                return false;
            } else {
                // dalej kopiujemy; Cancel przestanie być używany
            }
        }

        const QString srcPath = fi.absoluteFilePath();
        const QString dstPath = QDir(dstRoot).filePath(fi.fileName());

        if (fi.isDir()) {
            if (!copyDirectoryRecursive(srcPath, dstPath, stats, progress, bytesCopied, userAbort))
                return false;
        } else if (fi.isFile()) {
            // jeśli istniał – nadpisujemy
            if (QFileInfo::exists(dstPath))
                QFile::remove(dstPath);

            if (!QFile::copy(srcPath, dstPath)) {
                QMessageBox::warning(nullptr, tr("Error"), tr("Failed to copy:\n%1\nto\n%2").arg(srcPath, dstPath));
                return false;
            }
            finalizeCopiedFile(srcPath, dstPath);

            bytesCopied += static_cast<quint64>(fi.size());

            if (stats.totalBytes > 0) {
                const int value = static_cast<int>(qMin<quint64>(bytesCopied, stats.totalBytes));
                progress.setValue(value);
            }

            qApp->processEvents();
        }
    }
    return true;
}

void FilePanel::feedSearchResults(const QVector<SearchResult> &results, const QString &searchPath) {
    entries.clear();
    QString basePath = searchPath;
    if (!basePath.endsWith('/'))
        basePath += '/';

    for (const SearchResult &r: results) {
        QString fullPath = r.dir + "/" + r.name;
        QFileInfo info(fullPath);
        // Branch is relative to search path
        QString branch;
        if (r.dir.startsWith(basePath))
            branch = r.dir.mid(basePath.length());
        else
            branch = r.dir; // fallback to absolute if not under search path
        entries.append(PanelEntry(info, branch));
    }

    branchMode = true;
    sortEntries();
    model->refresh();
    selectFirstEntry();
    emit selectionChanged();
}

// ============================================================================
// Branch mode incremental updates (avoid full reload)
// ============================================================================

bool FilePanel::removeEntryByRelPath(const QString &relPath) {
    // Find entry by relative path
    for (int i = 0; i < entries.size(); ++i) {
        QString entryRelPath;
        if (entries[i].branch.isEmpty()) {
            entryRelPath = entries[i].info.fileName();
        } else {
            entryRelPath = entries[i].branch + "/" + entries[i].info.fileName();
        }

        if (entryRelPath == relPath) {
            // Found - remove from entries
            entries.removeAt(i);
            // Refresh model to reflect the change
            model->refresh();
            return true;
        }
    }
    return false;
}

bool FilePanel::renameEntry(const QString &oldRelPath, const QString &newRelPath) {
    // Find entry by old relative path
    for (int i = 0; i < entries.size(); ++i) {
        QString entryRelPath;
        if (entries[i].branch.isEmpty()) {
            entryRelPath = entries[i].info.fileName();
        } else {
            entryRelPath = entries[i].branch + "/" + entries[i].info.fileName();
        }

        if (entryRelPath == oldRelPath) {
            // Found - update entry
            QDir baseDir(currentPath);
            QString newFullPath = baseDir.absoluteFilePath(newRelPath);
            QFileInfo newInfo(newFullPath);

            // Update entry
            entries[i].info = newInfo;

            // Update branch if path changed
            int lastSlash = newRelPath.lastIndexOf('/');
            if (lastSlash >= 0) {
                entries[i].branch = newRelPath.left(lastSlash);
            } else {
                entries[i].branch.clear();
            }

            // Refresh model row
            int modelRow = model->entryIndexToRow(i);
            model->refreshRow(modelRow);
            return true;
        }
    }
    return false;
}

bool FilePanel::updateEntryBranch(const QString &relPath, const QString &newBranch) {
    // Find entry by relative path
    for (int i = 0; i < entries.size(); ++i) {
        QString entryRelPath;
        if (entries[i].branch.isEmpty()) {
            entryRelPath = entries[i].info.fileName();
        } else {
            entryRelPath = entries[i].branch + "/" + entries[i].info.fileName();
        }

        if (entryRelPath == relPath) {
            // Found - update branch
            entries[i].branch = newBranch;

            // Update info to point to new location
            QDir baseDir(currentPath);
            QString newRelPathFull =
                    newBranch.isEmpty() ? entries[i].info.fileName() : newBranch + "/" + entries[i].info.fileName();
            entries[i].info = QFileInfo(baseDir.absoluteFilePath(newRelPathFull));

            // Refresh model row
            int modelRow = model->entryIndexToRow(i);
            model->refreshRow(modelRow);
            return true;
        }
    }
    return false;
}

bool FilePanel::addEntryFromPath(const QString &fullPath, const QString &branch) {
    QFileInfo info(fullPath);
    if (!info.exists()) {
        return false;
    }

    // Create new entry and add to entries list
    PanelEntry entry(info, branch);
    entries.append(entry);

    // Refresh model to show the new entry
    model->refresh();
    return true;
}

// ============================================================================
// Visible files tracking for file watcher
// ============================================================================

void FilePanel::scheduleVisibleFilesUpdate() {
    // Restart debounce timer - will emit after 1000ms of no changes
    m_visibilityDebounceTimer->start();
}

void FilePanel::scrollContentsBy(int dx, int dy) {
    QTableView::scrollContentsBy(dx, dy);
    // Schedule update when scrolling changes visible files
    scheduleVisibleFilesUpdate();
}

QStringList FilePanel::getVisibleFilePaths() const {
    QStringList paths;

    if (!model || !viewport())
        return paths;

    const int rowCount = model->rowCount();
    if (rowCount == 0)
        return paths;

    // Get visible rect
    QRect visibleRect = viewport()->rect();

    for (int row = 0; row < rowCount; ++row) {
        QModelIndex idx = model->index(row, COLUMN_NAME);
        QRect rowRect = visualRect(idx);

        // Check if row is visible
        if (!rowRect.isValid() || !visibleRect.intersects(rowRect))
            continue;

        // Get entry index from row
        int entryIdx = model->rowToEntryIndex(row);
        if (entryIdx < 0 || entryIdx >= entries.size())
            continue; // Skip [..] row

        const PanelEntry &entry = entries[entryIdx];

        // Only track files, not directories
        if (!entry.info.isDir()) {
            paths.append(entry.info.absoluteFilePath());
        }
    }

    return paths;
}

void FilePanel::emitVisibleFiles() {
    QStringList paths = getVisibleFilePaths();
    emit visibleFilesChanged(m_side, paths);
}

bool FilePanel::refreshEntryByPath(const QString &filePath) {
    // Find entry by absolute path
    for (int i = 0; i < entries.size(); ++i) {
        if (entries[i].info.absoluteFilePath() == filePath) {
            // Refresh file info
            entries[i].info.refresh();

            // Refresh model row
            int modelRow = model->entryIndexToRow(i);
            model->refreshRow(modelRow);
            return true;
        }
    }
    return false;
}

#include "FilePanel_impl.inc"
