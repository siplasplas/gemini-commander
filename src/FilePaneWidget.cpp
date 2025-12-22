#include "FilePaneWidget.h"
#include "SearchEdit.h"
#include "SizeFormat.h"

#include <QItemSelectionModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "keys/ObjectRegistry.h"

FilePaneWidget::FilePaneWidget(Side side, QWidget* parent)
    : m_side(side), QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setReadOnly(true);
    QFontMetrics fm(m_pathEdit->font());
    int h = fm.height() + 4;
    m_pathEdit->setFixedHeight(h);

    m_filePanel = new FilePanel(side, nullptr);
    ObjectRegistry::add(m_filePanel, "Panel");
    m_searchEdit = new SearchEdit(this);
    ObjectRegistry::add(m_searchEdit, "SearchEdit");
    m_searchEdit->hide();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("QLabel { background-color: #f4f4f4; padding: 2px 6px; }");
    m_statusLabel->setText(QString());

    layout->addWidget(m_pathEdit);
    layout->addWidget(m_filePanel);
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_statusLabel);

    setLayout(layout);

    connect(m_filePanel, &FilePanel::directoryChanged,
            this, &FilePaneWidget::onDirectoryChanged);
    connect(m_filePanel, &FilePanel::selectionChanged,
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

    // Format sizes using SizeFormat
    QString selectedSizeStr = QString::fromStdString(SizeFormat::formatSize(selectedBytes, false));
    QString totalSizeStr = QString::fromStdString(SizeFormat::formatSize(totalBytes, false));

    // Format: "59 k / 1.31 M in 1 / 2 file(s), 0 / 1 dir(s)"
    QString text = QString("%1 / %2 in %3 / %4 file(s), %5 / %6 dir(s)")
            .arg(selectedSizeStr)
            .arg(totalSizeStr)
            .arg(selectedFileCount)
            .arg(totalFileCount)
            .arg(selectedDirCount)
            .arg(totalDirCount);
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

