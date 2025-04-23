#include "MainWindow.h"

#include <QSplitter>
#include <QLineEdit>
#include <QFrame>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setWindowTitle("Gemini Commander");
    resize(800, 600);
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);

    leftPanel = new QFrame(mainSplitter);
    leftPanel->setFrameShape(QFrame::StyledPanel);
    leftPanel->setMinimumWidth(200);

    rightPanel = new QFrame(mainSplitter);
    rightPanel->setFrameShape(QFrame::StyledPanel);
    rightPanel->setMinimumWidth(200);

    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);

    commandLineEdit = new QLineEdit(centralWidget);

    mainLayout->addWidget(mainSplitter);
    mainLayout->addWidget(commandLineEdit);

    mainLayout->setStretchFactor(mainSplitter, 1);
    mainLayout->setStretchFactor(commandLineEdit, 0);

    setCentralWidget(centralWidget);
}