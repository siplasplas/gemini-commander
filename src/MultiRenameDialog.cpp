#include "MultiRenameDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSet>

MultiRenameDialog::MultiRenameDialog(const QStringList& fileNames,
                                     const QStringList& existingNames,
                                     QWidget* parent)
    : QDialog(parent)
    , m_originalNames(fileNames)
    , m_existingNames(existingNames)
{
    setWindowTitle(tr("Multi-Rename Tool"));
    setMinimumSize(600, 400);
    setupUi();
    updatePreview();
}

void MultiRenameDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Find/Replace section
    auto* formLayout = new QFormLayout();

    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText(tr("Text to find (e.g. -latest-)"));
    connect(m_findEdit, &QLineEdit::textChanged, this, &MultiRenameDialog::updatePreview);
    formLayout->addRow(tr("Find:"), m_findEdit);

    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(tr("Replace with (e.g. -20250601-)"));
    connect(m_replaceEdit, &QLineEdit::textChanged, this, &MultiRenameDialog::updatePreview);
    formLayout->addRow(tr("Replace:"), m_replaceEdit);

    mainLayout->addLayout(formLayout);

    // Preview table
    m_tableView = new QTableView(this);
    m_model = new QStandardItemModel(0, 2, this);
    m_model->setHorizontalHeaderLabels({tr("Old Name"), tr("New Name")});
    m_tableView->setModel(m_model);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableView->verticalHeader()->setVisible(false);

    mainLayout->addWidget(m_tableView, 1);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &MultiRenameDialog::onAccept);
    buttonLayout->addWidget(m_okButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

QString MultiRenameDialog::applyRename(const QString& fileName) const
{
    QString findText = m_findEdit->text();
    QString replaceText = m_replaceEdit->text();

    if (findText.isEmpty()) {
        return fileName;
    }

    return QString(fileName).replace(findText, replaceText);
}

void MultiRenameDialog::updatePreview()
{
    m_model->removeRows(0, m_model->rowCount());

    for (const QString& oldName : m_originalNames) {
        QString newName = applyRename(oldName);

        QList<QStandardItem*> row;
        auto* oldItem = new QStandardItem(oldName);
        auto* newItem = new QStandardItem(newName);

        // Highlight if name changed
        if (oldName != newName) {
            newItem->setForeground(QBrush(Qt::darkGreen));
        }

        row << oldItem << newItem;
        m_model->appendRow(row);
    }
}

bool MultiRenameDialog::checkConflicts()
{
    QSet<QString> newNames;
    QSet<QString> unchangedNames;

    // Build set of names that won't change (for conflict detection)
    for (const QString& existingName : m_existingNames) {
        if (!m_originalNames.contains(existingName)) {
            unchangedNames.insert(existingName);
        }
    }

    // Check for conflicts
    for (const QString& oldName : m_originalNames) {
        QString newName = applyRename(oldName);

        // Skip if name doesn't change
        if (oldName == newName) {
            continue;
        }

        // Check if two different files would get the same new name
        if (newNames.contains(newName)) {
            QMessageBox::warning(this, tr("Conflict"),
                tr("Multiple files would be renamed to '%1'.\n"
                   "Please adjust your rename pattern.").arg(newName));
            return false;
        }
        newNames.insert(newName);

        // Check if new name conflicts with existing file not being renamed
        if (unchangedNames.contains(newName)) {
            QMessageBox::warning(this, tr("Conflict"),
                tr("'%1' would conflict with an existing file '%2'.\n"
                   "Please adjust your rename pattern.").arg(oldName, newName));
            return false;
        }
    }

    return true;
}

void MultiRenameDialog::onAccept()
{
    // Check if there's anything to rename
    bool hasChanges = false;
    for (const QString& oldName : m_originalNames) {
        if (oldName != applyRename(oldName)) {
            hasChanges = true;
            break;
        }
    }

    if (!hasChanges) {
        QMessageBox::information(this, tr("No Changes"),
            tr("No files will be renamed with the current pattern."));
        return;
    }

    if (checkConflicts()) {
        accept();
    }
}

QList<QPair<QString, QString>> MultiRenameDialog::getRenameOperations() const
{
    QList<QPair<QString, QString>> result;

    for (const QString& oldName : m_originalNames) {
        QString newName = applyRename(oldName);
        if (oldName != newName) {
            result.append({oldName, newName});
        }
    }

    return result;
}