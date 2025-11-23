#include "MainWindow.h"
#include "Panel.h"

#include <QSplitter>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QStandardItemModel>
#include <QTableView>
#include "editor/EditorFrame.h"

#include <QHeaderView>

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
        } else if ((keyEvent->key() == Qt::Key_F3||keyEvent->key() == Qt::Key_F4) && modifiers == Qt::NoModifier) {
            QModelIndex currentIndex = panels[nPanel]->tableView->currentIndex();
            if (!currentIndex.isValid()) {
                return true;
            }

            QString name = panels[nPanel]->model->data(
                currentIndex.sibling(currentIndex.row(), COLUMN_NAME)
            ).toString();

            if (name == "[..]") {
                return true;
            }

            QDir dir(panels[nPanel]->currentPath);
            QString fullPath = dir.absoluteFilePath(name);

            QFileInfo info(fullPath);
            if (!info.isFile()) {
                // selected is directory - nothing
                return true;
            }

            if (!editorFrame)
                editorFrame = new EditorFrame(this);

            if (keyEvent->key() == Qt::Key_F3)
                editorFrame->openFileInViewer(fullPath);
            else
                editorFrame->openFileInEditor(fullPath);

            editorFrame->show();
            editorFrame->raise();
            editorFrame->activateWindow();

            return true;
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F3) {
            Panel* panel = panels[nPanel];
            if (panel->sortColumn == COLUMN_NAME) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_NAME;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->tableView->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->loadDirectory();
            return true;

        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F6) {
            Panel* panel = panels[nPanel];
            if (panel->sortColumn == COLUMN_SIZE) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_SIZE;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->tableView->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->loadDirectory();
            return true;

        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F5) {
            Panel* panel = panels[nPanel];
            if (panel->sortColumn == COLUMN_DATE) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_DATE;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->tableView->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->loadDirectory();
            return true;

        } else if ((keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right) && modifiers == Qt::NoModifier) {
            commandLineEdit->setFocus();
            commandLineEdit->selectAll();
            return true; // Event handled
        } else if (obj==commandLineEdit && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) && modifiers == Qt::NoModifier) {
            panels[nPanel]->tableView->setFocus();
            return true; // Event handled
        }
        else if (keyEvent->key() == Qt::Key_Home && modifiers == Qt::NoModifier) {
            QTableView* view = panels[nPanel]->tableView;
            int rows = view->model()->rowCount();
            if (rows > 0) {
                QModelIndex idx = view->model()->index(0, 0);
                view->setCurrentIndex(idx);
                view->scrollTo(idx, QAbstractItemView::PositionAtTop);
            }
            return true;

        } else if (keyEvent->key() == Qt::Key_End && modifiers == Qt::NoModifier) {
            QTableView* view = panels[nPanel]->tableView;
            int rows = view->model()->rowCount();
            if (rows > 0) {
                QModelIndex idx = view->model()->index(rows - 1, 0);
                view->setCurrentIndex(idx);
                view->scrollTo(idx, QAbstractItemView::PositionAtBottom);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_P && modifiers == Qt::ControlModifier) {
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
