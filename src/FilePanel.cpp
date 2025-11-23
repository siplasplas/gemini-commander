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

void FilePanel::sortEntries() {
  // --------------------------
  // Sorting (TC-like)
  // --------------------------
  std::sort(entries.begin(), entries.end(),
            [this](const QFileInfo &a, const QFileInfo &b) {
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
                const QString na = a.fileName();
                const QString nb = b.fileName();
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
                  return cmpNames(asc);
                } else if (!aDir && !bDir) {
                  if (a.size() != b.size())
                    return asc ? (a.size() < b.size()) : (a.size() > b.size());
                  return cmpNames(asc);
                } else {
                  return cmpNames(asc);
                }

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
    auto *nameItem = new QStandardItem("[..]");
    nameItem->setData(QString(""), Qt::UserRole); // full_name = "" for [..]
    row.append(nameItem);

    // COLUMN_EXT (empty)
    row.append(new QStandardItem("<DIR>"));

    // COLUMN_SIZE
    row.append(new QStandardItem(""));

    // COLUMN_DATE
    QFileInfo info(".");
    row.append(
        new QStandardItem(info.lastModified().toString("yyyy-MM-dd hh:mm")));

    model->appendRow(row);
  }
}

void FilePanel::addEntries() {
  // --------------------------
  // Add files and directories
  // --------------------------
  for (const QFileInfo &info : entries) {

    const QString base = info.completeBaseName();
    const QString ext = info.isDir() ? QString() : info.suffix();
    QString fullName = base;
    if (!ext.isEmpty())
      fullName += "." + ext;

    QList<QStandardItem *> row;

    // COLUMN_ID
    row.append(new QStandardItem("id"));

    // COLUMN_NAME
    auto *nameItem = new QStandardItem(base);
    nameItem->setData(fullName, Qt::UserRole);
    row.append(nameItem);

    // COLUMN_EXT
    if (info.isDir()) {
      row.append(new QStandardItem(""));
    } else {
      row.append(new QStandardItem(ext));
    }

    // COLUMN_SIZE
    row.append(new QStandardItem(QString::number(info.size())));

    // COLUMN_DATE
    row.append(
        new QStandardItem(info.lastModified().toString("yyyy-MM-dd hh:mm")));

    model->appendRow(row);
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

void FilePanel::loadDirectory()
{
    dir = new QDir(currentPath);
    if (!dir->exists()) {
        return;
    }

    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot;
    entries = dir->entryInfoList(filters, QDir::NoSort);
    addAllEntries();
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
    dir = new QDir(currentPath);
    QString selectedName;

    if (name == "") {
        // Going up: Select the previous directory name after load
        selectedName = dir->dirName(); // Basename of current path
        dir->cdUp();
        currentPath = dir->absolutePath();
    } else {
        // Check if dir
        QFileInfo info(dir->absoluteFilePath(name));
        if (info.isDir()) {
            dir->cd(name);
            currentPath = dir->absolutePath();
            selectedName = ""; // Select first item ([..]) when going down
        } else {
            // Handle file open if needed (currently do nothing)
            return;
        }
    }

    // Reload directory
    loadDirectory();
    selectEntryByName(selectedName);
}

FilePanel::FilePanel(QSplitter *splitter) {
    model = new QStandardItemModel(nullptr);
    QStringList headers = {"id","Name", "Type", "Size", "Date"};
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

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    setColumnWidth(1, 100);
    setColumnWidth(3, 150);

    QHeaderView* header = horizontalHeader();
    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    header->setHighlightSections(false);


    connect(header, &QHeaderView::sectionClicked,
            this, &FilePanel::onHeaderSectionClicked);

    splitter->addWidget(this);

    connect(this, &QTableView::activated, [this](const QModelIndex &index) {
        onPanelActivated(index);
    });

    initSearchEdit();
    loadDirectory();

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

void FilePanel::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);

    if (!searchEdit || !searchEdit->isVisible())
        return;

    const int margin = 4;
    const int h = searchEdit->sizeHint().height();

    // Use full widget rect, not viewport geometry, so we do not overlap header layout
    QRect r = rect();

    // Place search bar at the bottom, above the widget's bottom edge
    QRect editRect(r.left() + margin,
                   r.bottom() - h - margin,
                   r.width() - 2 * margin,
                   h);

    searchEdit->setGeometry(editRect);
    searchEdit->raise();
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

void FilePanel::updateSearchGeometry()
{
    // możesz tu wywołać fragment z resizeEvent albo po prostu:
    QResizeEvent ev(size(), size());
    resizeEvent(&ev);
}

void FilePanel::keyPressEvent(QKeyEvent* event) {
    // Ctrl+S: explicit search start, empty pattern
    if (event->key() == Qt::Key_S &&
    (event->modifiers() & Qt::ControlModifier)) {
        if (!searchEdit)
            initSearchEdit();

        if (searchEdit) {
            searchEdit->show();
            searchEdit->raise();
            updateSearchGeometry();
            searchEdit->setFocus();

            searchEdit->blockSignals(true);
            searchEdit->clear();
            searchEdit->blockSignals(false);

            lastSearchText.clear();
        }
        return;
    }

    // Printable char without modifiers: start / continue search
    if (event->modifiers() == Qt::NoModifier &&
        !event->text().isEmpty()) {

        const QChar ch = event->text().at(0);
        if (!ch.isNull() && !ch.isSpace()) {
            if (!searchEdit)
                initSearchEdit();

            if (searchEdit) {
                if (!searchEdit->isVisible())
                    searchEdit->show();
                searchEdit->raise();
                updateSearchGeometry();

                // If searchEdit does not have focus yet, we start a new pattern
                if (!searchEdit->hasFocus()) {
                    searchEdit->blockSignals(true);
                    searchEdit->setText(QString(ch));
                    searchEdit->blockSignals(false);
                    lastSearchText.clear();
                    searchEdit->setFocus();
                    // Manual trigger search for single char
                    onSearchTextChanged(searchEdit->text());
                } else {
                    // Let QLineEdit handle the character; our onSearchTextChanged will react
                    QTableView::keyPressEvent(event);
                }
                return;
            }
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
    // Przeładuj katalog z nowym sortowaniem
    loadDirectory();
    // Ensure first visible item is shown
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
