#include "MainWindow.h"

#include "Config.h"
#include "FilePanel.h"

#include "editor/EditorFrame.h"
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <QHeaderView>
#include <QTimer>
#include <QActionGroup>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString cfg = Config::instance().defaultConfigPath();
    Config::instance().load(cfg);
    Config::instance().setConfigPath(cfg);

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
        panels.push_back(new FilePanel(mainSplitter));

    commandLineEdit = new QLineEdit(centralWidget);

    // Install event filters for Tab key handling
    for (int i = 0; i < panels.size(); ++i)
        panels[i]->installEventFilter(this);
    commandLineEdit->installEventFilter(this);

    mainLayout->addWidget(mainSplitter);
    mainLayout->addWidget(commandLineEdit);
    mainLayout->setStretchFactor(mainSplitter, 1);
    mainLayout->setStretchFactor(commandLineEdit, 0);

    setCentralWidget(centralWidget);
    for (int i = 0; i < panels.size(); ++i)
         panels[i]->active(false);
    QTimer::singleShot(0, this, [this]() {
           panels[nPanel]->setFocus();   });
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
                panels[next]->setFocus();
                return true; // Event handled
            }
        } else if ((keyEvent->key() == Qt::Key_F3||keyEvent->key() == Qt::Key_F4) && modifiers == Qt::NoModifier) {
            QModelIndex currentIndex = panels[nPanel]->currentIndex();
            if (!currentIndex.isValid()) {
                return true;
            }

            QString name = panels[nPanel]->getRowName(currentIndex.row());
            if (name == "") {
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
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_D) {
            showFavoriteDirsMenu(nPanel);
            return true;
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F3) {
            FilePanel* panel = panels[nPanel];
            if (panel->sortColumn == COLUMN_NAME) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_NAME;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->addAllEntries();
            return true;

        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F4) {
            FilePanel* panel = panels[nPanel];
            if (panel->sortColumn == COLUMN_EXT) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_EXT;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->addAllEntries();
            return true;

        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F6) {
            FilePanel* panel = panels[nPanel];
            if (panel->sortColumn == COLUMN_SIZE) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_SIZE;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->addAllEntries();
            return true;

        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_F5) {
            FilePanel* panel = panels[nPanel];
            if (panel->sortColumn == COLUMN_DATE) {
                panel->sortOrder = (panel->sortOrder == Qt::AscendingOrder)
                        ? Qt::DescendingOrder
                        : Qt::AscendingOrder;
            } else {
                panel->sortColumn = COLUMN_DATE;
                panel->sortOrder = Qt::AscendingOrder;
            }
            panel->horizontalHeader()->setSortIndicator(panel->sortColumn, panel->sortOrder);
            panel->addAllEntries();
            return true;

        } else if ((keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right) && modifiers == Qt::NoModifier) {
            commandLineEdit->setFocus();
            commandLineEdit->selectAll();
            return true; // Event handled
        } else if (obj==commandLineEdit && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) && modifiers == Qt::NoModifier) {
            panels[nPanel]->setFocus();
            return true; // Event handled
        }
        else if (keyEvent->key() == Qt::Key_Home && modifiers == Qt::NoModifier) {
            QTableView* view = panels[nPanel];
            int rows = view->model()->rowCount();
            if (rows > 0) {
                QModelIndex idx = view->model()->index(0, COLUMN_NAME);
                view->setCurrentIndex(idx);
                view->scrollTo(idx, QAbstractItemView::PositionAtTop);
            }
            return true;

        } else if (keyEvent->key() == Qt::Key_End && modifiers == Qt::NoModifier) {
            QTableView* view = panels[nPanel];
            int rows = view->model()->rowCount();
            if (rows > 0) {
                QModelIndex idx = view->model()->index(rows - 1, COLUMN_NAME);
                view->setCurrentIndex(idx);
                view->scrollTo(idx, QAbstractItemView::PositionAtBottom);
            }
            return true;
        } else if (modifiers == Qt::ControlModifier && keyEvent->key() == Qt::Key_PageUp) {
            FilePanel* panel = panels[nPanel];
            QDir dir(panel->currentPath);

            // If we are in the root directory – do nothing
            if (dir.isRoot()) {
                return true;
            }

            // Remember the name of the current directory (as in onPanelActivated)
            QString selectedName = dir.dirName();

            // Go up one directory
            dir.cdUp();
            panel->currentPath = dir.absolutePath();

            // Reload panel content
            panel->loadDirectory();
            panel->selectEntryByName(selectedName);

            return true;
        } else if (keyEvent->key() == Qt::Key_P && modifiers == Qt::ControlModifier) {
            // Ctrl + P: Set current directory to commandLineEdit
            QString currentPath = panels[nPanel]->currentPath;
            commandLineEdit->setText(commandLineEdit->text() + currentPath);
            return true; // Event handled
        } else if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
            if (modifiers == Qt::ControlModifier) {
                // Ctrl + Enter: Set selected item name to commandLineEdit
                QModelIndex currentIndex = panels[nPanel]->currentIndex();
                if (currentIndex.isValid()) {
                    QString name = panels[nPanel]->model->data(currentIndex.sibling(currentIndex.row(), 0)).toString();
                    commandLineEdit->setText(commandLineEdit->text() + name);
                    return true; // Event handled
                }
            } else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
                // Shift + Ctrl + Enter: Set full path of selected item to commandLineEdit
                QModelIndex currentIndex = panels[nPanel]->currentIndex();
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
        if (panels[i] == widget)
            return i;
    return -1;
}

void MainWindow::showFavoriteDirsMenu(int panelIndex)
{
    if (panelIndex < 0 || panelIndex >= panels.size())
        return;

    FilePanel* panel = panels[panelIndex];
    if (!panel)
        return;

    const QString currentDir = QDir::cleanPath(panel->currentPath);

    const auto& favorites = Config::instance().favoriteDirs();

    // Split favorites into root entries (no group) and grouped entries
    QVector<const FavoriteDir*> rootEntries;
    QMap<QString, QVector<const FavoriteDir*>> grouped;

    for (const auto& fav : favorites) {
        if (fav.group.isEmpty()) {
            rootEntries.append(&fav);
        } else {
            grouped[fav.group].append(&fav);
        }
    }

    QMenu menu(this);
    QActionGroup groupActions(&menu);
    groupActions.setExclusive(true);

    auto makeLabel = [](const FavoriteDir& fav) -> QString {
        if (!fav.label.isEmpty())
            return fav.label;
        QFileInfo fi(fav.path);
        QString label = fi.fileName();
        if (label.isEmpty())
            label = fav.path;
        return label;
    };

    int accelIndex = 1;

    // Root favorites – shown directly in the main menu
    for (const FavoriteDir* fav : rootEntries) {
        QString path = QDir::cleanPath(fav->path);
        QString label = makeLabel(*fav);

        QString text;
        if (accelIndex <= 9) {
            text = QString("&%1  %2").arg(accelIndex).arg(label);
            ++accelIndex;
        } else {
            text = label;
        }

        QAction* act = menu.addAction(text);
        act->setCheckable(true);
        act->setData(path);

        if (!currentDir.isEmpty() && path == currentDir)
            act->setChecked(true);

        groupActions.addAction(act);
    }

    // Grouped favorites – each group as submenu
    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
        const QString& groupName = it.key();
        const auto& entries = it.value();

        QMenu* sub = menu.addMenu(groupName);

        for (const FavoriteDir* fav : entries) {
            QString path = QDir::cleanPath(fav->path);
            QString label = makeLabel(*fav);

            QString text;
            if (accelIndex <= 9) {
                text = QString("&%1  %2").arg(accelIndex).arg(label);
                ++accelIndex;
            } else {
                text = label;
            }

            QAction* act = sub->addAction(text);
            act->setCheckable(true);
            act->setData(path);

            if (!currentDir.isEmpty() && path == currentDir)
                act->setChecked(true);

            groupActions.addAction(act);
        }
    }

    if (!favorites.isEmpty())
        menu.addSeparator();

    if (favorites.isEmpty()) {
        QAction* infoAct = menu.addAction("No dirs yet");
        infoAct->setEnabled(false);
    }

    menu.addSeparator();
    QAction* addAct = menu.addAction("Add current dir");

    bool alreadyInFavorites = Config::instance().containsFavoriteDir(currentDir);
    addAct->setEnabled(!alreadyInFavorites && !currentDir.isEmpty());

    // Popup position: above the active panel
    QPoint panelPos = panel->mapToGlobal(QPoint(panel->width() / 2, panel->height()/2));
    QAction* chosen = menu.exec(panelPos);
    if (!chosen)
        return;

    if (chosen == addAct) {
        // Add to root group ("")
        Config::instance().addFavoriteDir(currentDir, /*label*/ QString(), /*group*/ QString());
        Config::instance().save();
        return;
    }

    QString targetDir = chosen->data().toString();
    if (targetDir.isEmpty())
        return;

    QDir dir(targetDir);
    if (!dir.exists())
        return;

    panel->currentPath = dir.absolutePath();
    panel->loadDirectory();
    panel->setFocus();
}
