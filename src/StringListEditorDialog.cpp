#include "StringListEditorDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>

StringListEditorDialog::StringListEditorDialog(const QStringList& items,
                                               int selectedIndex,
                                               const Options& options,
                                               QWidget* parent)
    : QDialog(parent), m_options(options)
{
    setWindowTitle(options.title);
    setMinimumWidth(420);
    setMinimumHeight(320);
    setupUi(options);

    for (const QString& s : items)
        m_list->addItem(s);

    int idx = qBound(0, selectedIndex, m_list->count() - 1);
    if (m_list->count() > 0)
        m_list->setCurrentRow(idx);

    updateButtons();
}

void StringListEditorDialog::setupUi(const Options& options)
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── List (drag-and-drop reordering) ──────────────────────────────
    m_list = new QListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_list, 1);

    // ── Edit row (+ optional Browse) ─────────────────────────────────
    auto* editRow = new QHBoxLayout();
    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("Type or select an item above..."));
    editRow->addWidget(m_edit, 1);

    if (options.allowFileOpen) {
        m_browseBtn = new QPushButton(tr("Browse..."), this);
        editRow->addWidget(m_browseBtn);
    }
    mainLayout->addLayout(editRow);

    // ── Action buttons ────────────────────────────────────────────────
    auto* actionRow = new QHBoxLayout();
    m_addBtn    = new QPushButton(tr("Add new"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    actionRow->addWidget(m_addBtn);
    actionRow->addWidget(m_deleteBtn);
    actionRow->addStretch();
    mainLayout->addLayout(actionRow);

    // ── Dialog buttons ────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_okBtn     = new QPushButton(tr("OK"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_okBtn->setDefault(true);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_okBtn);
    mainLayout->addLayout(btnRow);

    // ── Connections ───────────────────────────────────────────────────
    connect(m_list, &QListWidget::currentRowChanged,
            this, [this](int) { onSelectionChanged(); });

    connect(m_edit, &QLineEdit::textEdited,
            this, &StringListEditorDialog::onEditChanged);

    connect(m_addBtn,    &QPushButton::clicked, this, &StringListEditorDialog::onAddNew);
    connect(m_deleteBtn, &QPushButton::clicked, this, &StringListEditorDialog::onDelete);

    if (m_browseBtn)
        connect(m_browseBtn, &QPushButton::clicked, this, &StringListEditorDialog::onBrowse);

    connect(m_okBtn,     &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

// ── Slots ─────────────────────────────────────────────────────────────────

void StringListEditorDialog::onSelectionChanged()
{
    auto* item = m_list->currentItem();
    m_syncing = true;
    m_edit->setText(item ? item->text() : QString());
    m_syncing = false;
    updateButtons();
}

void StringListEditorDialog::onEditChanged(const QString& text)
{
    if (m_syncing)
        return;
    auto* item = m_list->currentItem();
    if (item)
        item->setText(text);
    updateButtons();
}

void StringListEditorDialog::onAddNew()
{
    // Seed new entry with the directory of the currently edited path
    QString current = m_edit->text().trimmed();
    QString seed;
    if (!current.isEmpty()) {
        QString dir = QFileInfo(current).absolutePath();
        if (!dir.isEmpty() && QFileInfo::exists(dir))
            seed = dir + "/";
    }

    auto* item = new QListWidgetItem(seed);
    m_list->addItem(item);
    m_list->setCurrentItem(item);
    m_syncing = true;
    m_edit->setText(seed);
    m_syncing = false;
    m_edit->setFocus();
    m_edit->setCursorPosition(m_edit->text().length());
    updateButtons();
}

void StringListEditorDialog::onDelete()
{
    int row = m_list->currentRow();
    if (row < 0)
        return;
    delete m_list->takeItem(row);
    // select nearest surviving row
    int newRow = qMin(row, m_list->count() - 1);
    if (newRow >= 0)
        m_list->setCurrentRow(newRow);
    else
        onSelectionChanged();  // clears edit
    updateButtons();
}

void StringListEditorDialog::onBrowse()
{
    QString startDir = QFileInfo(m_edit->text().trimmed()).absolutePath();
    if (startDir.isEmpty() || !QFileInfo::exists(startDir))
        startDir = QString();

    QString path = QFileDialog::getOpenFileName(
        this,
        m_options.fileDialogTitle,
        startDir,
        m_options.fileFilter);
    if (path.isEmpty())
        return;

    auto* item = m_list->currentItem();
    if (!item) {
        item = new QListWidgetItem(QString());
        m_list->addItem(item);
        m_list->setCurrentItem(item);
    }
    item->setText(path);
    m_syncing = true;
    m_edit->setText(path);
    m_syncing = false;
    updateButtons();
}

void StringListEditorDialog::updateButtons()
{
    bool hasSelection = m_list->currentRow() >= 0;
    m_deleteBtn->setEnabled(hasSelection);
}

// ── Getters ───────────────────────────────────────────────────────────────

QStringList StringListEditorDialog::items() const
{
    QStringList result;
    for (int i = 0; i < m_list->count(); ++i) {
        QString s = m_list->item(i)->text().trimmed();
        if (!s.isEmpty())
            result << s;
    }
    return result;
}

int StringListEditorDialog::selectedIndex() const
{
    return m_list->currentRow();
}