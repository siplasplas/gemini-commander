#include <algorithm>

#include "Panel.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>

void Panel::active(bool active) {
    if (active)
        styleActive(tableView);
    else
        styleInactive(tableView);
}

void Panel::loadDirectory()
{
    model->removeRows(0, model->rowCount()); // Clear existing items

    QDir dir(currentPath);
    if (!dir.exists()) {
        return;
    }

    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot;
    // Bez sortowania QDir – sortujemy ręcznie
    QFileInfoList entries = dir.entryInfoList(filters, QDir::NoSort);

    // Sortowanie w stylu Total Commandera
    std::sort(entries.begin(), entries.end(),
              [this](const QFileInfo &a, const QFileInfo &b) {
                  const bool aDir = a.isDir();
                  const bool bDir = b.isDir();

                  // 1) katalogi zawsze na górze, niezależnie od sortOrder
                  if (aDir != bDir) {
                      return aDir && !bDir; // directory < file
                  }

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
                      // Sortowanie po nazwie (pełnej), katalogi i pliki osobno, ale tą samą zasadą
                      return cmpNames(asc);

                  case COLUMN_EXT:
                      // Sortowanie po rozszerzeniu (tylko pliki),
                      // katalogi między sobą nadal po nazwie
                      if (aDir && bDir) {
                          return cmpNames(asc);
                      } else if (!aDir && !bDir) {
                          const QString ea = a.suffix();
                          const QString eb = b.suffix();
                          const int cmp = ea.compare(eb, Qt::CaseInsensitive);
                          if (cmp != 0) {
                              return asc ? (cmp < 0) : (cmp > 0);
                          }
                          // dogrywka: po nazwie
                          return cmpNames(asc);
                      } else {
                          // nie powinniśmy tu trafić (dirs już rozdzielone wyżej)
                          return cmpNames(asc);
                      }

                  case COLUMN_SIZE:
                      // Jak TC: katalogi między sobą po nazwie,
                      // pliki po rozmiarze, a przy remisie po nazwie
                      if (aDir && bDir) {
                          return cmpNames(asc);
                      } else if (!aDir && !bDir) {
                          if (a.size() != b.size()) {
                              return asc ? (a.size() < b.size())
                                         : (a.size() > b.size());
                          }
                          return cmpNames(asc);
                      } else {
                          // nie powinniśmy tu trafić (dirs już rozdzielone wyżej)
                          return cmpNames(asc);
                      }

                  case COLUMN_DATE: {
                      // Sortowanie po dacie modyfikacji, katalogi też po dacie
                      const QDateTime da = a.lastModified();
                      const QDateTime db = b.lastModified();
                      if (da != db) {
                          return asc ? (da < db) : (da > db);
                      }
                      return cmpNames(asc);
                  }

                  default:
                      // Domyślnie po nazwie
                      return cmpNames(asc);
                  }
              });

    // [..] jako pierwszy wiersz (ręcznie, jak w TC)
    bool isRoot = dir.isRoot();
    if (!isRoot) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem(""));                 // COLUMN_ID
        row.append(new QStandardItem("[..]"));             // COLUMN_NAME
        row.append(new QStandardItem("<DIR>"));            // COLUMN_TYPE
        row.append(new QStandardItem(""));                 // COLUMN_SIZE
        QFileInfo info(".");
        row.append(new QStandardItem(
            info.lastModified().toString("yyyy-MM-dd hh:mm"))); // COLUMN_DATE
        model->appendRow(row);
    }

    // Dodajemy posortowane wpisy
    for (const QFileInfo &info : entries) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem("id"));                    // COLUMN_ID
        row.append(new QStandardItem(info.completeBaseName())); // COLUMN_NAME
        if (info.isDir()) {
            row.append(new QStandardItem(""));                  // COLUMN_TYPE (katalog – puste)
        } else {
            row.append(new QStandardItem(info.suffix()));       // COLUMN_TYPE = rozszerzenie
        }
        row.append(new QStandardItem(QString::number(info.size()))); // COLUMN_SIZE
        row.append(new QStandardItem(
            info.lastModified().toString("yyyy-MM-dd hh:mm"))); // COLUMN_DATE
        model->appendRow(row);
    }

    tableView->setRootIndex(QModelIndex());
}

void Panel::onPanelActivated(const QModelIndex &index) {
    if (!index.isValid()) return;

    QString name;
    if (index.row() > 0 ||currentPath=="/") {
        name = model->data(index.sibling(index.row(), COLUMN_NAME)).toString(); // Name column
        QString ext = model->data(index.sibling(index.row(), COLUMN_EXT)).toString();
        if (!ext.isEmpty())
            name += "." + ext;
    }

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

    // Select and set current index for the appropriate row
    for (int row = 0; row < model->rowCount(); ++row) {
        QString rowName;
        if (row>0 || currentPath=="/") {
            rowName = model->item(row, COLUMN_NAME)->text();
            QString ext = model->item(row, COLUMN_EXT)->text();
            if (!ext.isEmpty())
                rowName += "." + ext;
        }
        if (rowName == selectedName) {
            QModelIndex selectIndex = model->index(row, COLUMN_NAME);
            tableView->setCurrentIndex(selectIndex); // Sets both selection and current for keyboard navigation
            tableView->scrollTo(selectIndex); // Optional: Scroll to the selected item
            tableView->setFocus(); // Ensure the view has focus for keyboard input
            break;
        }
    }

}

Panel::Panel(QSplitter *splitter) {
    tableView = new QTableView(splitter);
    model = new QStandardItemModel(nullptr);
    QStringList headers = {"id","Name", "Type", "Size", "Date"};
    model->setHorizontalHeaderLabels(headers);
    currentPath = QDir::homePath();
    tableView->setModel(model);

    tableView->hideColumn(COLUMN_ID);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setShowGrid(false);
    tableView->verticalHeader()->hide();
    
    QFontMetrics fm(tableView->font());
    int rowHeight = fm.height();
    tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->verticalHeader()->setDefaultSectionSize(rowHeight);

    // Note: Sorting is handled manually in loadDirectory, so disable view sorting
    tableView->setSortingEnabled(false);

    tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    tableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    tableView->setColumnWidth(1, 100);
    tableView->setColumnWidth(3, 150);

    QHeaderView* header = tableView->horizontalHeader();
    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    header->setHighlightSections(false);


    connect(header, &QHeaderView::sectionClicked,
            this, &Panel::onHeaderSectionClicked);

    splitter->addWidget(tableView);

    connect(tableView, &QTableView::activated, [this](const QModelIndex &index) {
        onPanelActivated(index);
    });
}

Panel::~Panel() {
    delete tableView;
    delete model;
}

void Panel::styleActive(QWidget* widget) {
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

void Panel::styleInactive(QWidget* widget) {
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


void Panel::onHeaderSectionClicked(int logicalIndex)
{
    if (sortColumn == logicalIndex) {
        sortOrder = (sortOrder == Qt::AscendingOrder)
                    ? Qt::DescendingOrder
                    : Qt::AscendingOrder;
    } else {
        sortColumn = logicalIndex;
        sortOrder = Qt::AscendingOrder;
    }
    tableView->horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    // Przeładuj katalog z nowym sortowaniem
    loadDirectory();
}
