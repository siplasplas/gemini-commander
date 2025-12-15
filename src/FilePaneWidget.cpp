#include "FilePaneWidget.h"
#include "SearchEdit.h"

#include <QItemSelectionModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "keys/ObjectRegistry.h"

FilePaneWidget::FilePaneWidget(Side side, QWidget* parent)
    : m_side(side), QWidget(parent)
{
    ObjectRegistry::add(this, "PaneComposite");
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


    // initial
    setCurrentPath(m_filePanel->currentPath);
    updateStatusLabel();
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
    auto* view = m_filePanel;
    auto* model = view->model;
    auto* selModel = view->selectionModel();
    if (!model || !selModel) {
        m_statusLabel->clear();
        return;
    }

    const QModelIndexList rows = selModel->selectedRows(COLUMN_NAME);

    qint64 totalBytes = 0;
    int fileCount = 0;
    int dirCount = 0;

    for (const QModelIndex& idx : rows) {
        int row = idx.row();
        const QString type = model->item(row, COLUMN_EXT)->text();
        const QString sizeStr = model->item(row, COLUMN_SIZE)->text();

        if (type == "<DIR>" || type == "Directory") {
            ++dirCount;
        } else {
            ++fileCount;
            bool ok = false;
            qint64 sz = sizeStr.toLongLong(&ok);
            if (ok)
                totalBytes += sz;
        }
    }

    const int totalRows = model->rowCount();
    QString text = QString("Selected %1 bytes, %2 file(s), %3 dir(s), %4 of %5 items")
            .arg(totalBytes)
            .arg(fileCount)
            .arg(dirCount)
            .arg(rows.size())
            .arg(totalRows);
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

