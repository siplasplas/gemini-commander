#include <unicode/unistr.h>
#include <unicode/translit.h>
#include <unicode/utypes.h>
#include <algorithm>

#include "FilePanel.h"
#include <QDir>
#include <QDrag>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QStandardItemModel>
#include <QUrl>
#include <QDesktopServices>
#include <QUrl>
#include <QDirIterator>
#include <QMessageBox>
#include <QInputDialog>
#include <QStorageInfo>

#include "SizeFormat.h"

QString stripLeadingDot(const QString& s)
{
    if (!s.isEmpty() && s.startsWith('.'))
        return s.mid(1);
    return s;
}

void FilePanel::sortEntries() {
  // --------------------------
  // Sorting (TC-like)
  // --------------------------
  std::sort(entries.begin(), entries.end(),
            [this](const PanelEntry &c, const PanelEntry &d) {
              auto a = c.info;
              auto b = d.info;
              const bool aDir = a.isDir();
              const bool bDir = b.isDir();

              // Directories always on top
              if (aDir != bDir)
                return aDir && !bDir;

              auto lessCI = [](const QString &x, const QString &y) {
                return x.compare(y, Qt::CaseInsensitive) < 0;
              };
              auto greaterCI = [](const QString &x, const QString &y) {
                return x.compare(y, Qt::CaseInsensitive) > 0;
              };

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
                  const QString ea = a.suffix();
                  const QString eb = b.suffix();
                  int c = ea.compare(eb, Qt::CaseInsensitive);
                  if (c != 0)
                    return asc ? (c < 0) : (c > 0);
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
                                  return asc ? (c.totalSizeBytes < d.totalSizeBytes)
                                             : (c.totalSizeBytes > d.totalSizeBytes);
                              return cmpNames(asc);
                          }

                          // 3) both have no calculated size → sort only by name
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
void FilePanel::addFirstEntry(bool isRoot) {
  // --------------------------
  // Add ".." entry
  // --------------------------
  if (!isRoot) {
    QList<QStandardItem *> row;

    // COLUMN_ID
    row.append(new QStandardItem(""));

    // COLUMN_NAME (base)
    auto *upItem = new QStandardItem("[..]");
    upItem->setData(QString(""), Qt::UserRole); // full_name = "" for [..]
    QIcon upIcon = style()->standardIcon(QStyle::SP_FileDialogToParent);
    upItem->setIcon(upIcon);
    row.append(upItem);

    // COLUMN_TYPE (empty)
    row.append(new QStandardItem(""));

    // COLUMN_SIZE
    row.append(new QStandardItem("<DIR>"));

    // COLUMN_DATE
    QFileInfo info(".");
    row.append(
        new QStandardItem(info.lastModified().toString("yyyy-MM-dd hh:mm")));

    model->appendRow(row);
  }
}

static QStringList getTextColumn(PanelEntry& entry)
{
    QStringList colStrings;
    colStrings.append(""); // ID column

    QString base, ext;
    auto& info = entry.info;

    if (info.isDir()) {
        base = info.fileName();
    } else {
        base = info.completeBaseName();
        ext  = info.suffix();

        // hidden: ".git" type case
        if (base.isEmpty()) {
            base = "." + ext;
            ext.clear();
        }
    }

    QString fullName = base;
    if (!ext.isEmpty())
        fullName += "." + ext;

    colStrings.append(fullName);               // COLUMN_NAME
    colStrings.append(ext);                    // COLUMN_EXT

    QString sizeStr;
    if (!info.isDir()) {
        sizeStr = QString::fromStdString(SizeFormat::formatSize(info.size(), false));
    } else if (entry.hasTotalSize == TotalSizeStatus::Has) {
        sizeStr = QString::fromStdString(SizeFormat::formatSize(entry.totalSizeBytes, false));
    } else if (entry.hasTotalSize == TotalSizeStatus::InPogress) {
        sizeStr = "....";
    } else {
        sizeStr = "<DIR>";
    }
    colStrings.append(sizeStr);                // COLUMN_SIZE

    colStrings.append(info.lastModified().toString("yyyy-MM-dd hh:mm")); // COLUMN_DATE

    return colStrings;
}

QList<QStandardItem*> FilePanel::entryToRow(PanelEntry& entry)
{
    const auto list = getTextColumn(entry);
    QList<QStandardItem*> row;

    for (int col = COLUMN_ID; col <= COLUMN_DATE; ++col)
        row.append(new QStandardItem(list[col]));

    // fullName dla UserRole
    QString fullName;
    if (list[COLUMN_EXT].isEmpty())
        fullName = list[COLUMN_NAME];
    else
        fullName = list[COLUMN_NAME] + "." + list[COLUMN_EXT];

    row[COLUMN_NAME]->setData(fullName, Qt::UserRole);

    EntryContentState state = ensureContentState(entry);
    row[COLUMN_NAME]->setIcon(iconForExtension(list[COLUMN_EXT], state));

    return row;
}

void FilePanel::updateColumn(int row, PanelEntry& entry)
{
    const auto list = getTextColumn(entry);

    for (int col = COLUMN_NAME; col <= COLUMN_DATE; ++col) {
        if (QStandardItem* item = model->item(row, col)) {
            item->setText(list[col]);
        }
    }

    // ikona (na wypadek gdyby state się zmienił)
    EntryContentState state = ensureContentState(entry);
    if (QStandardItem* nameItem = model->item(row, COLUMN_NAME)) {
        nameItem->setIcon(iconForExtension(list[COLUMN_EXT], state));
    }
    if (viewport()) {
        QModelIndex idx = model->index(row, COLUMN_NAME);
        QRect r = visualRect(idx);
        viewport()->repaint(r);
    }
}

void FilePanel::addEntries()
{
    for (PanelEntry& entry : entries) {
        const int rowIndex = model->rowCount();
        model->appendRow(entryToRow(entry));
        if (entry.isMarked)
            updateRowMarking(rowIndex, true);
    }
}

void FilePanel::addAllEntries() {
    QString selectedName;
    auto rows = selectionModel()->selectedRows();
    if (!rows.isEmpty())
        selectedName = getRowName(rows.first().row());
    sortEntries();
    model->removeRows(0, model->rowCount());
    addFirstEntry(dir->isRoot());
    addEntries();
    setRootIndex(QModelIndex());
    selectEntryByName(selectedName);
}

void FilePanel::triggerCurrentEntry() {
    auto rows = selectionModel()->selectedRows();
    onPanelActivated(rows.first());
}

void FilePanel::loadDirectory()
{
    dir = new QDir(currentPath);
    if (!dir->exists()) {
        return;
    }
    entries.clear();
    QDirIterator it(currentPath, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();
        PanelEntry entry(info);
        entries.append(entry);
    }
    addAllEntries();
    emit directoryChanged(currentPath);
}

QString FilePanel::getRowName(int row) const {
    if (row < 0 || row >= model->rowCount())
        return {};
    return model->item(row, COLUMN_NAME)->data(Qt::UserRole).toString();
}

void FilePanel::selectEntryByName(const QString& fullName)
{
    // For "[..]" / going up we expect empty fullName
    if (fullName.isEmpty()) {
        // row 0 (parent entry), unless we are at root
        if (currentPath != "/" && model->rowCount() > 0) {
            QModelIndex idx = model->index(0, COLUMN_NAME);
            setCurrentIndex(idx);
            scrollTo(idx);
            setFocus();
        }
    }

    QModelIndex start = model->index(0, COLUMN_NAME);
    QModelIndexList matches = model->match(
        start,
        Qt::UserRole,             // search fullName stored in UserRole
        fullName,
        1,                        // first match only
        Qt::MatchExactly
    );

    if (matches.isEmpty())
        return;

    QModelIndex selectIndex = matches.first();
    setCurrentIndex(selectIndex);
    scrollTo(selectIndex);
    setFocus();
}

void FilePanel::onPanelActivated(const QModelIndex &index) {
    if (!index.isValid()) return;

    QString name = getRowName(index.row());
    QDir dir(currentPath);
    QString selectedName;

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
            const QString absPath = info.absoluteFilePath();
            QDesktopServices::openUrl(QUrl::fromLocalFile(absPath));
            return; // do not reload directory
        }
    }
    loadDirectory();
    selectEntryByName(selectedName);
}

FilePanel::FilePanel(QWidget* parent)
    : QTableView(parent) {
    model = new QStandardItemModel(nullptr);
    QStringList headers = {"id","Name", "Ext", "Size", "Date"};
    model->setHorizontalHeaderLabels(headers);
    currentPath = QDir::homePath();
    setModel(model);

    hideColumn(COLUMN_ID);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    verticalHeader()->hide();
    
    QFontMetrics fm(font());
    int rowHeight = fm.height();
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(rowHeight);

    // Note: Sorting is handled manually in loadDirectory, so disable view sorting
    setSortingEnabled(false);

    QHeaderView* header = horizontalHeader();
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
    setColumnWidth(COLUMN_DATE, 150);


    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    header->setHighlightSections(false);


    connect(header, &QHeaderView::sectionClicked,
            this, &FilePanel::onHeaderSectionClicked);

    connect(this, &QTableView::activated, [this](const QModelIndex &index) {
        onPanelActivated(index);
    });

    initSearchEdit();
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
}

FilePanel::~FilePanel() {
    delete dir;
    delete model;
}

void FilePanel::initSearchEdit()
{
    searchEdit = new QLineEdit(this);
    searchEdit->hide();
    searchEdit->setPlaceholderText("Search...");
    searchEdit->setClearButtonEnabled(true);

    // Make it visually distinct and compact
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() - 1);
    searchEdit->setFont(f);

    searchEdit->setFrame(true);

    // We will manually position it in resizeEvent()
    searchEdit->installEventFilter(this);

    connect(searchEdit, &QLineEdit::textChanged,
            this, &FilePanel::onSearchTextChanged);
}

QString FilePanel::normalizeForSearch(const QString& s) const
{
    if (s.isEmpty())
        return {};

    QString tmp = s;

    // Treat names starting with dot as if the dot was not there (".git" -> "git")
    if (tmp.size() > 1 && tmp[0] == u'.')
        tmp.remove(0, 1);

    // QString (UTF-16) -> ICU UnicodeString (UTF-16)
    icu::UnicodeString ustr(
        reinterpret_cast<const UChar*>(tmp.utf16()),
        tmp.length()
    );

    // Unicode case folding (full Unicode, not only ASCII)
    ustr.foldCase();

    // Create transliterator once (static) to strip diacritics:
    // "NFD; [:Nonspacing Mark:] Remove; NFC"
    static icu::Transliterator* accentStripper = nullptr;
    static bool translitInitTried = false;

    if (!translitInitTried) {
        translitInitTried = true;
        UErrorCode status = U_ZERO_ERROR;
        accentStripper = icu::Transliterator::createInstance(
            "NFD; [:Nonspacing Mark:] Remove; NFC",
            UTRANS_FORWARD,
            status
        );
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

bool FilePanel::findAndSelectPattern(const QString& pattern,
                                     bool forward,
                                     bool wrap,
                                     int startRow)
{
    if (!model || model->rowCount() == 0)
        return false;

    const QString normPattern = normalizeForSearch(pattern);
    if (normPattern.isEmpty())
        return false;

    const int rows = model->rowCount();
    if (rows <= 0)
        return false;

    // Determine search direction
    int step = forward ? 1 : -1;

    auto nextRow = [rows, wrap](int row, int step) -> int {
        int nr = row + step;
        if (wrap) {
            if (nr < 0)
                nr = rows - 1;
            else if (nr >= rows)
                nr = 0;
        }
        return nr;
    };

    int row = startRow;
    // Start from "next" row, not the current one
    row = nextRow(row, step);

    for (int i = 0; i < rows; ++i) {
        if (row < 0 || row >= rows)
            break;

        QString fullName = getRowName(row);
        if (!fullName.isEmpty()) {
            QString normName = normalizeForSearch(fullName);
            if (normName.startsWith(normPattern)) {
                QModelIndex idx = model->index(row, COLUMN_NAME);
                setCurrentIndex(idx);
                scrollTo(idx);
                return true;
            }
        }

        row = nextRow(row, step);
    }

    return false;
}

void FilePanel::active(bool active) {
    if (active)
        styleActive();
    else
        styleInactive();
}


void FilePanel::onSearchTextChanged(const QString& text)
{
    // Remember current text as candidate
    QString newText = text;

    if (newText.isEmpty()) {
        lastSearchText.clear();
        return;
    }

    int startRow = currentIndex().isValid() ? currentIndex().row() : -1;

    // Try to find match from current position
    if (findAndSelectPattern(newText, /*forward*/true, /*wrap*/true, startRow)) {
        lastSearchText = newText;
    } else {
        // No match: revert to previous text and restore cursor position
        // (blocks further typing that would lead to "no matches")
        searchEdit->blockSignals(true);
        searchEdit->setText(lastSearchText);
        searchEdit->blockSignals(false);

        // Optional: beep to indicate failure
        // QApplication::beep();
    }
}

void FilePanel::keyPressEvent(QKeyEvent* event) {
    // Ctrl+S: explicit search start, empty pattern
    if (event->key() == Qt::Key_S &&
    (event->modifiers() & Qt::ControlModifier)) {
        emit searchRequested(QString{});  // puste – użytkownik zaczyna pisać
        event->accept();
        return;
    }

    // Printable char without modifiers: start / continue search
    if (event->modifiers() == Qt::NoModifier &&
        !event->text().isEmpty()) {
        const QChar ch = event->text().at(0);
        if (!ch.isSpace()) {
            emit searchRequested(QString(ch));
            event->accept();
            return;
            }
        }

    // Default behavior for other keys (navigation, F-keys, etc.)
    QTableView::keyPressEvent(event);
}



bool FilePanel::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == searchEdit && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);

        if (ke->key() == Qt::Key_Escape) {
            // ESC: close search bar
            searchEdit->hide();
            searchEdit->clear();
            lastSearchText.clear();
            setFocus();
            return true;
        }

        if (ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Up) {
            if (!lastSearchText.isEmpty()) {
                int startRow = currentIndex().isValid()
                               ? currentIndex().row()
                               : -1;
                bool forward = (ke->key() == Qt::Key_Down);
                findAndSelectPattern(lastSearchText,
                                     forward,
                                     /*wrap*/true,
                                     startRow);
            }
            return true; // do not move cursor in QLineEdit
        }
    }

    return QTableView::eventFilter(obj, ev);
}

/*
void FilePanel::styleActive() {
    setStyleSheet(
        "QTableView:item {"
        "background-color: white;"
        "color: black;"
        "}"
        "QTableView:item:selected {"
        "    background-color: blue;"
        "    color: white;"
        "}");
}

void FilePanel::styleInactive() {
    setStyleSheet(
        "QTableView:item {"
        "background-color: white;"
        "color: black;"
        "}"
        "QTableView:item:selected {"
        "    background-color: lightgray;"
        "    color: white;"
        "}");
}

*/
void FilePanel::styleActive() {
    setStyleSheet(
        "QTableView::item {"
        "    background-color: white;"
        "}"
        "QTableView::item:selected {"
        "    background-color: blue;"
        "}"
    );
}

void FilePanel::styleInactive() {
    setStyleSheet(
        "QTableView::item {"
        "    background-color: white;"
        "}"
    );
}

void FilePanel::onHeaderSectionClicked(int logicalIndex)
{
    if (sortColumn == logicalIndex) {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                    ? Qt::DescendingOrder
                    : Qt::AscendingOrder;
    } else {
        sortColumn = logicalIndex;
        sortOrder = Qt::AscendingOrder;
    }
    horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    loadDirectory();
}

void FilePanel::startDrag(Qt::DropActions supportedActions)
{
    QModelIndexList selectedRows = selectionModel()->selectedRows(COLUMN_NAME);
    if (selectedRows.isEmpty())
        return;

    QList<QUrl> urls;
    urls.reserve(selectedRows.size());

    // Use first selected row to build drag pixmap
    QString firstName;
    {
        int row = selectedRows.first().row();
        firstName = getRowName(row);
    }

    for (const QModelIndex& idx : selectedRows) {
        int row = idx.row();
        QString name = getRowName(row);
        if (name.isEmpty())
            continue; // skip parent entry "[..]" / empty name

        dir = new QDir(currentPath);
        QString fullPath = dir->absoluteFilePath(name);
        urls.append(QUrl::fromLocalFile(fullPath));
    }

    if (urls.isEmpty())
        return;

    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(urls); // text/uri-list

    QDrag* drag = new QDrag(this);
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
    QStyle* st = style();
    QIcon fileIcon = st->standardIcon(QStyle::SP_FileIcon);

    QPixmap iconPixmap = fileIcon.pixmap(48, 48);
    QPoint iconPos((size - iconPixmap.width()) / 2,
                   (size - iconPixmap.height()) / 2 - 10);
    p.drawPixmap(iconPos, iconPixmap);

    // Draw file name (only base, trimmed)
    p.setPen(Qt::white);
    QFont f = font();
    f.setPointSize(f.pointSize() + 1);
    p.setFont(f);

    QString text = firstName;
    QFontMetrics fm(f);
    text = fm.elidedText(text, Qt::ElideRight, size - 10);

    p.drawText(QRect(5, size - fm.height() - 8, size - 10, fm.height() + 4),
               Qt::AlignCenter, text);

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

QIcon FilePanel::iconForEntry(const QFileInfo& info)
{
    static QFileIconProvider provider;

    // Katalog – klasyczna ikonka folderu
    if (info.isDir()) {
        return provider.icon(QFileIconProvider::Folder);
    }

    // Plik – spróbuj po MIME (na podstawie rozszerzenia, bez czytania zawartości)
    QMimeDatabase db;
    QMimeType mt = db.mimeTypeForFile(info, QMimeDatabase::MatchExtension);
    if (mt.isValid()) {
        QIcon ico = QIcon::fromTheme(mt.iconName());
        if (!ico.isNull())
            return ico;
    }

    // Fallback: domyślna ikonka pliku z systemu / stylu
    return provider.icon(QFileIconProvider::File);
}

QIcon FilePanel::iconForExtension(const QString& ext, EntryContentState contentState)
{
    static QFileIconProvider provider;
    static QMimeDatabase db;
    static QHash<QString, QIcon> cache;   // pliki
    static QHash<int, QIcon> folderCache; // katalogi (EntryContentState jako int)

    // --- katalogi ---
    if (contentState != EntryContentState::NotDirectory) {
        int key = static_cast<int>(contentState);
        auto it = folderCache.find(key);
        if (it != folderCache.end())
            return it.value();

        QString iconPath;
        if (contentState == EntryContentState::DirEmpty)
            iconPath = ":/icons/folder_blue.svg";
        else if (contentState == EntryContentState::DirNotEmpty)
            iconPath = ":/icons/folder_yellow.svg";
        else
            iconPath = ":/icons/folder_white.svg";

        QIcon icon(iconPath);
        folderCache.insert(key, icon);
        return icon;
    }

    // --- pliki: cache po rozszerzeniu ---
    auto it = cache.find(ext);
    if (it != cache.end())
        return it.value();

    QIcon icon;

    if (!ext.isEmpty()) {
        QMimeType mt = db.mimeTypeForFile("." + ext, QMimeDatabase::MatchExtension);
        if (mt.isValid()) {
            // próba ikony z motywu
            icon = QIcon::fromTheme(mt.iconName());

            // ewentualnie możesz użyć mt.genericIconName() jako fallback motywu:
            if (icon.isNull() && !mt.genericIconName().isEmpty())
                icon = QIcon::fromTheme(mt.genericIconName());
        }
    }

    // jeśli nie udało się wyciągnąć nic z motywu, bierz domyślną
    if (icon.isNull())
        icon = provider.icon(QFileIconProvider::File);

    cache.insert(ext, icon);
    return icon;
}

void FilePanel::updateSearch(const QString& text)
{
    m_lastSearchText = text;

    if (text.isEmpty()) {
        m_lastSearchRow = -1;
        return;
    }

    const int rowCount = model->rowCount();
    if (rowCount == 0)
        return;

    // Start from current row if no previous match
    int startRow = m_lastSearchRow >= 0
                   ? m_lastSearchRow
                   : currentIndex().row();

    int row = startRow;

    auto normalize = [&](const QString& s) {
        return normalizeForSearch(s);  // Twoja istniejąca funkcja
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

void FilePanel::nextMatch()
{
    if (m_lastSearchText.isEmpty())
        return;

    const int rowCount = model->rowCount();
    if (rowCount == 0)
        return;

    int row = (m_lastSearchRow >= 0)
              ? m_lastSearchRow
              : currentIndex().row();

    auto normalize = [&](const QString& s) {
        return normalizeForSearch(s);
    };

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

void FilePanel::prevMatch()
{
    if (m_lastSearchText.isEmpty())
        return;

    const int rowCount = model->rowCount();
    if (rowCount == 0)
        return;

    int row = (m_lastSearchRow >= 0)
              ? m_lastSearchRow
              : currentIndex().row();

    auto normalize = [&](const QString& s) {
        return normalizeForSearch(s);
    };

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

void FilePanel::jumpWithControl(int direction)
{
    // direction: +1 = Ctrl+Down, -1 = Ctrl+Up
    if (!model || model->rowCount() == 0)
        return;

    int rowCount = model->rowCount();

    // Find a range of directories and files
    int lastDirRow = -1;
    int firstFileRow = rowCount;

    for (int r = 0; r < rowCount; ++r) {
        QString size = model->item(r, COLUMN_SIZE)->text();
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
        }
        else {
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
        }
        else {
            // we are in the directories → go to the top
            row = 0;
        }
    }
    QModelIndex idx = model->index(row, COLUMN_NAME);

    QItemSelectionModel* sm = selectionModel();
    if (sm) {
        sm->clearSelection();
        sm->setCurrentIndex(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    } else {
        setCurrentIndex(idx);
    }

    scrollTo(idx, QAbstractItemView::PositionAtCenter);

}

EntryContentState FilePanel::ensureContentState(PanelEntry& entry) const
{
    if (entry.contentState != EntryContentState::DirUnknown)
        return entry.contentState;

    if (!entry.info.isDir()) {
        entry.contentState = EntryContentState::NotDirectory;
        return entry.contentState;
    }

    QDir dir(entry.info.absoluteFilePath());
    entry.contentState = dir.isEmpty()
        ? EntryContentState::DirEmpty
        : EntryContentState::DirNotEmpty;

    return entry.contentState;
}

void FilePanel::createNewDirectory(QWidget* dialogParent)
{
    if (!dir)
        return;

    QModelIndex current_index = currentIndex();
    QString suggestedName;

    if (current_index.isValid()) {
        QStandardItem* item = model->item(current_index.row(), COLUMN_NAME);
        QString fullName = item->data(Qt::UserRole).toString();
        if (!fullName.isEmpty())
            suggestedName = fullName;      // poprawna nazwa pliku/katalogu
    }

    QWidget* parent = dialogParent ? dialogParent : this;

    bool ok = false;
    QString name = QInputDialog::getText(
        parent,
        tr("Create new directory"),
        tr("Input new name:"),
        QLineEdit::Normal,
        suggestedName,
        &ok
    );

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

void FilePanel::renameOrMoveEntry(QWidget* dialogParent, const QString& defaultTargetDir)
{
    if (!dir)
        return;

    QModelIndex current_index = currentIndex();
    if (!current_index.isValid())
        return;

    QStandardItem* item = model->item(current_index.row(), COLUMN_NAME);
    if (!item)
        return;

    const QString fullName = item->data(Qt::UserRole).toString();
    if (fullName.isEmpty()) {
        return;
    }

    QWidget* parent = dialogParent ? dialogParent : this;

    QString suggested = fullName;

    if (!defaultTargetDir.isEmpty()) {
        QDir dstDir(defaultTargetDir);
        suggested = dstDir.filePath(fullName);
    }

    bool ok = false;
    QString newName = QInputDialog::getText(
        parent,
        tr("Rename / move"),
        tr("New name or path:"),
        QLineEdit::Normal,
        suggested,
        &ok
    );

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

    qDebug() << srcInfo;
    qDebug() << dstInfo;
    if (!srcInfo.isValid() || !dstInfo.isValid()) {
        QMessageBox::warning(
            parent,
            tr("Error"),
            tr("Cannot determine storage devices for source or destination.")
        );
        return;
    }

    if (srcInfo.device() != dstInfo.device()) {
        QMessageBox::warning(
            parent,
            tr("Error"),
            tr("Source and destination are on different devices.\n"
               "Move operation is not supported in this mode.")
        );
        return;
    }

    // Próba rename/move w obrębie jednego filesystemu
    QFile file(srcPath);
    if (!file.rename(dstPath)) {
        QMessageBox::warning(
            parent,
            tr("Error"),
            tr("Failed to rename/move:\n%1\nto\n%2").arg(srcPath, dstPath)
        );
        return;
    }

    // Odśwież panel
    loadDirectory();

    // Spróbuj zaznaczyć nową nazwę (tylko ostatni segment)
    const QString leafName = QFileInfo(dstPath).fileName();
    if (!leafName.isEmpty())
        selectEntryByName(leafName);
}

void FilePanel::updateRowMarking(int row, bool marked)
{
    if (!model || row < 0 || row >= model->rowCount())
        return;

    const int cols = model->columnCount();
    for (int col = 0; col < cols; ++col) {
        QStandardItem* item = model->item(row, col);
        if (!item)
            continue;

        if (marked) {
            item->setForeground(Qt::red);
        } else {
            item->setForeground(QBrush()); // reset do domyślnego koloru
        }
    }
}

std::pair<PanelEntry*, int> FilePanel::currentEntryRow() {
    QModelIndex idx = currentIndex();
    if (!idx.isValid())
        return {nullptr, -1};

    const int row = idx.row();

    if (!dir)
        return {nullptr, row};

    const bool isRoot = dir->isRoot();
    const int offset = isRoot ? 0 : 1; // 0: no [..], 1:first row is [..]
    const int entryIndex = row - offset;

    if (entryIndex < 0 || entryIndex >= entries.size())
        return {nullptr, row}; // np. [..]

    return {&entries[entryIndex], row};
}

void FilePanel::toggleMarkOnCurrent(bool advanceRow)
{
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
}

void FilePanel::rememberSelectionAndClear()
{
    QModelIndex idx = currentIndex();
    if (idx.isValid())
        m_lastSelectedRow = idx.row();
    else
        m_lastSelectedRow = -1;
    clearSelection();
    setCurrentIndex(QModelIndex());
}

void FilePanel::restoreSelectionFromMemory()
{
    if (!model)
        return;

    if (m_lastSelectedRow < 0 || m_lastSelectedRow >= model->rowCount())
        return;

    QModelIndex idx = model->index(m_lastSelectedRow, COLUMN_NAME);
    if (!idx.isValid())
        return;

    setCurrentIndex(idx);
    if (selectionModel()) {
        selectionModel()->select(
            idx,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
        );
    }
    scrollTo(idx);
}
