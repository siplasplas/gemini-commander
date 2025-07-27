#include "MainWindow.h"
#include "DirectoryFirstProxyModel.h"

#include <QSplitter>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QTableView>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupModels();
    setupUi();
    setWindowTitle("Gemini Commander");
    resize(1024, 768);
}

void MainWindow::setupModels()
{
    leftSourceModel = new QFileSystemModel(this);
    rightSourceModel = new QFileSystemModel(this);

    QDir::Filters filters = QDir::AllDirs | QDir::Files;
    leftSourceModel->setFilter(filters);
    rightSourceModel->setFilter(filters);

    QString homePath = QDir::homePath();
    leftSourceModel->setRootPath(homePath);
    rightSourceModel->setRootPath(homePath);

    leftProxyModel = new DirectoryFirstProxyModel(this);
    rightProxyModel = new DirectoryFirstProxyModel(this);

    leftProxyModel->setSourceModel(leftSourceModel);
    rightProxyModel->setSourceModel(rightSourceModel);
}


void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);

    leftTableView = new QTableView(mainSplitter);
    rightTableView = new QTableView(mainSplitter);

    QTableView* views[] = {leftTableView, rightTableView};
    DirectoryFirstProxyModel* proxyModels[] = {leftProxyModel, rightProxyModel};
    QFileSystemModel* sourceModels[] = {leftSourceModel, rightSourceModel};

    for (int i = 0; i < 2; ++i)
    {
        QTableView* currentView = views[i];
        DirectoryFirstProxyModel* currentProxy = proxyModels[i];
        QFileSystemModel* currentSource = sourceModels[i];

        currentView->setModel(currentProxy);

        currentView->setRootIndex(currentProxy->mapFromSource(currentSource->index(currentSource->rootPath())));

        currentView->hideColumn(2);

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

        currentView->setSortingEnabled(true);
        currentView->sortByColumn(0, Qt::AscendingOrder);

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

    connect(leftTableView, &QTableView::doubleClicked, this, &MainWindow::onLeftPanelDoubleClick);
    connect(rightTableView, &QTableView::doubleClicked, this, &MainWindow::onRightPanelDoubleClick);


    mainLayout->addWidget(mainSplitter);
    mainLayout->addWidget(commandLineEdit);
    mainLayout->setStretchFactor(mainSplitter, 1);
    mainLayout->setStretchFactor(commandLineEdit, 0);

    setCentralWidget(centralWidget);
}

void MainWindow::onLeftPanelDoubleClick(const QModelIndex &proxyIndex)
{
    QModelIndex sourceIndex = leftProxyModel->mapToSource(proxyIndex);

    if (leftSourceModel->isDir(sourceIndex)) {
        leftTableView->setRootIndex(proxyIndex);
    }
}

void MainWindow::onRightPanelDoubleClick(const QModelIndex &proxyIndex)
{
    QModelIndex sourceIndex = rightProxyModel->mapToSource(proxyIndex);
    if (rightSourceModel->isDir(sourceIndex)) {
        rightTableView->setRootIndex(proxyIndex);
    }
}