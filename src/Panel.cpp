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

    // Determine sort flags
    QDir::SortFlags sortFlags = QDir::DirsFirst | QDir::IgnoreCase;

    switch (sortColumn) {
    case COLUMN_NAME:
        sortFlags |= QDir::Name;
        break;
    case COLUMN_SIZE:
        sortFlags |= QDir::Size;
        break;
    case COLUMN_DATE:
        sortFlags |= QDir::Time;
        break;
    default:
        sortFlags |= QDir::Name;
        break;
    }

    if (sortOrder == Qt::DescendingOrder) {
        sortFlags |= QDir::Reversed;
    }

    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot;
    QFileInfoList entries = dir.entryInfoList(filters, sortFlags);

    // Add "[..]" if not root
    bool isRoot = dir.isRoot();
    if (!isRoot) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem(""));
        row.append(new QStandardItem("[..]"));
        row.append(new QStandardItem(""));
        row.append(new QStandardItem("<DIR>"));
        QFileInfo info(".");
        row.append(new QStandardItem(info.lastModified().toString("yyyy-MM-dd hh:mm")));
        model->appendRow(row);
    }

    // Add all sorted entries
    for (const QFileInfo &info : entries) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem("id"));
        row.append(new QStandardItem(info.fileName()));
        if (info.isDir()) {
            row.append(new QStandardItem("Directory"));
        } else {
            row.append(new QStandardItem("File"));
        }
        row.append(new QStandardItem(QString::number(info.size())));
        row.append(new QStandardItem(info.lastModified().toString("yyyy-MM-dd hh:mm")));
        model->appendRow(row);
    }

    tableView->setRootIndex(QModelIndex());
}

void Panel::onPanelActivated(const QModelIndex &index) {
    if (!index.isValid()) return;

    QString name = model->data(index.sibling(index.row(), 0)).toString(); // Name column

    QDir dir(currentPath);
    QString selectedName;

    if (name == "[..]") {
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
            selectedName = "[..]"; // Select first item ([..]) when going down
        } else {
            // Handle file open if needed (currently do nothing)
            return;
        }
    }

    // Reload directory
    loadDirectory();

    // Select and set current index for the appropriate row
    if (!selectedName.isEmpty()) {
        for (int row = 0; row < model->rowCount(); ++row) {
            QString rowName = model->item(row, 0)->text();
            if (rowName == selectedName) {
                QModelIndex selectIndex = model->index(row, 0);
                tableView->setCurrentIndex(selectIndex); // Sets both selection and current for keyboard navigation
                tableView->scrollTo(selectIndex); // Optional: Scroll to the selected item
                tableView->setFocus(); // Ensure the view has focus for keyboard input
                break;
            }
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
