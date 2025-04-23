#include "MainWindow.h"
#include "DirectoryFirstProxyModel.h"

#include <QSplitter>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupModels(); // First configure models
    setupUi();     // Next UI, which uses models
    setWindowTitle("Gemini Commander");
    resize(1024, 768);

    leftTreeView->sortByColumn(0, Qt::AscendingOrder);
    rightTreeView->sortByColumn(0, Qt::AscendingOrder);
}

void MainWindow::setupModels()
{
    leftSourceModel = new QFileSystemModel(this);
    rightSourceModel = new QFileSystemModel(this);

    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot;
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

    leftTreeView = new QTreeView(mainSplitter);
    rightTreeView = new QTreeView(mainSplitter);

    // === Configuration of Views ====.
    // 1. Set the PROXY MODEL for each view
    leftTreeView->setModel(leftProxyModel);
    rightTreeView->setModel(rightProxyModel);

    // 2. Set the root of the view by mapping the index from the source through the proxy
    // This is VERY IMPORTANT!
    leftTreeView->setRootIndex(leftProxyModel->mapFromSource(leftSourceModel->index(leftSourceModel->rootPath())));
    rightTreeView->setRootIndex(rightProxyModel->mapFromSource(rightSourceModel->index(rightSourceModel->rootPath())));


    // 3. Column configuration (indexes now refer to the proxy model, but are the same as in the source)
    leftTreeView->hideColumn(2); // Ukryj kolumnę "Typ" (indeks 2)
    rightTreeView->hideColumn(2);

    // Set the sorting in the view (this will allow the user to click on headings)
    leftTreeView->setSortingEnabled(true);
    rightTreeView->setSortingEnabled(true);

    // Adjust the width of the columns (as before).
    leftTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    leftTreeView->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    leftTreeView->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    leftTreeView->setColumnWidth(1, 100);
    leftTreeView->setColumnWidth(3, 150);

    rightTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    rightTreeView->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    rightTreeView->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    rightTreeView->setColumnWidth(1, 100);
    rightTreeView->setColumnWidth(3, 150);

    mainSplitter->addWidget(leftTreeView);
    mainSplitter->addWidget(rightTreeView);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);

    commandLineEdit = new QLineEdit(centralWidget);

    mainLayout->addWidget(mainSplitter);
    mainLayout->addWidget(commandLineEdit);
    mainLayout->setStretchFactor(mainSplitter, 1);
    mainLayout->setStretchFactor(commandLineEdit, 0);

    setCentralWidget(centralWidget);
}