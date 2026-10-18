#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;

/**
 * @brief Choice of one or two spelling dictionaries among all hunspell has installed.
 *
 * E.g. English + Polish for English comments in a Polish file; a hint says how
 * to install another language.
 */
class SpellLanguageDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpellLanguageDialog(const QStringList& chosen, QWidget* parent = nullptr);

    /// The checked dictionaries (names such as "pl_PL"), at most two.
    QStringList chosenDictionaries() const { return m_order; }

    static constexpr int MaxLanguages = 2;

private:
    QListWidget* m_list = nullptr;
    QStringList m_order; // checked dictionaries, oldest first
};
