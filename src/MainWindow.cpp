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
#include <QKeyEvent> // For key event handling

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

    // Connect using lambda to pass isLeft flag
    connect(leftTableView, &QTableView::activated, [this](const QModelIndex &index) {
        onPanelActivated(index, true);
    });
    connect(rightTableView, &QTableView::activated, [this](const QModelIndex &index) {
        onPanelActivated(index, false);
    });

    // Install event filters for Tab key handling
    leftTableView->installEventFilter(this);
    rightTableView->installEventFilter(this);

    mainLayout->addWidget(mainSplitter);
    mainLayout->addWidget(commandLineEdit);
    mainLayout->setStretchFactor(mainSplitter, 1);
    mainLayout->setStretchFactor(commandLineEdit, 0);

    setCentralWidget(centralWidget);
    styleActive(leftTableView);
    styleInactive(rightTableView);
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

void MainWindow::onPanelActivated(const QModelIndex &index, bool isLeft)
{
    if (!index.isValid()) return;

    QStandardItemModel *model = isLeft ? leftModel : rightModel;
    QString &currentPath = isLeft ? leftCurrentPath : rightCurrentPath;
    QTableView *view = isLeft ? leftTableView : rightTableView;

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
    loadDirectory(model, currentPath, view);

    // Select and set current index for the appropriate row
    if (!selectedName.isEmpty()) {
        for (int row = 0; row < model->rowCount(); ++row) {
            QString rowName = model->item(row, 0)->text();
            if (rowName == selectedName) {
                QModelIndex selectIndex = model->index(row, 0);
                view->setCurrentIndex(selectIndex); // Sets both selection and current for keyboard navigation
                view->scrollTo(selectIndex); // Optional: Scroll to the selected item
                view->setFocus(); // Ensure the view has focus for keyboard input
                break;
            }
        }
    }
}

void MainWindow::styleActive(QWidget* widget) {
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

void MainWindow::styleInactive(QWidget* widget) {
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

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab) {
            if (obj == leftTableView) {
                rightTableView->setFocus();
                return true; // Event handled
            } else if (obj == rightTableView) {
                leftTableView->setFocus();
                return true; // Event handled
            }
        }
    } else if (event->type() == QEvent::FocusIn) {
        if (obj == leftTableView || obj == rightTableView) {
            styleActive(dynamic_cast<QWidget*>(obj));
        }
    } else if (event->type() == QEvent::FocusOut) {
        if (obj == leftTableView || obj == rightTableView) {
            styleInactive(dynamic_cast<QWidget*>(obj));
        }
    }
    return QMainWindow::eventFilter(obj, event);
}