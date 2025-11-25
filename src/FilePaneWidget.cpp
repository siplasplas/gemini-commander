#include "FilePaneWidget.h"
#include "SearchLineEdit.h"

#include <QItemSelectionModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

FilePaneWidget::FilePaneWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setReadOnly(true);
    QFontMetrics fm(m_pathEdit->font());
    int h = fm.height() + 4;
    m_pathEdit->setFixedHeight(h);

    m_filePanel = new FilePanel(nullptr);
    m_searchEdit = new SearchLineEdit(this);
    m_searchEdit->hide();

    m_statusLabel = new QLabel(this);
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

    connect(m_searchEdit, &SearchLineEdit::nextMatchRequested,
            m_filePanel,  &FilePanel::nextMatch);

    connect(m_searchEdit, &SearchLineEdit::prevMatchRequested,
            m_filePanel,  &FilePanel::prevMatch);

    connect(m_searchEdit, &SearchLineEdit::escapePressed,
            this, [this]() {
                m_searchEdit->clear();
                m_searchEdit->hide();
                if (m_filePanel)
                    m_filePanel->setFocus();
                // opcjonalnie: m_filePanel->cancelSearch();
            });

    connect(m_filePanel, &FilePanel::searchRequested,
        this, [this](const QString& initialText) {
            m_searchEdit->show();
            m_searchEdit->setFocus();
            m_searchEdit->clear();
            if (!initialText.isEmpty())
                m_searchEdit->setText(initialText);
            // poinformuj panel o nowym tekście
            m_filePanel->updateSearch(m_searchEdit->text());
        });

    connect(m_searchEdit, &SearchLineEdit::acceptPressed,
            this, [this]() {
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
