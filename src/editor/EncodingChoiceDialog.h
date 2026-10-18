#pragma once

#include <QByteArray>
#include <QDialog>
#include <QList>
#include <qce/encoding/Encoding.h>

class QComboBox;
class QListWidget;
class QTableWidget;

/**
 * @brief Choice of a file's encoding when detection is not sure.
 *
 * Lists the plausible encodings with their language and probability, and
 * previews the lines where they decode the file differently, in the order
 * qcodeedit gives them. "Other encoding" adds any encoding to the comparison.
 */
class EncodingChoiceDialog : public QDialog {
    Q_OBJECT
public:
    EncodingChoiceDialog(const QByteArray& bytes, const QList<qce::encoding::EncodingOption>& options,
                         const QString& current, const QFont& textFont, QWidget* parent = nullptr);

    /// The chosen encoding; empty when the dialog was cancelled.
    QString chosenEncoding() const;

private:
    void fillOptions();
    void fillPreview();
    void addOtherEncoding(const QString& encoding);
    void highlightChosenColumn();

    QByteArray m_bytes;
    QList<qce::encoding::EncodingOption> m_options;
    QListWidget* m_list = nullptr;
    QTableWidget* m_preview = nullptr;
    QComboBox* m_other = nullptr;
};
