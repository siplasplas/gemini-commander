#include "MainWindow.h"
#include "SearchDialog.h"
#include "SortedDirIterator.h"

#include <QMenuBar>
#include <QMenu>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenus();
    setWindowTitle("MIME Explorer");
    resize(900, 600);
}

void MainWindow::setupUi()
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    m_treeWidget = new QTreeWidget(central);
    m_treeWidget->setHeaderLabels({"Name", "Default Application", "Count"});
    m_treeWidget->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_treeWidget->header()->setStretchLastSection(false);
    m_treeWidget->setColumnWidth(0, 400);
    m_treeWidget->setColumnWidth(1, 200);
    m_treeWidget->setColumnWidth(2, 80);

    // Lazy load default app when item is expanded
    connect(m_treeWidget, &QTreeWidget::itemExpanded, this, &MainWindow::onItemExpanded);

    m_statusLabel = new QLabel(central);
    m_statusLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 4px; }");

    layout->addWidget(m_treeWidget);
    layout->addWidget(m_statusLabel);

    setCentralWidget(central);
}

void MainWindow::setupMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* searchAction = new QAction(tr("&Search Files..."), this);
    searchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(searchAction, &QAction::triggered, this, &MainWindow::onSearchFiles);
    fileMenu->addAction(searchAction);

    fileMenu->addSeparator();

    QAction* quitAction = new QAction(tr("&Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAction);

    QMenu* mimeMenu = menuBar()->addMenu(tr("&Mime"));

    QAction* allMimesAction = new QAction(tr("Show &All MIME Types"), this);
    allMimesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(allMimesAction, &QAction::triggered, this, &MainWindow::onShowAllMimes);
    mimeMenu->addAction(allMimesAction);
}

void MainWindow::onSearchFiles()
{
    SearchDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        m_treeWidget->clear();
        m_statusLabel->setText("Searching...");

        QString searchPath = dialog.searchPath();
        QMimeDatabase::MatchMode matchMode = dialog.matchMode();

        // Use SortedDirIterator for recursive search
        QDir::Filters filters = QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden;
        SortedDirIterator iter(searchPath, filters);

        int fileCount = 0;
        bool cancelled = false;

        while (iter.hasNext() && !cancelled) {
            QFileInfo info = iter.next();

            if (info.isFile()) {
                QMimeType mt = m_mimeDb.mimeTypeForFile(info.absoluteFilePath(), matchMode);
                QString mimeName = mt.name();

                // Split mime type: "image/jpeg" -> "image", "jpeg"
                QStringList parts = mimeName.split('/');
                if (parts.size() == 2) {
                    QString category = parts[0];
                    QString subType = parts[1];
                    QString extension = info.suffix().toLower();
                    if (extension.isEmpty())
                        extension = "(no extension)";

                    addFileToTree(info.absoluteFilePath(), mimeName, extension);
                }

                fileCount++;

                // Update progress every 1000 files
                if (fileCount % 1000 == 0) {
                    m_statusLabel->setText(QString("Processed %1 files...").arg(fileCount));
                    QApplication::processEvents();

                    // Check if user wants to cancel (could add a cancel button)
                }
            }
        }

        m_statusLabel->setText(QString("Done. Found %1 files.").arg(fileCount));
    }
}

void MainWindow::addFileToTree(const QString& filePath, const QString& mimeType, const QString& extension)
{
    QStringList parts = mimeType.split('/');
    if (parts.size() != 2)
        return;

    QString category = parts[0];
    QString subType = parts[1];

    // Find or create category (level 1)
    QTreeWidgetItem* categoryItem = findOrCreateCategory(category);

    // Find or create subtype (level 2)
    QTreeWidgetItem* subTypeItem = findOrCreateSubType(categoryItem, subType);

    // Update default app info for subtype
    if (subTypeItem->text(1).isEmpty()) {
        QString defaultApp = getDefaultAppForMime(mimeType);
        subTypeItem->setText(1, defaultApp);
    }

    // Find or create extension (level 3)
    QTreeWidgetItem* extItem = findOrCreateExtension(subTypeItem, extension);

    // Add file path (level 4)
    QTreeWidgetItem* fileItem = new QTreeWidgetItem(extItem);
    fileItem->setText(0, filePath);

    // Update counts
    int extCount = extItem->text(2).toInt() + 1;
    extItem->setText(2, QString::number(extCount));

    int subCount = subTypeItem->text(2).toInt() + 1;
    subTypeItem->setText(2, QString::number(subCount));

    int catCount = categoryItem->text(2).toInt() + 1;
    categoryItem->setText(2, QString::number(catCount));
}

QTreeWidgetItem* MainWindow::findOrCreateCategory(const QString& category)
{
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_treeWidget->topLevelItem(i);
        if (item->text(0) == category)
            return item;
    }

    QTreeWidgetItem* item = new QTreeWidgetItem(m_treeWidget);
    item->setText(0, category);
    item->setText(2, "0");
    return item;
}

QTreeWidgetItem* MainWindow::findOrCreateSubType(QTreeWidgetItem* parent, const QString& subType)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem* item = parent->child(i);
        if (item->text(0) == subType)
            return item;
    }

    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    item->setText(0, subType);
    item->setText(2, "0");
    return item;
}

QTreeWidgetItem* MainWindow::findOrCreateExtension(QTreeWidgetItem* parent, const QString& extension)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem* item = parent->child(i);
        if (item->text(0) == extension)
            return item;
    }

    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    item->setText(0, extension);
    item->setText(2, "0");
    return item;
}

QString MainWindow::getDefaultAppForMime(const QString& mimeType)
{
    // Read directly from mimeapps.list files (no external process)
    static QStringList mimeappsFiles = {
        QDir::homePath() + "/.config/mimeapps.list",
        QDir::homePath() + "/.local/share/applications/mimeapps.list",
        "/usr/share/applications/mimeapps.list"
    };

    static QStringList desktopSearchPaths = {
        QDir::homePath() + "/.local/share/applications",
        "/usr/share/applications",
        "/usr/local/share/applications"
    };

    QString desktopFile;

    // Find default app in mimeapps.list files
    for (const QString& mimeappsPath : mimeappsFiles) {
        QFile file(mimeappsPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream in(&file);
        bool inDefaultSection = false;

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();

            if (line.startsWith('[')) {
                inDefaultSection = (line == "[Default Applications]");
                continue;
            }

            if (inDefaultSection && line.startsWith(mimeType + "=")) {
                desktopFile = line.mid(mimeType.length() + 1).split(';').first();
                break;
            }
        }

        if (!desktopFile.isEmpty())
            break;
    }

    if (desktopFile.isEmpty())
        return QString();

    // Get app name from .desktop file
    for (const QString& searchPath : desktopSearchPaths) {
        QString fullPath = searchPath + "/" + desktopFile;
        QFile file(fullPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith("Name=")) {
                    return line.mid(5);
                }
            }
        }
    }

    // Fallback: return desktop file name without .desktop
    if (desktopFile.endsWith(".desktop"))
        return desktopFile.left(desktopFile.length() - 8);
    return desktopFile;
}

void MainWindow::onShowAllMimes()
{
    m_treeWidget->clear();
    m_statusLabel->setText("Loading all MIME types...");
    QApplication::processEvents();

    populateAllMimes();

    int count = 0;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        count += m_treeWidget->topLevelItem(i)->childCount();
    }
    m_statusLabel->setText(QString("Showing %1 MIME types in %2 categories.")
                           .arg(count)
                           .arg(m_treeWidget->topLevelItemCount()));
}

void MainWindow::populateAllMimes()
{
    QList<QMimeType> allMimes = m_mimeDb.allMimeTypes();

    // Group by category
    QMap<QString, QList<QMimeType>> grouped;

    for (const QMimeType& mt : allMimes) {
        QString name = mt.name();
        QStringList parts = name.split('/');
        if (parts.size() == 2) {
            grouped[parts[0]].append(mt);
        }
    }

    int totalMimes = allMimes.size();
    int processed = 0;

    // Build tree
    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
        const QString& category = it.key();
        const QList<QMimeType>& mimes = it.value();

        QTreeWidgetItem* categoryItem = new QTreeWidgetItem(m_treeWidget);
        categoryItem->setText(0, category);
        categoryItem->setText(2, QString::number(mimes.size()));

        for (const QMimeType& mt : mimes) {
            QString subType = mt.name().split('/').last();

            QTreeWidgetItem* subItem = new QTreeWidgetItem(categoryItem);
            subItem->setText(0, subType);
            // Store full mime name for lazy loading of default app
            subItem->setData(0, Qt::UserRole, mt.name());

            // Add extensions as children
            QStringList suffixes = mt.suffixes();
            if (!suffixes.isEmpty()) {
                for (const QString& suffix : suffixes) {
                    QTreeWidgetItem* extItem = new QTreeWidgetItem(subItem);
                    extItem->setText(0, "." + suffix);
                }
            }

            // Add aliases as info
            QStringList aliases = mt.aliases();
            if (!aliases.isEmpty()) {
                QTreeWidgetItem* aliasItem = new QTreeWidgetItem(subItem);
                aliasItem->setText(0, "Aliases: " + aliases.join(", "));
                aliasItem->setForeground(0, Qt::gray);
            }

            // Add parent mimes
            QStringList parents = mt.parentMimeTypes();
            if (!parents.isEmpty()) {
                QTreeWidgetItem* parentItem = new QTreeWidgetItem(subItem);
                parentItem->setText(0, "Parents: " + parents.join(", "));
                parentItem->setForeground(0, Qt::darkGray);
            }

            processed++;
            if (processed % 100 == 0) {
                m_statusLabel->setText(QString("Loading MIME types... %1/%2").arg(processed).arg(totalMimes));
                QApplication::processEvents();
            }
        }
    }

    m_treeWidget->sortItems(0, Qt::AscendingOrder);
}

void MainWindow::onItemExpanded(QTreeWidgetItem* item)
{
    // Only load default app when expanding a subtype item (level 2)
    // Level 2 items have a parent that is top-level (parent->parent() == nullptr)
    QTreeWidgetItem* parent = item->parent();
    if (parent != nullptr && parent->parent() == nullptr) {
        // This is a subtype item - load default app if not already loaded
        QString mimeType = item->data(0, Qt::UserRole).toString();
        if (!mimeType.isEmpty() && item->text(1).isEmpty()) {
            // Check cache first
            auto it = m_defaultAppCache.find(mimeType);
            if (it != m_defaultAppCache.end()) {
                item->setText(1, it.value());
            } else {
                QString defaultApp = getDefaultAppForMime(mimeType);
                m_defaultAppCache.insert(mimeType, defaultApp);
                item->setText(1, defaultApp);
            }
        }
    }
}
