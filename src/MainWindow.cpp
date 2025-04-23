#include "MainWindow.h"

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
}

void MainWindow::setupModels()
{
    leftModel = new QFileSystemModel(this);
    rightModel = new QFileSystemModel(this);

    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot;
    leftModel->setFilter(filters);
    rightModel->setFilter(filters);

    QString homePath = QDir::homePath();
    leftModel->setRootPath(homePath); // Important!
    rightModel->setRootPath(homePath); // Important!

    leftModel->sort(0, Qt::DescendingOrder);
}


void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);

    leftTreeView = new QTreeView(mainSplitter);
    rightTreeView = new QTreeView(mainSplitter);

    leftTreeView->setModel(leftModel);
    rightTreeView->setModel(rightModel);

    leftTreeView->setRootIndex(leftModel->index(leftModel->rootPath()));
    rightTreeView->setRootIndex(rightModel->index(rightModel->rootPath()));


    leftTreeView->hideColumn(2);
    rightTreeView->hideColumn(2);

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

    leftTreeView->setSortingEnabled(true);
    rightTreeView->setSortingEnabled(true);
    leftTreeView->sortByColumn(0, Qt::AscendingOrder); // Default sprting

    // ----------------------------------

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