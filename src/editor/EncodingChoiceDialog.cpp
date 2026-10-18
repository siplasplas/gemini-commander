#include "EncodingChoiceDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString optionText(const qce::encoding::EncodingOption& option)
{
    QString text = option.encoding;
    const QString language = qce::encoding::languageName(option.language);
    if (!language.isEmpty())
        text += QStringLiteral(" — ") + language;
    if (option.confidence > 0)
        text += QStringLiteral(" — %1 %").arg(qRound(option.confidence * 100));
    return text;
}

// 1-based line of a byte offset.
int lineAt(const QByteArray& bytes, qsizetype offset)
{
    return int(bytes.left(offset).count('\n')) + 1;
}

} // namespace

EncodingChoiceDialog::EncodingChoiceDialog(const QByteArray& bytes,
                                           const QList<qce::encoding::EncodingOption>& options,
                                           const QString& current, const QFont& textFont, QWidget* parent)
    : QDialog(parent), m_bytes(bytes), m_options(options)
{
    setWindowTitle(tr("Choose Encoding"));
    resize(760, 420);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Detection is not sure which encoding this file uses. "
                                "Choose one; the lines below show where they differ."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_list = new QListWidget(this);
    m_list->setMaximumHeight(fontMetrics().height() * 8);
    layout->addWidget(m_list);

    auto* otherRow = new QHBoxLayout;
    otherRow->addWidget(new QLabel(tr("Other encoding:"), this));
    m_other = new QComboBox(this);
    m_other->addItem(QString());
    m_other->addItems(qce::encoding::availableEncodings());
    otherRow->addWidget(m_other, 1);
    layout->addLayout(otherRow);

    m_preview = new QTableWidget(this);
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->setSelectionMode(QAbstractItemView::NoSelection);
    m_preview->setFont(textFont);
    m_preview->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_preview, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    fillOptions();
    for (int i = 0; i < m_options.size(); ++i)
        if (m_options[i].encoding == current)
            m_list->setCurrentRow(i);
    if (m_list->currentRow() < 0)
        m_list->setCurrentRow(0);
    fillPreview();

    connect(m_list, &QListWidget::currentRowChanged, this, &EncodingChoiceDialog::highlightChosenColumn);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(m_other, &QComboBox::textActivated, this, &EncodingChoiceDialog::addOtherEncoding);
}

QString EncodingChoiceDialog::chosenEncoding() const
{
    if (result() != QDialog::Accepted)
        return {};
    const int row = m_list->currentRow();
    return row >= 0 && row < m_options.size() ? m_options[row].encoding : QString();
}

void EncodingChoiceDialog::fillOptions()
{
    const int row = m_list->currentRow();
    m_list->clear();
    for (const auto& option : m_options)
        m_list->addItem(optionText(option));
    if (row >= 0)
        m_list->setCurrentRow(row);
}

void EncodingChoiceDialog::fillPreview()
{
    // qcodeedit's order (lines with most differing bytes first) is kept: the
    // lines that speak most against the best option are often unrepresentative.
    const auto lines = qce::encoding::previewEncodings(m_bytes, m_options);
    m_preview->clear();
    m_preview->setColumnCount(int(m_options.size()));
    m_preview->setRowCount(int(lines.size()));
    QStringList headers;
    for (const auto& option : m_options)
        headers << option.encoding;
    m_preview->setHorizontalHeaderLabels(headers);
    QStringList rows;
    for (int row = 0; row < lines.size(); ++row) {
        rows << tr("Line %1").arg(lineAt(m_bytes, lines[row].byteOffset));
        for (int column = 0; column < lines[row].variants.size() && column < m_options.size(); ++column)
            m_preview->setItem(row, column, new QTableWidgetItem(lines[row].variants[column]));
    }
    m_preview->setVerticalHeaderLabels(rows);
    highlightChosenColumn();
}

void EncodingChoiceDialog::addOtherEncoding(const QString& encoding)
{
    if (encoding.isEmpty())
        return;
    for (int i = 0; i < m_options.size(); ++i) {
        if (m_options[i].encoding == encoding) {
            m_list->setCurrentRow(i);
            return;
        }
    }
    m_options.append({encoding, QString(), 0});
    fillOptions();
    m_list->setCurrentRow(int(m_options.size()) - 1);
    fillPreview();
}

void EncodingChoiceDialog::highlightChosenColumn()
{
    const int chosen = m_list->currentRow();
    const QColor highlight = palette().color(QPalette::Highlight).lighter(170);
    for (int row = 0; row < m_preview->rowCount(); ++row)
        for (int column = 0; column < m_preview->columnCount(); ++column)
            if (QTableWidgetItem* item = m_preview->item(row, column))
                item->setBackground(column == chosen ? QBrush(highlight) : QBrush());
}
