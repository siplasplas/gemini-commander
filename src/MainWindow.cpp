#include "MainWindow.h"
#include "Panel.h"

#include <QSplitter>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QDir>
#include <QKeyEvent> // For key event handling
#include <QStandardItemModel>
#include <QTableView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    for (auto & panel : panels)
        panel->loadDirectory();

    setWindowTitle("Gemini Commander");
    resize(1024, 768);
}

MainWindow::~MainWindow() {
    for (int i=0; i<panels.size(); i++)
        delete panels[i];
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);

    for (int i=0; i<numPanels; i++)
        panels.push_back(new Panel(mainSplitter));

    commandLineEdit = new QLineEdit(centralWidget);

    // Install event filters for Tab key handling
    for (int i = 0; i < panels.size(); ++i)
        panels[i]->tableView->installEventFilter(this);
    commandLineEdit->installEventFilter(this);

    mainLayout->addWidget(mainSplitter);
    mainLayout->addWidget(commandLineEdit);
    mainLayout->setStretchFactor(mainSplitter, 1);
    mainLayout->setStretchFactor(commandLineEdit, 0);

    setCentralWidget(centralWidget);
    for (int i = 0; i < panels.size(); ++i)
        panels[i]->active(i==nPanel);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
        if (keyEvent->key() == Qt::Key_Tab) {
            auto view = dynamic_cast<QTableView*> (obj);
            if (view) {
                int n = numberForWidget(view);
                int next = (n+1) % panels.size();
                panels[next]->tableView->setFocus();
                return true; // Event handled
            }
        } else if ((keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right) && modifiers == Qt::NoModifier) {
            commandLineEdit->setFocus();
            commandLineEdit->selectAll();
            return true; // Event handled
        }
        else if (obj==commandLineEdit && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) && modifiers == Qt::NoModifier) {
            panels[nPanel]->tableView->setFocus();
            return true; // Event handled
        }
        else if (keyEvent->key() == Qt::Key_P && modifiers == Qt::ControlModifier) {
            // Ctrl + P: Set current directory to commandLineEdit
            QString currentPath = panels[nPanel]->currentPath;
            commandLineEdit->setText(commandLineEdit->text() + currentPath);
            return true; // Event handled
        } else if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
            if (modifiers == Qt::ControlModifier) {
                // Ctrl + Enter: Set selected item name to commandLineEdit
                QModelIndex currentIndex = panels[nPanel]->tableView->currentIndex();
                if (currentIndex.isValid()) {
                    QString name = panels[nPanel]->model->data(currentIndex.sibling(currentIndex.row(), 0)).toString();
                    commandLineEdit->setText(commandLineEdit->text() + name);
                    return true; // Event handled
                }
            } else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
                // Shift + Ctrl + Enter: Set full path of selected item to commandLineEdit
                QModelIndex currentIndex = panels[nPanel]->tableView->currentIndex();
                if (currentIndex.isValid()) {
                    QString name = panels[nPanel]->model->data(currentIndex.sibling(currentIndex.row(), 0)).toString();
                    if (name == "[..]") {
                        name = "..";
                    }
                    QDir dir(panels[nPanel]->currentPath);
                    QString fullPath = dir.absoluteFilePath(name);
                    commandLineEdit->setText(commandLineEdit->text() + fullPath);
                    return true; // Event handled
                }
            }
            // Plain Enter: Let default handling (activated signal) proceed
        }
    } else if (event->type() == QEvent::FocusIn) {
        auto view = dynamic_cast<QTableView*> (obj);
        if (view) {
            nPanel = numberForWidget(view);
            panels[nPanel]->active(true);
        }
    } else if (event->type() == QEvent::FocusOut) {
        auto view = dynamic_cast<QTableView*> (obj);
        if (view) {
            panels[numberForWidget(view)]->active(false);
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

int MainWindow::numberForWidget(QTableView *widget) {
    for (int i=0; i<panels.size(); ++i)
        if (panels[i]->tableView == widget)
            return i;
    return -1;
}
