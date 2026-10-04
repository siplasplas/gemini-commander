#pragma once
#include <QWidget>
#include <QVector>
#include <qce/TextCursor.h>
#include <QStringList>
class QLineEdit;
class QLabel;
class QToolButton;
class Editor;
class QPushButton;
class QShortcut;

// Inline search/replace bar shown above an editor (taken from qceditor).
class FindBar : public QWidget
{
    Q_OBJECT
public:
    explicit FindBar(Editor* editor);
    ~FindBar() override;
    void showSearch();
    void showReplace();
    void findNext(bool backwards = false);
private:
    struct Match { int start; int end; QStringList captures; };
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void rebuild(bool moveCursor = true);
    void updateHighlights();
    void activate(int index);
    void closeSearch();
    void replaceCurrent();
    void replaceAll();
    void excludeCurrent();
    QString replacementFor(const Match& match) const;
    void updateScope(const QString& text);
    int cursorOffset() const;
    int offsetFor(qce::TextCursor cursor) const;
    qce::TextCursor positionFor(int offset) const;
    Editor* m_tab;
    QLineEdit* m_query;
    QLineEdit* m_replacement;
    QWidget* m_replaceRow;
    QToolButton* m_expand;
    QToolButton* m_preserveCase;
    QPushButton* m_replace;
    QPushButton* m_replaceAll;
    QPushButton* m_exclude;
    QLabel* m_count;
    QToolButton* m_case;
    QToolButton* m_words;
    QToolButton* m_regex;
    QToolButton* m_wrap;
    QToolButton* m_scope;
    QToolButton* m_previous;
    QToolButton* m_next;
    QShortcut* m_escape = nullptr;
    QVector<Match> m_matches;
    QVector<int> m_lineStarts;
    int m_current = -1;
    int m_scopeStart = 0;
    int m_scopeEnd = 0;
    bool m_scopeAvailable = false;
    bool m_replacing = false;
    QString m_snapshot;
    QStringList m_captureNames;
    QVector<Match> m_excluded;
};
