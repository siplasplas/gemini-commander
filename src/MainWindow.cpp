#include "MainWindow.h"

#include <QSplitter>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QTableView>
#include <QHeaderView>
#include <QDir>
#include <QFileInfo>
#include <QStandardItemModel>
#include <QFontMetrics>
#include <algorithm> // For sorting
#include <QItemSelectionModel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupModels();
    setupUi();

    // Initial load after views are created and models are set
    loadDirectory(leftModel, leftCurrentPath, leftTableView);
    loadDirectory(rightModel, rightCurrentPath, rightTableView);

    // Set initial selection and focus
    if (leftModel->rowCount() > 0) {
        QModelIndex initialIndex = leftModel->index(0, 0);
        leftTableView->setCurrentIndex(initialIndex);
    }
    if (rightModel->rowCount() > 0) {
        QModelIndex initialIndex = rightModel->index(0, 0);
        rightTableView->setCurrentIndex(initialIndex);
    }

    setWindowTitle("Gemini Commander");
    resize(1024, 768);
}

void MainWindow::setupModels()
{
    leftModel = new QStandardItemModel(this);
    rightModel = new QStandardItemModel(this);

    // Set column headers (Name, Size, Type, Modified)
    QStringList headers = {"Name", "Size", "Type", "Modified"};
    leftModel->setHorizontalHeaderLabels(headers);
    rightModel->setHorizontalHeaderLabels(headers);

    QString homePath = QDir::homePath();
    leftCurrentPath = homePath;
    rightCurrentPath = homePath;
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);

    leftTableView = new QTableView(mainSplitter);
    rightTableView = new QTableView(mainSplitter);

    QTableView* views[] = {leftTableView, rightTableView};
    QStandardItemModel* models[] = {leftModel, rightModel};

    for (int i = 0; i < 2; ++i)
    {
        QTableView* currentView = views[i];
        QStandardItemModel* currentModel = models[i];

        currentView->setModel(currentModel);

        currentView->hideColumn(2); // Hide "Type" column if not needed

        currentView->setSelectionBehavior(QAbstractItemView::SelectRows);
        currentView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        currentView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        currentView->setShowGrid(false);
        currentView->verticalHeader()->hide();
        currentView->setAlternatingRowColors(true);

        // Set a fixed line height to make it more compact
        QFontMetrics fm(currentView->font());
        int rowHeight = fm.height();
        currentView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        currentView->verticalHeader()->setDefaultSectionSize(rowHeight);

        // Note: Sorting is handled manually in loadDirectory, so disable view sorting
        currentView->setSortingEnabled(false);

        currentView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        currentView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        currentView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
        currentView->setColumnWidth(1, 100);
        currentView->setColumnWidth(3, 150);
    }

    mainSplitter->addWidget(leftTableView);
    mainSplitter->addWidget(rightTableView);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);

    commandLineEdit = new QLineEdit(centralWidget);

    connect(leftTableView, &QTableView::activated, this, &MainWindow::onLeftPanelActivated);
    connect(rightTableView, &QTableView::activated, this, &MainWindow::onRightPanelActivated);

    mainLayout->addWidget(mainSplitter);
    mainLayout->addWidget(commandLineEdit);
    mainLayout->setStretchFactor(mainSplitter, 1);
    mainLayout->setStretchFactor(commandLineEdit, 0);

    setCentralWidget(centralWidget);
}

void MainWindow::loadDirectory(QStandardItemModel *model, const QString &path, QTableView *view)
{
    model->removeRows(0, model->rowCount()); // Clear existing items

    QDir dir(path);
    if (!dir.exists()) {
        return; // Handle error if needed
    }

    // Filters: All dirs and files, no "." and ".."
    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot;
    QFileInfoList entries = dir.entryInfoList(filters, QDir::Name | QDir::IgnoreCase);

    // Separate dirs and files for "directories first" sorting
    QList<QFileInfo> dirs;
    QList<QFileInfo> files;
    for (const QFileInfo &info : entries) {
        if (info.fileName() == ".") continue; // Explicitly skip "."
        if (info.isDir()) {
            dirs.append(info);
        } else {
            files.append(info);
        }
    }

    // Sort dirs and files alphabetically (case-insensitive already from QDir)
    std::sort(dirs.begin(), dirs.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.fileName().toLower() < b.fileName().toLower();
    });
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.fileName().toLower() < b.fileName().toLower();
    });

    // Add "[..]" if not root
    bool isRoot = dir.isRoot();
    if (!isRoot) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem("[..]"));
        row.append(new QStandardItem("")); // Size empty for dir
        row.append(new QStandardItem("Directory")); // Type
        row.append(new QStandardItem("")); // Modified empty
        model->appendRow(row);
    }

    // Add dirs
    for (const QFileInfo &info : dirs) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem(info.fileName()));
        row.append(new QStandardItem("")); // Size empty for dir
        row.append(new QStandardItem("Directory"));
        row.append(new QStandardItem(info.lastModified().toString("yyyy-MM-dd hh:mm")));
        model->appendRow(row);
    }

    // Add files
    for (const QFileInfo &info : files) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem(info.fileName()));
        row.append(new QStandardItem(QString::number(info.size())));
        row.append(new QStandardItem("File"));
        row.append(new QStandardItem(info.lastModified().toString("yyyy-MM-dd hh:mm")));
        model->appendRow(row);
    }

    // Set root index to show the entire model
    view->setRootIndex(QModelIndex());
}

void MainWindow::onLeftPanelActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QString name = leftModel->data(index.sibling(index.row(), 0)).toString(); // Name column

    QDir dir(leftCurrentPath);
    QString selectedName;

    if (name == "[..]") {
        // Going up: Select the previous directory name after load
        selectedName = dir.dirName(); // Basename of current path
        dir.cdUp();
        leftCurrentPath = dir.absolutePath();
    } else {
        // Check if dir
        QFileInfo info(dir.absoluteFilePath(name));
        if (info.isDir()) {
            dir.cd(name);
            leftCurrentPath = dir.absolutePath();
            selectedName = "[..]"; // Select first item ([..]) when going down
        } else {
            // Handle file open if needed (currently do nothing)
            return;
        }
    }

    // Reload directory
    loadDirectory(leftModel, leftCurrentPath, leftTableView);

    // Select and set current index for the appropriate row
    if (!selectedName.isEmpty()) {
        for (int row = 0; row < leftModel->rowCount(); ++row) {
            QString rowName = leftModel->item(row, 0)->text();
            if (rowName == selectedName) {
                QModelIndex selectIndex = leftModel->index(row, 0);
                leftTableView->setCurrentIndex(selectIndex); // Sets both selection and current for keyboard navigation
                leftTableView->scrollTo(selectIndex); // Optional: Scroll to the selected item
                leftTableView->setFocus(); // Ensure the view has focus for keyboard input
                break;
            }
        }
    }
}

void MainWindow::onRightPanelActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QString name = rightModel->data(index.sibling(index.row(), 0)).toString(); // Name column

    QDir dir(rightCurrentPath);
    QString selectedName;

    if (name == "[..]") {
        // Going up: Select the previous directory name after load
        selectedName = dir.dirName(); // Basename of current path
        dir.cdUp();
        rightCurrentPath = dir.absolutePath();
    } else {
        // Check if dir
        QFileInfo info(dir.absoluteFilePath(name));
        if (info.isDir()) {
            dir.cd(name);
            rightCurrentPath = dir.absolutePath();
            selectedName = "[..]"; // Select first item ([..]) when going down
        } else {
            // Handle file open if needed (currently do nothing)
            return;
        }
    }

    // Reload directory
    loadDirectory(rightModel, rightCurrentPath, rightTableView);

    // Select and set current index for the appropriate row
    if (!selectedName.isEmpty()) {
        for (int row = 0; row < rightModel->rowCount(); ++row) {
            QString rowName = rightModel->item(row, 0)->text();
            if (rowName == selectedName) {
                QModelIndex selectIndex = rightModel->index(row, 0);
                rightTableView->setCurrentIndex(selectIndex); // Sets both selection and current for keyboard navigation
                rightTableView->scrollTo(selectIndex); // Optional: Scroll to the selected item
                rightTableView->setFocus(); // Ensure the view has focus for keyboard input
                break;
            }
        }
    }
}