#include "SpellLanguageDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <qce/spell/SpellChecker.h>

SpellLanguageDialog::SpellLanguageDialog(const QStringList& chosen, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Spelling Languages"));
    resize(420, 460);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Check one or two languages; a word is right in either "
                                "(e.g. English comments in a Polish file)."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_list = new QListWidget(this);
    const auto dictionaries = qce::spell::installedDictionaries();
    for (const auto& dictionary : dictionaries) {
        auto* item = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(dictionary.displayName, dictionary.name),
                                         m_list);
        item->setData(Qt::UserRole, dictionary.name);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        const bool checked = chosen.contains(dictionary.name);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        if (checked)
            m_order << dictionary.name;
    }
    if (dictionaries.isEmpty())
        m_list->addItem(tr("No hunspell dictionaries are installed."));
    layout->addWidget(m_list, 1);

    auto* hint = new QLabel(tr("Other languages are installed as hunspell dictionaries, e.g. "
                               "<tt>sudo apt install %1</tt> for German, then reopen this window.")
                                .arg(qce::spell::packageHint(QStringLiteral("de"))), this);
    hint->setWordWrap(true);
    hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(hint);

    // At most two: checking a third unchecks the one checked first.
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        const QString name = item->data(Qt::UserRole).toString();
        if (name.isEmpty())
            return;
        m_order.removeAll(name);
        if (item->checkState() == Qt::Checked)
            m_order << name;
        while (m_order.size() > MaxLanguages) {
            const QString dropped = m_order.takeFirst();
            for (int i = 0; i < m_list->count(); ++i)
                if (m_list->item(i)->data(Qt::UserRole).toString() == dropped) {
                    const QSignalBlocker block(m_list);
                    m_list->item(i)->setCheckState(Qt::Unchecked);
                }
        }
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
