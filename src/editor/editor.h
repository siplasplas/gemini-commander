#ifndef EDITOR_H
#define EDITOR_H

#include <QWidget>
#include <QString>
#include <QDateTime>
#include <QPalette>
#include <memory>
#include <qce/TextCursor.h>
#include <qce/TextAttribute.h>
#include <qce/kate/KateTheme.h>
#include <qce/encoding/Encoding.h>

#include "BaseViewer.h"

namespace qce {
class CodeEdit;
class SimpleTextDocument;
class LineNumberGutter;
class FoldingGutter;
class RulesHighlighter;
class RuleBasedFoldingProvider;
}
class FindBar;
class QFrame;
class QLabel;
class QMenu;
namespace qce::encoding { class EncodingGuard; }

/**
 * @class Editor
 * @brief Text editor tab built on the qcodeedit widget
 *
 * Holds one document: loading, saving, Kate syntax highlighting with folding,
 * the inline search/replace bar and the check for changes on disk.
 */
class Editor : public BaseViewer {
    Q_OBJECT

signals:
    void configFileSaved();
    /// Emitted when the file path changes (e.g. after Save As).
    void filePathChanged();
    void modificationChanged(bool modified);
    /// Encoding, line breaks or language changed (status bar refresh).
    void statusChanged();

public:
    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override;

    /**
     * @brief Loads a file into the editor
     * @return false if the file exists but cannot be read
     * @note A path that does not exist yet gives an empty document saved under that path
     */
    bool loadFile(const QString& path);

    [[nodiscard]] QString baseFileName() const override;
    [[nodiscard]] bool isModified() const override;

    /**
     * @brief Saves document to disk, asking for a name if it has none
     * @return true on successful save
     * @note For config.toml validates TOML syntax first; shows error message on failure
     */
    bool saveFile();
    /// Asks for a new name and saves under it.
    bool saveFileAs();

    bool isChangedOnDisk() const;
    bool reloadFromDisk();

    /// Encoding the file was read in and is saved in (cpg name, e.g. cp1250).
    QString encoding() const;
    /// Reads the file again, decoding it as `encoding`.
    bool reopenWithEncoding(const QString& encoding);
    /// The plausible encodings when detection was not confident on opening
    /// (best first); empty when it was.
    QList<qce::encoding::EncodingOption> uncertainEncodings() const { return m_uncertain.options; }
    /// Opens the encoding choice dialog and reopens the file with the choice.
    void chooseEncoding();
    /// Saves from now on in `encoding`; the text stays as it is.
    void setSaveEncoding(const QString& encoding);
    /// Encoding, BOM and line breaks of the file, for the status bar.
    qce::encoding::FileFormat fileFormat() const;
    /// Natural language of the file, detected in the background after
    /// loading ("Polish"); empty until known.
    QString languageName() const { return m_language.name; }
    void updateFileInfo();

    void setReadOnly(bool readOnly);
    void setWordWrap(bool wrap);
    void setShowWhitespace(bool show);

    /// Inserts text at the cursor (replacing the selection) as one undo step.
    void insertAtCursor(const QString& text);

    /// Expands collapsed folds that hide the given range.
    void revealRange(qce::TextCursor start, qce::TextCursor end);
    void showSearch();
    void showReplace();
    void findNext(bool backwards = false);
    /// Asks for line[:column] and moves the cursor there.
    void goToPosition();

    /// Picks the highlighter again (e.g. after the Kate syntax data was downloaded).
    void reapplyHighlighter();

    qce::CodeEdit* codeEdit() const { return m_edit; }
    qce::SimpleTextDocument* document() const { return m_doc; }

private:
    /// Empty `encoding` detects it.
    bool readFile(const QString& path, bool resetSyntax, const QString& encoding = {});
    bool writeFile(const QString& path);
    void showContextMenu(const QPoint& position);
    void addEncodingMenu(QMenu& menu);
    /// Reopens as `encoding`, asking first when there are unsaved edits.
    void reopenAsking(const QString& encoding);
    void showEncodingNotice();
    void hideEncodingNotice();
    void applyThemePalette();
    qce::TextAttribute themedAttribute(const QString& style, const qce::TextAttribute& fallback) const;
    void applyHighlighterForFile(const QString& path);
    void applyCppHighlighter();
    bool applyKateHighlighter(const QString& xmlPath);
    void clearHighlighter();
    void setPath(const QString& path);

    qce::SimpleTextDocument* m_doc = nullptr;
    qce::CodeEdit* m_edit = nullptr;
    std::unique_ptr<qce::LineNumberGutter> m_lineNumbers;
    std::unique_ptr<qce::FoldingGutter> m_foldGutter;
    std::unique_ptr<qce::RulesHighlighter> m_highlighter;
    std::unique_ptr<qce::RuleBasedFoldingProvider> m_foldProvider;

    enum class SyntaxMode { Automatic, PlainText, Manual };
    SyntaxMode m_syntaxMode = SyntaxMode::Automatic;
    QString m_syntaxFile;
    QString m_themeFile;
    KateTheme m_theme;
    QPalette m_defaultPalette;

    FindBar* m_findBar = nullptr;
    // Shown above the editor when the encoding was detected without confidence.
    QFrame* m_encodingNotice = nullptr;
    QLabel* m_encodingNoticeText = nullptr;
    qce::encoding::EncodingDetection m_uncertain;

    // How the file is stored (encoding, BOM, CRLF, final line break), so that
    // saving writes it back the same way; also keeps typed and pasted text
    // storable in a legacy code page.
    qce::encoding::EncodingGuard* m_encodingGuard = nullptr;
    // The last save was cancelled in the encoding question, not failed.
    bool m_saveCancelled = false;
    qce::encoding::Language m_language;
    int m_languageRequest = 0; // ignores results for an older load

    bool m_wordWrap = true;
    QDateTime m_loadedModified;
    qint64 m_loadedSize = -1;
};

#endif // EDITOR_H
