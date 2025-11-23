#include <algorithm>

#include "FilePanel.h"
#include <QDir>
#include <QDrag>
#include <QFileInfo>
#include <QHeaderView>
#include <QMimeData>
#include <QPainter>
#include <QStandardItemModel>
#include <QUrl>

void FilePanel::active(bool active) {
    if (active)
        styleActive(this);
    else
        styleInactive(this);
}

void FilePanel::loadDirectory()
{
    model->removeRows(0, model->rowCount());

    QDir dir(currentPath);
    if (!dir.exists()) {
        return;
    }

    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot;
    QFileInfoList entries = dir.entryInfoList(filters, QDir::NoSort);

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
                              return asc ? (a.size() < b.size())
                                         : (a.size() > b.size());
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

    // --------------------------
    // Add ".." entry
    // --------------------------
    bool isRoot = dir.isRoot();
    if (!isRoot) {
        QList<QStandardItem*> row;

        // COLUMN_ID
        row.append(new QStandardItem(""));

        // COLUMN_NAME (base)
        auto* nameItem = new QStandardItem("[..]");
        nameItem->setData(QString(""), Qt::UserRole);   // full_name = "" for [..]
        row.append(nameItem);

        // COLUMN_EXT (empty)
        row.append(new QStandardItem("<DIR>"));

        // COLUMN_SIZE
        row.append(new QStandardItem(""));

        // COLUMN_DATE
        QFileInfo info(".");
        row.append(new QStandardItem(
            info.lastModified().toString("yyyy-MM-dd hh:mm")));

        model->appendRow(row);
    }

    // --------------------------
    // Add files and directories
    // --------------------------
    for (const QFileInfo &info : entries) {

        const QString base = info.completeBaseName();
        const QString ext = info.isDir() ? QString() : info.suffix();
        QString fullName = base;
        if (!ext.isEmpty())
            fullName += "." + ext;

        QList<QStandardItem*> row;

        // COLUMN_ID
        row.append(new QStandardItem("id"));

        // COLUMN_NAME
        auto* nameItem = new QStandardItem(base);
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
        row.append(new QStandardItem(
            info.lastModified().toString("yyyy-MM-dd hh:mm")));

        model->appendRow(row);
    }

    setRootIndex(QModelIndex());
}

QString FilePanel::getRowName(int row) const {
    if (row < 0 || row >= model->rowCount())
        return {};
    return model->item(row, COLUMN_NAME)->data(Qt::UserRole).toString();
}

bool FilePanel::selectEntryByName(const QString& fullName)
{
    // For "[..]" / going up we expect empty fullName
    if (fullName.isEmpty()) {
        // row 0 (parent entry), unless we are at root
        if (currentPath != "/" && model->rowCount() > 0) {
            QModelIndex idx = model->index(0, COLUMN_NAME);
            setCurrentIndex(idx);
            scrollTo(idx);
            setFocus();
            return true;
        }
        return false;
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
        return false;

    QModelIndex selectIndex = matches.first();
    setCurrentIndex(selectIndex);
    scrollTo(selectIndex);
    setFocus();
    return true;
}


void FilePanel::onPanelActivated(const QModelIndex &index) {
    if (!index.isValid()) return;

    QString name = getRowName(index.row());
    QDir dir(currentPath);
    QString selectedName;

    if (name == "") {
        // Going up: Select the previous directory name after load
        selectedName = dir.dirName(); // Basename of current path
        dir.cdUp();
        currentPath = dir.absolutePath();
    } else {
        // Check if dir
        QFileInfo info(dir.absoluteFilePath(name));
        if (info.isDir()) {
            dir.cd(name);
            currentPath = dir.absolutePath();
            selectedName = ""; // Select first item ([..]) when going down
        } else {
            // Handle file open if needed (currently do nothing)
            return;
        }
    }

    // Reload directory
    loadDirectory();
    bool b = selectEntryByName(selectedName);
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
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
}

FilePanel::~FilePanel() {
    delete model;
}

void FilePanel::styleActive(QWidget* widget) {
    widget->setStyleSheet(
        "QTableView:item {"
        "background-color: white;"
        "color: black;"
        "}"
        "QTableView:item:selected {"
        "    background-color: blue;"
        "    color: white;"
        "}");
}

void FilePanel::styleInactive(QWidget* widget) {
    widget->setStyleSheet(
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

        QDir dir(currentPath);
        QString fullPath = dir.absoluteFilePath(name);
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
