#include "FilePaneWidget.h"
#include <QVBoxLayout>
#include <QItemSelectionModel>
#include <QStandardItemModel>

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

    m_filePanel = new FilePanel(nullptr); // jeśli FilePanel wymaga QSplitter, trzeba lekko przerobić ctor
    // U Ciebie FilePanel dziedziczy z QTableView i przyjmował QSplitter*.
    // Teraz najlepiej dodać alternatywny konstruktor bez splittera,
    // albo przekazywać splitter z zewnątrz. Przyjmijmy, że masz już wersję bez splittera.

    m_statusLabel = new QLabel(this);
    m_statusLabel->setText(QString());

    layout->addWidget(m_pathEdit);
    layout->addWidget(m_filePanel);
    layout->addWidget(m_statusLabel);

    setLayout(layout);

    // sygnały z FilePanel – musimy je mieć w FilePanel
    // np. directoryChanged(path), selectionChanged()
    connect(m_filePanel, &FilePanel::directoryChanged,
            this, &FilePaneWidget::onDirectoryChanged);
    connect(m_filePanel, &FilePanel::selectionChanged,
            this, &FilePaneWidget::onSelectionChanged);

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
        const QString type = model->item(row, COLUMN_TYPE)->text();
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
