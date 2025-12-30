#include "FilePaneWidget.h"
#include "SearchEdit.h"
#include "SizeFormat.h"
#include "Config.h"

#include <QItemSelectionModel>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QEvent>
#include <QTabWidget>

#include "keys/ObjectRegistry.h"
#include "quitls.h"

FilePaneWidget::FilePaneWidget(Side side, QWidget* parent)
    : m_side(side), QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Path bar with favorites and history buttons
    auto* pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(0);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setStyleSheet("QLineEdit { background-color: white; }");
    m_pathEdit->installEventFilter(this);
    ObjectRegistry::add(m_pathEdit, "PathEdit");
    QFontMetrics fm(m_pathEdit->font());
    int h = fm.height() + 4;
    m_pathEdit->setFixedHeight(h);

    m_favoritesButton = new QToolButton(this);
    m_favoritesButton->setText("*");
    m_favoritesButton->setToolTip(tr("Favorite directories (Ctrl+D)"));
    m_favoritesButton->setFixedHeight(h);
    connect(m_favoritesButton, &QToolButton::clicked, this, [this]() {
        QPoint pos = m_favoritesButton->mapToGlobal(QPoint(0, m_favoritesButton->height()));
        emit favoritesRequested(pos);
    });

    m_historyButton = new QToolButton(this);
    m_historyButton->setText("▾");
    m_historyButton->setToolTip(tr("Directory history (Alt+Left/Right)"));
    m_historyButton->setFixedHeight(h);
    connect(m_historyButton, &QToolButton::clicked, this, &FilePaneWidget::showHistoryMenu);

    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(m_favoritesButton);
    pathLayout->addWidget(m_historyButton);

    m_filePanel = new FilePanel(side, nullptr);
    ObjectRegistry::add(m_filePanel, "Panel");
    m_searchEdit = new SearchEdit(this);
    ObjectRegistry::add(m_searchEdit, "SearchEdit");
    m_searchEdit->hide();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("QLabel { padding: 2px 6px; }");
    m_statusLabel->setText(QString());

    layout->addLayout(pathLayout);
    layout->addWidget(m_filePanel);
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_statusLabel);

    setLayout(layout);

    connect(m_filePanel, &FilePanel::directoryChanged,
            this, &FilePaneWidget::onDirectoryChanged);
    connect(m_filePanel, &FilePanel::selectionChanged,
            this, &FilePaneWidget::onSelectionChanged);

    // Connect history navigation signals
    connect(m_filePanel, &FilePanel::goBackRequested,
            this, &FilePaneWidget::goBack);
    connect(m_filePanel, &FilePanel::goForwardRequested,
            this, &FilePaneWidget::goForward);

    // Update status on keyboard navigation (up/down, page up/down, etc.)
    connect(m_filePanel->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &FilePaneWidget::onSelectionChanged);

    connect(m_searchEdit, &QLineEdit::textChanged,
            m_filePanel,  &FilePanel::updateSearch);

    connect(m_searchEdit, &SearchEdit::nextMatchRequested,
            m_filePanel,  &FilePanel::nextMatch);

    connect(m_searchEdit, &SearchEdit::prevMatchRequested,
            m_filePanel,  &FilePanel::prevMatch);

    connect(m_searchEdit, &SearchEdit::acceptPressed,
        this, [this]() {
            m_filePanel->rememberSelection();
            m_searchEdit->hide();
            if (m_filePanel) {
                m_filePanel->triggerCurrentEntry();
            }
        });

    connect(m_searchEdit, &SearchEdit::escapePressed,
        this, [this]() {
            m_filePanel->rememberSelection();
            m_searchEdit->hide();
        });

    // initial
    setCurrentPath(m_filePanel->currentPath);
}

void FilePaneWidget::setCurrentPath(const QString& path)
{
    m_pathEdit->setText(path);
    if (m_filePanel->currentPath != path) {
        m_filePanel->currentPath = path;
        m_filePanel->loadDirectory();
    }
}

QString FilePaneWidget::currentPath() const
{
    return m_pathEdit->text();
}

void FilePaneWidget::onDirectoryChanged(const QString& path)
{
    m_pathEdit->setText(path);
    addToHistory(path);

    // Update tab title to last path component
    // QTabWidget uses internal QStackedWidget, so we need to traverse up
    QWidget* p = parentWidget();
    while (p && !qobject_cast<QTabWidget*>(p)) {
        p = p->parentWidget();
    }
    if (auto* tabWidget = qobject_cast<QTabWidget*>(p)) {
        int tabIndex = tabWidget->indexOf(this);
        if (tabIndex >= 0) {
            QString title;
            if (path == "/" || path.isEmpty()) {
                title = "/";
            } else {
                QFileInfo info(path);
                title = info.fileName();
                if (title.isEmpty())
                    title = path;
            }
            tabWidget->setTabText(tabIndex, title);
        }
    }
}

void FilePaneWidget::onSelectionChanged()
{
    updateStatusLabel();
}

void FilePaneWidget::updateStatusLabel()
{
    auto* panel = m_filePanel;
    if (!panel) {
        m_statusLabel->clear();
        return;
    }

    // Iterate over all entries
    qint64 selectedBytes = 0;
    qint64 totalBytes = 0;
    int selectedFileCount = 0;
    int selectedDirCount = 0;
    int totalFileCount = 0;
    int totalDirCount = 0;

    for (const auto& entry : panel->entries) {
        bool isDir = entry.info.isDir();

        // Calculate size for this entry
        qint64 entrySize = 0;
        if (entry.hasTotalSize == TotalSizeStatus::Has) {
            entrySize = entry.totalSizeBytes;
        } else {
            entrySize = entry.info.size();
        }

        // Total stats
        if (isDir) {
            ++totalDirCount;
        } else {
            ++totalFileCount;
        }
        totalBytes += entrySize;

        // Selected stats (only if marked)
        if (entry.isMarked) {
            if (isDir) {
                ++selectedDirCount;
            } else {
                ++selectedFileCount;
            }
            selectedBytes += entrySize;
        }
    }

    // In branch mode with nothing marked, show branch+filename for current entry
    if (panel->branchMode && selectedFileCount == 0 && selectedDirCount == 0) {
        auto p = panel->currentEntryRow();
        if (p.first) {
            QString branchFile = p.first->branch;
            if (!branchFile.isEmpty() && !branchFile.endsWith('/'))
                branchFile += '/';
            branchFile += p.first->info.fileName();
            m_statusLabel->setText(branchFile);
            return;
        }
    }

    // Format sizes using SizeFormat (configurable)
    QString selectedSizeStr = qFormatSize(selectedBytes, Config::instance().sizeFormat());
    QString totalSizeStr = qFormatSize(totalBytes, Config::instance().sizeFormat());

    // Format counts always as precise (with thousand separators)
    QString selectedFileStr = formatWithSeparators(selectedFileCount);
    QString totalFileStr = formatWithSeparators(totalFileCount);
    QString selectedDirStr = formatWithSeparators(selectedDirCount);
    QString totalDirStr = formatWithSeparators(totalDirCount);

    // Format: "59 k / 1.31 M in 1 / 2 file(s), 0 / 1 dir(s)"
    QString text = QString("%1 / %2 in %3 / %4 file(s), %5 / %6 dir(s)")
            .arg(selectedSizeStr)
            .arg(totalSizeStr)
            .arg(selectedFileStr)
            .arg(totalFileStr)
            .arg(selectedDirStr)
            .arg(totalDirStr);
    m_statusLabel->setText(text);
}


bool FilePaneWidget::doLocalSearch(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    m_searchEdit->show();
    m_searchEdit->setFocus();
    m_searchEdit->clear();
    QString initialText = keyEvent->text();
    if (!initialText.isEmpty())
         m_searchEdit->setText(initialText);
    // inform the panel about the new text
    m_filePanel->updateSearch(m_searchEdit->text());
    return true;
}

// Directory navigation history implementation

void FilePaneWidget::addToHistory(const QString& path)
{
    // Don't add to history when navigating through history
    if (m_navigatingHistory)
        return;

    QString cleanPath = QDir::cleanPath(path);
    if (cleanPath.isEmpty())
        return;

    // Truncate forward history when navigating to new location
    // This happens when user goes back a few times, then navigates normally
    if (m_historyPosition >= 0 && m_historyPosition < m_history.size() - 1) {
        m_history.erase(m_history.begin() + m_historyPosition + 1, m_history.end());
    }

    // Remove ALL previous occurrences of this directory from history
    // This prevents duplicates when navigating back and forth
    m_history.removeAll(cleanPath);

    // Add to history at the end
    m_history.append(cleanPath);
    m_historyPosition = m_history.size() - 1;

    // Enforce size limit
    trimHistoryToLimit();
}

void FilePaneWidget::trimHistoryToLimit()
{
    int maxSize = Config::instance().maxHistorySize();
    if (maxSize <= 0)
        maxSize = 20;  // Fallback

    while (m_history.size() > maxSize) {
        m_history.removeFirst();
        m_historyPosition--;
    }

    // Ensure position is valid
    if (m_historyPosition < 0 && !m_history.isEmpty())
        m_historyPosition = 0;
    if (m_historyPosition >= m_history.size())
        m_historyPosition = m_history.size() - 1;
}

bool FilePaneWidget::canGoBack() const
{
    return m_historyPosition > 0;
}

bool FilePaneWidget::canGoForward() const
{
    return m_historyPosition >= 0 && m_historyPosition < m_history.size() - 1;
}

void FilePaneWidget::goBack()
{
    if (!canGoBack())
        return;

    m_navigatingHistory = true;
    m_historyPosition--;

    QString targetPath = m_history[m_historyPosition];
    m_filePanel->currentPath = targetPath;
    m_filePanel->loadDirectory();
    m_filePanel->selectFirstEntry();

    m_navigatingHistory = false;
}

void FilePaneWidget::goForward()
{
    if (!canGoForward())
        return;

    m_navigatingHistory = true;
    m_historyPosition++;

    QString targetPath = m_history[m_historyPosition];
    m_filePanel->currentPath = targetPath;
    m_filePanel->loadDirectory();
    m_filePanel->selectFirstEntry();

    m_navigatingHistory = false;
}

void FilePaneWidget::showHistoryMenu()
{
    if (m_history.isEmpty())
        return;

    QMenu menu(this);
    QString currentPath = m_filePanel->currentPath;

    // Display history in reverse order (newest first)
    for (int i = m_history.size() - 1; i >= 0; --i) {
        const QString& path = m_history[i];
        QAction* action = menu.addAction(path);
        action->setData(i);

        // Check current directory
        if (i == m_historyPosition) {
            action->setCheckable(true);
            action->setChecked(true);
        }
    }

    // Show menu at button position
    QPoint pos = m_historyButton->mapToGlobal(QPoint(0, m_historyButton->height()));
    QAction* selected = menu.exec(pos);

    if (selected) {
        int index = selected->data().toInt();
        navigateToHistoryIndex(index);
    }
}

void FilePaneWidget::navigateToHistoryIndex(int index)
{
    if (index < 0 || index >= m_history.size())
        return;

    if (index == m_historyPosition)
        return;  // Already at this position

    m_navigatingHistory = true;
    m_historyPosition = index;

    QString targetPath = m_history[m_historyPosition];
    m_filePanel->currentPath = targetPath;
    m_filePanel->loadDirectory();

    m_navigatingHistory = false;
}

bool FilePaneWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_pathEdit && event->type() == QEvent::FocusOut) {
        // Restore path edit to current panel path on focus loss
        restorePathEdit();
    }
    return QWidget::eventFilter(obj, event);
}

void FilePaneWidget::restorePathEdit()
{
    if (m_pathEdit && m_filePanel) {
        m_pathEdit->setText(m_filePanel->currentPath);
    }
}

bool FilePaneWidget::doRestoreAndReturnToPanel(QObject *obj, QKeyEvent *keyEvent)
{
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    // Restore path edit from current panel path
    restorePathEdit();

    // Return focus to panel
    if (m_filePanel) {
        m_filePanel->setFocus();
    }

    return true;
}

bool FilePaneWidget::doNavigateOrRestore(QObject *obj, QKeyEvent *keyEvent)
{
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);

    if (!m_pathEdit || !m_filePanel)
        return true;

    QString newPath = m_pathEdit->text().trimmed();

    // Check if the path is a valid directory
    QDir dir(newPath);
    if (dir.exists()) {
        // Navigate to the directory
        m_filePanel->currentPath = dir.absolutePath();
        m_filePanel->loadDirectory();
        m_filePanel->setFocus();
    } else {
        // Path doesn't exist - restore from current panel path
        restorePathEdit();
        m_filePanel->setFocus();
    }

    return true;
}
