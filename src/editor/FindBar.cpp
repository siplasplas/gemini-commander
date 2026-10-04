#include "FindBar.h"
#include "editor.h"
#include "ReplaceCommand.h"
#include <qce/CodeEdit.h>
#include <qce/CodeEditArea.h>
#include <qce/SimpleTextDocument.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QUndoStack>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QShortcut>
#include <QRegularExpression>
#include <QStyle>
#include <QSignalBlocker>
#include <QEvent>
#include <algorithm>

FindBar::FindBar(Editor* tab) : QWidget(tab), m_tab(tab)
{
    setObjectName("findBar");
    tab->codeEdit()->area()->installEventFilter(this);
    auto* rows = new QVBoxLayout(this);
    rows->setContentsMargins(8, 6, 8, 6);
    rows->setSpacing(4);
    auto* layout = new QHBoxLayout;
    rows->addLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto button = [this](const QString& text, const QString& tip, bool toggle = false) {
        auto* b = new QToolButton(this);
        b->setText(text); b->setToolTip(tip); b->setAutoRaise(true); b->setCheckable(toggle);
        return b;
    };
    m_expand = button(QString(), tr("Show/hide Replace"), true);
    m_expand->setObjectName("replaceToggle");
    m_expand->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    layout->addWidget(m_expand);
    auto* field = new QWidget(this);
    field->setObjectName("findField");
    field->setStyleSheet("#findField { border: 1px solid palette(mid); border-radius: 3px; background: palette(base); }");
    auto* fieldLayout = new QHBoxLayout(field);
    fieldLayout->setContentsMargins(5, 1, 3, 1); fieldLayout->setSpacing(1);
    fieldLayout->addWidget(new QLabel(QString::fromUtf8("⌕"), field));
    m_query = new QLineEdit(field);
    m_query->setObjectName("findQuery");
    m_query->setFrame(false); m_query->setClearButtonEnabled(true);
    m_query->setPlaceholderText(tr("Find"));
    fieldLayout->addWidget(m_query, 1);
    m_wrap = button(QString::fromUtf8("↩"), tr("Wrap around"), true); m_wrap->setChecked(true);
    m_case = button("Cc", tr("Match case"), true);
    m_words = button("W", tr("Whole words"), true);
    m_regex = button(".*", tr("Regular expression"), true);
    for (auto* b : {m_wrap, m_case, m_words, m_regex}) fieldLayout->addWidget(b);
    field->setMinimumWidth(260); field->setMaximumWidth(360);
    layout->addWidget(field, 1);
    m_count = new QLabel(this); m_count->setObjectName("findCount"); m_count->setMinimumWidth(65);
    layout->addWidget(m_count);
    m_previous = button(QString(), tr("Previous match (Shift+F3)"));
    m_previous->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_next = button(QString(), tr("Next match (F3)"));
    m_next->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    layout->addWidget(m_previous); layout->addWidget(m_next);
    m_scope = button(QString::fromUtf8("▽"), tr("Search in selection"), true);
    layout->addWidget(m_scope);
    layout->addStretch();
    auto* close = button(QString(), tr("Close search (Escape)"));
    close->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton)); layout->addWidget(close);
    m_replaceRow = new QWidget(this);
    m_replaceRow->setObjectName("replaceRow");
    auto* replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0); replaceLayout->setSpacing(5);
    replaceLayout->addSpacing(m_expand->sizeHint().width() + layout->spacing());
    auto* replaceField = new QWidget(m_replaceRow);
    replaceField->setObjectName("replaceField");
    replaceField->setStyleSheet("#replaceField { border: 1px solid palette(mid); border-radius: 3px; background: palette(base); }");
    auto* replacementLayout = new QHBoxLayout(replaceField);
    replacementLayout->setContentsMargins(5, 1, 3, 1); replacementLayout->setSpacing(1);
    replacementLayout->addWidget(new QLabel(QString::fromUtf8("⌕"), replaceField));
    m_replacement = new QLineEdit(replaceField);
    m_replacement->setObjectName("replaceText"); m_replacement->setFrame(false);
    m_replacement->setClearButtonEnabled(true); m_replacement->setPlaceholderText(tr("Replace with"));
    replacementLayout->addWidget(m_replacement, 1);
    m_preserveCase = button("Aa", tr("Preserve case"), true);
    replacementLayout->addWidget(m_preserveCase);
    replaceField->setMinimumWidth(260); replaceField->setMaximumWidth(360);
    replaceLayout->addWidget(replaceField, 1);
    m_replace = new QPushButton(tr("Replace"), m_replaceRow); m_replace->setObjectName("replaceCurrent");
    m_replaceAll = new QPushButton(tr("Replace All"), m_replaceRow); m_replaceAll->setObjectName("replaceAll");
    m_exclude = new QPushButton(tr("Exclude"), m_replaceRow); m_exclude->setObjectName("excludeMatch");
    replaceLayout->addWidget(m_replace); replaceLayout->addWidget(m_replaceAll); replaceLayout->addWidget(m_exclude);
    replaceLayout->addStretch(); rows->addWidget(m_replaceRow); m_replaceRow->hide();
    connect(m_expand, &QToolButton::toggled, this, [this](bool expanded) {
        m_replaceRow->setVisible(expanded);
        m_expand->setIcon(style()->standardIcon(expanded ? QStyle::SP_ArrowDown : QStyle::SP_ArrowRight));
    });
    connect(m_replace, &QPushButton::clicked, this, &FindBar::replaceCurrent);
    connect(m_replaceAll, &QPushButton::clicked, this, &FindBar::replaceAll);
    connect(m_exclude, &QPushButton::clicked, this, &FindBar::excludeCurrent);
    connect(m_replacement, &QLineEdit::returnPressed, this, &FindBar::replaceCurrent);
    connect(close, &QToolButton::clicked, this, &FindBar::closeSearch);
    connect(m_previous, &QToolButton::clicked, this, [this]() { findNext(true); });
    connect(m_next, &QToolButton::clicked, this, [this]() { findNext(); });
    connect(m_query, &QLineEdit::textChanged, this, [this]() { m_excluded.clear(); rebuild(); });
    connect(m_query, &QLineEdit::returnPressed, this, [this]() { findNext(); });
    for (auto* b : {m_case, m_words, m_regex}) connect(b, &QToolButton::toggled, this, [this]() { m_excluded.clear(); rebuild(); });
    connect(m_scope, &QToolButton::toggled, this, [this](bool enabled) {
        if (enabled) {
            auto* area = m_tab->codeEdit()->area();
            if (!m_scopeAvailable && !area->hasSelection()) {
                QSignalBlocker block(m_scope); m_scope->setChecked(false); return;
            }
            if (!m_scopeAvailable) {
                m_scopeStart = offsetFor(area->selectionStart());
                m_scopeEnd = offsetFor(area->selectionEnd());
                m_scopeAvailable = true;
            }
        }
        rebuild();
    });
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), tab);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &FindBar::closeSearch);
    // Only while the bar is shown, so Escape otherwise reaches the window (e.g. to close a viewer).
    escape->setEnabled(false);
    m_escape = escape;
    auto* backwards = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return), m_query);
    connect(backwards, &QShortcut::activated, this, [this]() { findNext(true); });
    auto* doc = tab->document();
    connect(doc, &qce::ITextDocument::linesChanged, this, [this]() { rebuild(false); });
    connect(doc, &qce::ITextDocument::linesInserted, this, [this]() { rebuild(false); });
    connect(doc, &qce::ITextDocument::linesRemoved, this, [this]() { rebuild(false); });
    connect(doc, &qce::ITextDocument::documentReset, this, [this]() { rebuild(false); });
    m_snapshot = doc->toPlainText();
    hide();
}
FindBar::~FindBar() = default;

bool FindBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_tab->codeEdit()->area() && event->type() == QEvent::PaletteChange)
        updateHighlights();
    return QWidget::eventFilter(watched, event);
}

void FindBar::showSearch()
{
    auto* area = m_tab->codeEdit()->area();
    const auto selection = area->selectedText();
    const bool wasHidden = isHidden();
    // Capture the user's scope before navigation selects the first result.
    if (isHidden() && !m_scope->isChecked()) {
        m_scopeAvailable = area->hasSelection();
        if (m_scopeAvailable) {
            m_scopeStart = offsetFor(area->selectionStart());
            m_scopeEnd = offsetFor(area->selectionEnd());
        }
    }
    show();
    if (wasHidden && !selection.isEmpty() && !selection.contains('\n')) m_query->setText(selection);
    rebuild(); m_query->setFocus(); m_query->selectAll();
}
void FindBar::showReplace()
{
    showSearch();
    m_expand->setChecked(true);
    m_replacement->setFocus(); m_replacement->selectAll();
}

int FindBar::offsetFor(qce::TextCursor cursor) const
{
    int offset = cursor.column;
    for (int line = 0; line < cursor.line; ++line)
        offset += m_tab->document()->lineAt(line).size() + 1;
    return offset;
}
int FindBar::cursorOffset() const
{
    auto* area = m_tab->codeEdit()->area();
    return offsetFor(area->hasSelection() ? area->selectionStart() : area->cursorPosition());
}
qce::TextCursor FindBar::positionFor(int offset) const
{
    const int line = int(std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), offset)
                         - m_lineStarts.begin()) - 1;
    return line < 0 ? qce::TextCursor{} : qce::TextCursor{line, offset - m_lineStarts[line]};
}
void FindBar::rebuild(bool moveCursor)
{
    if (m_replacing) return;
    m_matches.clear(); m_lineStarts.clear(); m_current = -1;
    const QString text = m_tab->document()->toPlainText();
    updateScope(text);
    int offset = 0;
    for (int i = 0; i < m_tab->document()->lineCount(); ++i) {
        m_lineStarts.append(offset); offset += m_tab->document()->lineAt(i).size() + 1;
    }
    m_query->setToolTip(QString());
    if (!isHidden() && !m_query->text().isEmpty()) {
        QString pattern = m_regex->isChecked() ? m_query->text() : QRegularExpression::escape(m_query->text());
        if (m_words->isChecked()) pattern = "(?<![\\p{L}\\p{N}_])(?:" + pattern + ")(?![\\p{L}\\p{N}_])";
        auto options = QRegularExpression::MultilineOption | QRegularExpression::UseUnicodePropertiesOption;
        if (!m_case->isChecked()) options |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression expression(pattern, options);
        if (!expression.isValid()) {
            m_count->setText(tr("Invalid pattern")); m_query->setToolTip(expression.errorString());
            m_next->setEnabled(false); m_previous->setEnabled(false); updateHighlights(); return;
        }
        m_captureNames = expression.namedCaptureGroups();
        auto results = expression.globalMatch(text);
        while (results.hasNext()) {
            const auto result = results.next();
            const int start = result.capturedStart(), end = result.capturedEnd();
            if (m_scope->isChecked() && (start < m_scopeStart || end > m_scopeEnd)) continue;
            const bool excluded = std::any_of(m_excluded.begin(), m_excluded.end(),
                [start, end](const Match& match) { return match.start == start && match.end == end; });
            if (!excluded) m_matches.append({start, end, result.capturedTexts()});
        }
        if (!m_matches.isEmpty()) {
            const int cursor = cursorOffset();
            auto it = std::lower_bound(m_matches.begin(), m_matches.end(), cursor,
                                      [](const Match& match, int pos) { return match.start < pos; });
            m_current = it == m_matches.end() ? 0 : int(it - m_matches.begin());
        }
    }
    m_next->setEnabled(!m_matches.isEmpty()); m_previous->setEnabled(!m_matches.isEmpty());
    if (m_current >= 0 && moveCursor) activate(m_current);
    else updateHighlights();
}
void FindBar::activate(int index)
{
    m_current = index;
    const auto match = m_matches[index];
    auto* area = m_tab->codeEdit()->area();
    const auto start = positionFor(match.start);
    const auto end = positionFor(match.end);
    m_tab->revealRange(start, end);
    area->setSelection(start, end);
    updateHighlights();
}
void FindBar::findNext(bool backwards)
{
    if (isHidden()) { show(); rebuild(); m_query->setFocus(); return; }
    if (m_matches.isEmpty()) return;
    int next = m_current + (backwards ? -1 : 1);
    if (next < 0 || next >= m_matches.size()) {
        if (!m_wrap->isChecked()) return;
        next = backwards ? m_matches.size() - 1 : 0;
    }
    activate(next);
}
void FindBar::updateHighlights()
{
    auto* area = m_tab->codeEdit()->area();
    QVector<qce::ExtraSelection> selections;
    if (!isHidden()) {
        const bool dark = area->palette().color(QPalette::Base).lightness() < 128;
        const QColor background(dark ? "#685326" : "#ffe39a");
        const QColor activeBackground(dark ? "#8d4d13" : "#ffae52");
        selections.reserve(m_matches.size() + 1);
        for (const auto& match : m_matches)
            selections.append({positionFor(match.start), positionFor(match.end), background, {}});
        if (m_current >= 0) {
            const auto current = m_matches[m_current];
            selections.append({positionFor(current.start), positionFor(current.end), activeBackground, {}});
        }
    }
    area->setExtraSelections(selections);
    m_next->setEnabled(!m_matches.isEmpty()); m_previous->setEnabled(!m_matches.isEmpty());
    const bool canReplace = !m_matches.isEmpty() && m_current >= 0 && !area->readOnly();
    m_replace->setEnabled(canReplace); m_replaceAll->setEnabled(canReplace);
    m_exclude->setEnabled(!m_matches.isEmpty() && m_current >= 0);
    if (m_query->toolTip().isEmpty()) m_count->setText(m_query->text().isEmpty() ? QString() :
        tr("%1/%2").arg(m_current < 0 ? 0 : m_current + 1).arg(m_matches.size()));
}
void FindBar::showEvent(QShowEvent* event)
{
    m_escape->setEnabled(true);
    QWidget::showEvent(event);
}

void FindBar::hideEvent(QHideEvent* event)
{
    m_escape->setEnabled(false);
    QWidget::hideEvent(event);
}

void FindBar::closeSearch()
{
    hide(); m_matches.clear(); m_current = -1; updateHighlights(); m_tab->codeEdit()->area()->setFocus();
}

void FindBar::updateScope(const QString& text)
{
    if (text == m_snapshot) return;
    m_excluded.clear();
    if (m_scopeAvailable) {
        int prefix = 0;
        while (prefix < text.size() && prefix < m_snapshot.size() && text[prefix] == m_snapshot[prefix]) ++prefix;
        int oldEnd = m_snapshot.size(), newEnd = text.size();
        while (oldEnd > prefix && newEnd > prefix && m_snapshot[oldEnd - 1] == text[newEnd - 1]) { --oldEnd; --newEnd; }
        auto adjust = [prefix, oldEnd, newEnd](int pos, bool end) {
            if (pos < prefix || (pos == prefix && !end)) return pos;
            if (pos >= oldEnd) return pos + newEnd - oldEnd;
            return end ? newEnd : prefix;
        };
        m_scopeStart = adjust(m_scopeStart, false); m_scopeEnd = adjust(m_scopeEnd, true);
    }
    m_snapshot = text;
}

QString FindBar::replacementFor(const Match& match) const
{
    const QString input = m_replacement->text();
    QString replacement;
    for (int i = 0; i < input.size(); ++i) {
        const QChar ch = input[i];
        if (m_regex->isChecked() && (ch == '\\' || ch == '$') && i + 1 < input.size()) {
            const QChar next = input[i + 1];
            if (next.isDigit()) {
                int end = i + 1;
                while (end < input.size() && input[end].isDigit()) ++end;
                const int group = input.mid(i + 1, end - i - 1).toInt();
                if (group < match.captures.size()) { replacement += match.captures[group]; i = end - 1; continue; }
            } else if (ch == '$' && next == '{') {
                const int end = input.indexOf('}', i + 2);
                const int group = end < 0 ? -1 : m_captureNames.indexOf(input.mid(i + 2, end - i - 2));
                if (group >= 0 && group < match.captures.size()) { replacement += match.captures[group]; i = end; continue; }
            } else if (ch == '\\') {
                if (next == 'n') { replacement += '\n'; ++i; continue; }
                if (next == 't') { replacement += '\t'; ++i; continue; }
                if (next == 'r') { replacement += '\r'; ++i; continue; }
                if (next == '\\') { replacement += '\\'; ++i; continue; }
            }
        }
        replacement += ch;
    }
    if (m_preserveCase->isChecked()) {
        const QString original = match.captures.value(0);
        if (original.toUpper() != original.toLower()) {
            if (original == original.toUpper()) replacement = replacement.toUpper();
            else if (original == original.toLower()) replacement = replacement.toLower();
            else if (!original.isEmpty() && original[0].isUpper() && original.mid(1) == original.mid(1).toLower()) {
                replacement = replacement.toLower();
                if (!replacement.isEmpty()) replacement[0] = replacement[0].toUpper();
            }
        }
    }
    return replacement;
}

void FindBar::replaceCurrent()
{
    auto* area = m_tab->codeEdit()->area();
    if (m_current < 0 || m_matches.isEmpty() || area->readOnly()) return;
    const Match match = m_matches[m_current];
    m_replacing = true;
    area->undoStack()->push(new ReplaceCommand(area, positionFor(match.start), positionFor(match.end), replacementFor(match)));
    m_replacing = false;
    rebuild(false);
    if (!m_matches.isEmpty()) {
        const int after = offsetFor(area->cursorPosition());
        auto it = std::lower_bound(m_matches.begin(), m_matches.end(), after,
            [](const Match& item, int offset) { return item.start < offset; });
        if (it != m_matches.end()) activate(int(it - m_matches.begin()));
        else if (m_wrap->isChecked()) activate(0);
    }
}

void FindBar::replaceAll()
{
    auto* area = m_tab->codeEdit()->area();
    if (m_matches.isEmpty() || area->readOnly()) return;
    m_replacing = true;
    area->undoStack()->beginMacro(tr("Replace All"));
    // Backwards replacement leaves the coordinates of earlier matches unchanged.
    for (int i = m_matches.size() - 1; i >= 0; --i) {
        const auto& match = m_matches[i];
        area->undoStack()->push(new ReplaceCommand(area, positionFor(match.start), positionFor(match.end), replacementFor(match)));
    }
    area->undoStack()->endMacro();
    m_replacing = false;
    rebuild();
}

void FindBar::excludeCurrent()
{
    if (m_current < 0 || m_matches.isEmpty()) return;
    const int current = m_current;
    m_excluded.append(m_matches[current]); m_matches.removeAt(current);
    if (m_matches.isEmpty()) { m_current = -1; updateHighlights(); }
    else activate(std::min(current, int(m_matches.size()) - 1));
}
