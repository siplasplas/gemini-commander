#pragma once
#include <qce/CodeEditArea.h>
#include <qce/ITextDocument.h>
#include <QPointer>
#include <QUndoCommand>

// Replaces a range (an empty range inserts) on the editor's undo stack, since
// qcodeedit's public editing API exposes only the document.
class ReplaceCommand : public QUndoCommand
{
public:
    ReplaceCommand(qce::CodeEditArea* area, qce::TextCursor start,
                   qce::TextCursor end, QString replacement)
        : m_area(area), m_doc(area->document()), m_start(start), m_end(end),
          m_replacement(std::move(replacement)), m_cursor(area->cursorPosition()),
          m_anchor(m_cursor == area->selectionStart() ? area->selectionEnd() : area->selectionStart())
    { setText(QObject::tr("Replace")); }
    void redo() override {
        if (!m_doc || !m_area) return;
        m_original = m_doc->removeText(m_start, m_end);
        m_newEnd = m_doc->insertText(m_start, m_replacement);
        m_area->setSelection(m_newEnd, m_newEnd);
    }
    void undo() override {
        if (!m_doc || !m_area) return;
        m_doc->removeText(m_start, m_newEnd);
        m_doc->insertText(m_start, m_original);
        m_area->setSelection(m_anchor, m_cursor);
    }
private:
    QPointer<qce::CodeEditArea> m_area;
    QPointer<qce::ITextDocument> m_doc;
    qce::TextCursor m_start, m_end, m_newEnd;
    QString m_replacement, m_original;
    qce::TextCursor m_cursor, m_anchor;
};
