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
#include <QKeyEvent>

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

    // Filter edit
    m_filterEdit = new QLineEdit(central);
    m_filterEdit->setPlaceholderText(tr("Filter subtypes (e.g. rar, compressed, pdf)..."));
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);

    m_treeWidget = new QTreeWidget(central);
    m_treeWidget->setHeaderLabels({"Name", "Icon", "Components", "Archive Type", "Default Application", "Count"});
    m_treeWidget->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_treeWidget->header()->setStretchLastSection(false);
    m_treeWidget->setColumnWidth(0, 300);  // Name
    m_treeWidget->setColumnWidth(1, 40);   // Icon
    m_treeWidget->setColumnWidth(2, 100);  // Components
    m_treeWidget->setColumnWidth(3, 150);  // Archive Type
    m_treeWidget->setColumnWidth(4, 200);  // Default Application
    m_treeWidget->setColumnWidth(5, 80);   // Count
    m_treeWidget->setIconSize(QSize(24, 24));

    // Lazy load default app when item is expanded
    connect(m_treeWidget, &QTreeWidget::itemExpanded, this, &MainWindow::onItemExpanded);

    m_statusLabel = new QLabel(central);
    m_statusLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 4px; }");

    layout->addWidget(m_filterEdit);
    layout->addWidget(m_treeWidget);
    layout->addWidget(m_statusLabel);

    setCentralWidget(central);

    // Install event filter to catch ESC during search
    installEventFilter(this);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            m_searchCancelled = true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
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
    SearchDialog dialog(m_lastSearchPath, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_treeWidget->clear();
        m_statusLabel->setText("Searching... (Press ESC to cancel)");
        m_searchCancelled = false;

        QString searchPath = dialog.searchPath();
        m_lastSearchPath = searchPath;  // Remember for next time
        QMimeDatabase::MatchMode matchMode = dialog.matchMode();

        // Use SortedDirIterator for recursive search
        QDir::Filters filters = QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden;
        SortedDirIterator iter(searchPath, filters);

        int fileCount = 0;

        while (iter.hasNext() && !m_searchCancelled) {
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

                    // Classify archive type
                    auto [components, archiveType] = classifyArchive(mt, info.absoluteFilePath());
                    QString componentsStr = components.isEmpty() ? QString() :
                        (components.size() == 1 ? components[0] : components[0] + "," + components[1]);
                    QString archiveTypeStr = archiveTypeToString(archiveType);

                    addFileToTree(info.absoluteFilePath(), mimeName, extension, componentsStr, archiveTypeStr);
                }

                fileCount++;

                // Update progress every 1000 files
                if (fileCount % 1000 == 0) {
                    m_statusLabel->setText(QString("Processed %1 files... (Press ESC to cancel)").arg(fileCount));
                    QApplication::processEvents();
                }
            }
        }

        if (m_searchCancelled) {
            m_statusLabel->setText(QString("Cancelled. Processed %1 files.").arg(fileCount));
        } else {
            m_statusLabel->setText(QString("Done. Found %1 files.").arg(fileCount));
        }
    }
}

void MainWindow::addFileToTree(const QString& filePath, const QString& mimeType, const QString& extension,
                               const QString& components, const QString& archiveType)
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

    // Store mime type for lazy loading of default app (on expand)
    if (subTypeItem->data(0, Qt::UserRole).toString().isEmpty()) {
        subTypeItem->setData(0, Qt::UserRole, mimeType);
    }

    // Set archive info on subtype level (if not already set)
    if (subTypeItem->text(2).isEmpty() && !components.isEmpty()) {
        subTypeItem->setText(2, components);
    }
    if (subTypeItem->text(3).isEmpty() && !archiveType.isEmpty()) {
        subTypeItem->setText(3, archiveType);
    }

    // Find or create extension (level 3)
    QTreeWidgetItem* extItem = findOrCreateExtension(subTypeItem, extension);

    // Add file path (level 4)
    QTreeWidgetItem* fileItem = new QTreeWidgetItem(extItem);
    fileItem->setText(0, filePath);

    // Set icon for the file (using FileIconResolver)
    QIcon icon = FileIconResolver::instance().getIcon(filePath, true);
    if (!icon.isNull()) {
        fileItem->setIcon(0, icon);
    }

    // Update counts (now in column 5)
    int extCount = extItem->text(5).toInt() + 1;
    extItem->setText(5, QString::number(extCount));

    int subCount = subTypeItem->text(5).toInt() + 1;
    subTypeItem->setText(5, QString::number(subCount));

    int catCount = categoryItem->text(5).toInt() + 1;
    categoryItem->setText(5, QString::number(catCount));
}

QTreeWidgetItem* MainWindow::findOrCreateCategory(const QString& category)
{
        int count = m_treeWidget->topLevelItemCount();

    // Binary search for existing or insertion point
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        QString midText = m_treeWidget->topLevelItem(mid)->text(0);
        int cmp = category.compare(midText, Qt::CaseInsensitive);
        if (cmp == 0)
            return m_treeWidget->topLevelItem(mid);
        if (cmp < 0)
            hi = mid;
        else
            lo = mid + 1;
    }

    // Not found - insert at position lo
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, category);
    item->setText(5, "0");  // Count in column 5
    m_treeWidget->insertTopLevelItem(lo, item);
    return item;
}

QTreeWidgetItem* MainWindow::findOrCreateSubType(QTreeWidgetItem* parent, const QString& subType)
{
    int count = parent->childCount();

    // Binary search for existing or insertion point
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        QString midText = parent->child(mid)->text(0);
        int cmp = subType.compare(midText, Qt::CaseInsensitive);
        if (cmp == 0)
            return parent->child(mid);
        if (cmp < 0)
            hi = mid;
        else
            lo = mid + 1;
    }

    // Not found - insert at position lo
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, subType);
    item->setText(5, "0");  // Count in column 5
    parent->insertChild(lo, item);
    return item;
}

QTreeWidgetItem* MainWindow::findOrCreateExtension(QTreeWidgetItem* parent, const QString& extension)
{
    int count = parent->childCount();

    // Binary search for existing or insertion point
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        QString midText = parent->child(mid)->text(0);
        int cmp = extension.compare(midText, Qt::CaseInsensitive);
        if (cmp == 0)
            return parent->child(mid);
        if (cmp < 0)
            hi = mid;
        else
            lo = mid + 1;
    }

    // Not found - insert at position lo
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, extension);
    item->setText(5, "0");  // Count in column 5
    parent->insertChild(lo, item);
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
        categoryItem->setText(5, QString::number(mimes.size()));  // Count in column 5

        for (const QMimeType& mt : mimes) {
            QString subType = mt.name().split('/').last();

            QTreeWidgetItem* subItem = new QTreeWidgetItem(categoryItem);
            subItem->setText(0, subType);
            // Store full mime name for lazy loading of default app
            subItem->setData(0, Qt::UserRole, mt.name());

            // Classify archive type - use sample path with first suffix if available
            QStringList suffixes = mt.suffixes();
            QString samplePath = suffixes.isEmpty() ? QString() : ("sample." + suffixes.first());
            auto [components, archiveType] = classifyArchive(mt, samplePath);
            if (!components.isEmpty()) {
                QString componentsStr = components.size() == 1 ? components[0] : components[0] + "," + components[1];
                subItem->setText(2, componentsStr);
                subItem->setText(3, archiveTypeToString(archiveType));
            }

            // Add extensions as children with icons
            if (!suffixes.isEmpty()) {
                for (const QString& suffix : suffixes) {
                    QTreeWidgetItem* extItem = new QTreeWidgetItem(subItem);
                    extItem->setText(0, "." + suffix);
                    // Get icon for this extension
                    QIcon icon = FileIconResolver::instance().getIconByName("file." + suffix);
                    if (!icon.isNull()) {
                        extItem->setIcon(0, icon);
                    }
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
        if (!mimeType.isEmpty() && item->text(4).isEmpty()) {  // Column 4 = Default Application
            // Check cache first
            auto it = m_defaultAppCache.find(mimeType);
            if (it != m_defaultAppCache.end()) {
                item->setText(4, it.value());
            } else {
                QString defaultApp = getDefaultAppForMime(mimeType);
                m_defaultAppCache.insert(mimeType, defaultApp);
                item->setText(4, defaultApp);
            }
        }
    }
}

void MainWindow::onFilterChanged(const QString& text)
{
    QString filter = text.trimmed().toLower();

    // Iterate through all top-level items (categories)
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* categoryItem = m_treeWidget->topLevelItem(i);
        int visibleChildren = 0;

        // Iterate through subtypes (level 2)
        for (int j = 0; j < categoryItem->childCount(); ++j) {
            QTreeWidgetItem* subTypeItem = categoryItem->child(j);
            QString subTypeName = subTypeItem->text(0).toLower();

            // Show if filter is empty or subtype contains filter text
            bool matches = filter.isEmpty() || subTypeName.contains(filter);
            subTypeItem->setHidden(!matches);

            if (matches)
                visibleChildren++;
        }

        // Hide category if no visible children (unless filter is empty)
        categoryItem->setHidden(!filter.isEmpty() && visibleChildren == 0);

        // Expand category if it has matching children
        if (visibleChildren > 0 && !filter.isEmpty()) {
            categoryItem->setExpanded(true);
        }
    }
}
