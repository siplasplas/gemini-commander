#include "editor.h"
#include "FindBar.h"
#include "ReplaceCommand.h"
#include "SyntaxData.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>

#include <qce/CodeEdit.h>
#include <qce/CodeEditArea.h>
#include <qce/FoldState.h>
#include <qce/RuleBasedFoldingProvider.h>
#include <qce/RulesHighlighter.h>
#include <qce/SimpleTextDocument.h>
#include <qce/margins/FoldingGutter.h>
#include <qce/margins/LineNumberGutter.h>
#include <qce/kate/KatePaths.h>
#include <qce/kate/KateXmlReader.h>
#include <qxfiledialog.h>

#include "../Config.h"

// Used only when no Kate definition matches (e.g. data not downloaded yet).
static bool isCppExtension(const QString& ext)
{
    static const QStringList cpp = {"c","cpp","cxx","cc","h","hpp","hxx"};
    return cpp.contains(ext.toLower());
}

Editor::Editor(QWidget *parent) :
    BaseViewer(parent)
{
    m_doc = new qce::SimpleTextDocument(this);
    m_edit = new qce::CodeEdit(this);
    m_edit->setDocument(m_doc);
    m_defaultPalette = m_edit->area()->palette();
    m_edit->area()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_edit->area(), &QWidget::customContextMenuRequested,
            this, &Editor::showContextMenu);

    m_lineNumbers = std::make_unique<qce::LineNumberGutter>(m_doc);
    m_lineNumbers->setFont(m_edit->area()->font());
    m_edit->addLeftMargin(m_lineNumbers.get());

    // FoldingGutter is added once and stays; it reads foldState() which is
    // updated whenever a new FoldingProvider is set.
    m_foldGutter = std::make_unique<qce::FoldingGutter>(
        &m_edit->area()->foldState(),
        [this](int line) { m_edit->area()->toggleFoldAt(line); });
    m_edit->addLeftMargin(m_foldGutter.get());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_findBar = new FindBar(this);
    layout->addWidget(m_findBar);
    layout->addWidget(m_edit);
    setFocusProxy(m_edit->area());

    // The undo stack's clean state is the document's saved state, so undoing
    // back to it clears the modification flag.
    connect(m_edit->area()->undoStack(), &QUndoStack::cleanChanged,
            this, [this](bool clean) { emit modificationChanged(!clean); });

    m_edit->area()->setWordWrap(m_wordWrap);

    m_theme = KateTheme::loadDefault();
    applyThemePalette();
}

Editor::~Editor()
{
    // The area refers to highlighter, folding provider and margins; detach them
    // before the unique_ptrs below release them.
    m_edit->area()->setHighlighter(nullptr);
    m_edit->area()->setFoldingProvider(nullptr);
    m_edit->removeLeftMargin(m_foldGutter.get());
    m_edit->removeLeftMargin(m_lineNumbers.get());
}

void Editor::setPath(const QString& path)
{
    const bool changed = path != m_filePath;
    m_filePath = path;
    setObjectName(m_filePath);
    if (changed)
        emit filePathChanged();
}

bool Editor::loadFile(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        // A name that does not exist yet (e.g. Shift+F4): empty document saved there.
        m_doc->setText(QString());
        m_encoding = QStringConverter::Utf8;
        m_bom = false;
        m_crlf = false;
        m_finalNewline = true;
        m_edit->area()->undoStack()->clear();
        setPath(path);
        updateFileInfo();
        reapplyHighlighter();
        return true;
    }
    return readFile(path, true);
}

/**
 * @brief Extracts filename from path
 * @return Short filename or "Untitled"
 */
QString Editor::baseFileName() const {
    if (m_filePath.isEmpty())
        return tr("Untitled");
    return QFileInfo(m_filePath).fileName();
}

bool Editor::isModified() const {
    return !m_edit->area()->undoStack()->isClean();
}

bool Editor::readFile(const QString& path, bool resetSyntax)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QFileInfo stamp(f);
    const QByteArray bytes = f.readAll();
    if (f.error() != QFileDevice::NoError)
        return false;

    // UTF-8 unless the bytes are not valid UTF-8; Latin-1 then keeps every
    // byte, so saving does not destroy the file.
    m_bom = bytes.startsWith("\xEF\xBB\xBF");
    QStringDecoder utf8(QStringConverter::Utf8);
    QString text = utf8.decode(bytes);
    m_encoding = QStringConverter::Utf8;
    if (utf8.hasError()) {
        QStringDecoder latin1(QStringConverter::Latin1);
        text = latin1.decode(bytes);
        m_encoding = QStringConverter::Latin1;
        m_bom = false;
    }
    m_crlf = text.contains(QLatin1String("\r\n"));
    if (m_crlf)
        text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    m_finalNewline = text.endsWith(QLatin1Char('\n'));

    m_doc->setText(text);
    m_edit->area()->undoStack()->clear();
    setPath(path);
    m_loadedModified = stamp.lastModified();
    m_loadedSize = stamp.size();

    if (resetSyntax) {
        m_syntaxMode = SyntaxMode::Automatic;
        m_syntaxFile.clear();
    }
    reapplyHighlighter();
    return true;
}

bool Editor::writeFile(const QString& path)
{
    QString text = m_doc->toPlainText();
    if (m_finalNewline)
        text += QLatin1Char('\n');
    if (m_crlf)
        text.replace(QLatin1Char('\n'), QLatin1String("\r\n"));
    QStringEncoder encoder(m_encoding, m_bom ? QStringConverter::Flag::WriteBom
                                             : QStringConverter::Flag::Default);
    const QByteArray bytes = encoder.encode(text);

    QSaveFile f(path);
    f.setDirectWriteFallback(true);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(bytes) != bytes.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

/**
 * @brief Saves document to its path
 * @return true on successful save
 *
 * @details For config.toml files, validates TOML syntax before saving.
 * @warning Shows QMessageBox on failure
 */
bool Editor::saveFile() {
    // An untitled document has no path to save to: ask for a name.
    // saveFileAs() returns false when the dialog is cancelled, so callers
    // (e.g. "Save and Close" in the tab-limit dialog) keep the tab open.
    if (m_filePath.isEmpty())
        return saveFileAs();

    // If this is the config file, validate TOML before saving
    if (Config::instance().isConfigFile(m_filePath)) {
        QString errorMsg;
        if (!Config::validateToml(m_doc->toPlainText(), errorMsg)) {
            QMessageBox::critical(this->parentWidget(),
                                  tr("Invalid Configuration"),
                                  tr("Cannot save: TOML syntax error.\n\n%1").arg(errorMsg));
            return false;
        }
    }

    if (!writeFile(m_filePath)) {
        qWarning() << "Failed to save document:" << m_filePath;
        QMessageBox::warning(this->parentWidget(), // Show error relative to parent window
                             tr("Save Error"),
                             tr("Failed to save file:\n%1").arg(m_filePath));
        return false;
    }

    // If config file was saved, emit signal to notify MainWindow
    if (Config::instance().isConfigFile(m_filePath)) {
        emit configFileSaved();
    }

    m_edit->area()->undoStack()->setClean();
    updateFileInfo();
    return true;
}

bool Editor::saveFileAs()
{
    const QString path = QxFileDialog::getSaveFileName(this, tr("Save As"), m_filePath);
    if (path.isEmpty())
        return false;
    if (!writeFile(path)) {
        QMessageBox::warning(this->parentWidget(), tr("Save Error"),
                             tr("Failed to save file:\n%1").arg(path));
        return false;
    }
    const bool renamed = path != m_filePath;
    setPath(path);
    m_edit->area()->undoStack()->setClean();
    updateFileInfo();
    if (renamed)
        reapplyHighlighter();
    return true;
}

void Editor::updateFileInfo()
{
    if (m_filePath.isEmpty()) {
        m_loadedModified = QDateTime();
        m_loadedSize = -1;
        return;
    }
    QFileInfo fi(m_filePath);
    m_loadedModified = fi.lastModified();
    m_loadedSize = fi.size();
}

bool Editor::isChangedOnDisk() const
{
    if (m_filePath.isEmpty())
        return false;
    QFileInfo fi(m_filePath);
    if (!fi.exists())
        return false;
    return fi.lastModified() != m_loadedModified || fi.size() != m_loadedSize;
}

bool Editor::reloadFromDisk()
{
    if (m_filePath.isEmpty() || isModified())
        return false;
    auto* area = m_edit->area();
    const auto cursor = area->cursorPosition();
    const auto anchor = cursor == area->selectionStart() ? area->selectionEnd() : area->selectionStart();
    if (!readFile(m_filePath, false))
        return false;
    // The component clamps both endpoints if lines or columns no longer exist.
    area->setSelection(anchor, cursor);
    const auto clampedCursor = area->cursorPosition();
    const auto clampedAnchor = clampedCursor == area->selectionStart() ? area->selectionEnd() : area->selectionStart();
    revealRange(area->selectionStart(), area->selectionEnd());
    area->setSelection(clampedAnchor, clampedCursor);
    return true;
}

void Editor::setReadOnly(bool readOnly)
{
    m_edit->area()->setReadOnly(readOnly);
}

void Editor::setWordWrap(bool wrap)
{
    m_wordWrap = wrap;
    m_edit->area()->setWordWrap(wrap);
}

void Editor::setShowWhitespace(bool show)
{
    m_edit->area()->setShowWhitespace(show);
}

void Editor::insertAtCursor(const QString& text)
{
    auto* area = m_edit->area();
    if (area->readOnly())
        return;
    area->undoStack()->push(new ReplaceCommand(area, area->selectionStart(),
                                               area->selectionEnd(), text));
}

void Editor::revealRange(qce::TextCursor start, qce::TextCursor end)
{
    auto* area = m_edit->area();
    auto& folds = area->foldState();
    int refreshLine = -1;
    const auto& regions = folds.regions();
    for (int i = 0; i < regions.size(); ++i) {
        if (!folds.isCollapsed(i)) continue;
        const auto& region = regions[i];
        const qce::TextCursor hiddenStart{region.startLine, region.startColumn};
        // Folding hides the header suffix and every following line through
        // endLine, including text after the closing marker on that line.
        const bool overlaps = start.line <= region.endLine
            && (start == end ? hiddenStart <= start : hiddenStart < end);
        if (!overlaps) continue;
        folds.setCollapsed(i, false);
        refreshLine = region.startLine;
    }
    if (refreshLine >= 0) {
        // The public toggle refreshes layout, scrollbars and the viewport.
        // It addresses the first region on a line; invert that region first
        // so the toggle restores its desired state, including same-line folds.
        const int first = folds.regionStartingAt(refreshLine);
        folds.setCollapsed(first, !folds.isCollapsed(first));
        area->toggleFoldAt(refreshLine);
    }
}

void Editor::showSearch()
{
    m_findBar->showSearch();
}

void Editor::showReplace()
{
    m_findBar->showReplace();
}

void Editor::findNext(bool backwards)
{
    m_findBar->findNext(backwards);
}

void Editor::goToPosition()
{
    auto* area = m_edit->area();
    const auto cursor = area->cursorPosition();
    const int lineCount = qMax(1, m_doc->lineCount());
    QInputDialog dialog(this);
    dialog.setWindowTitle(tr("Go to Line/Column"));
    dialog.setLabelText(tr("Line[:column] (1–%1):").arg(lineCount));
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setTextValue(QString("%1:%2").arg(cursor.line + 1).arg(cursor.column + 1));

    auto position = [this, lineCount](const QString& text) -> std::optional<qce::TextCursor> {
        static const QRegularExpression pattern(QStringLiteral("^\\s*([1-9][0-9]*)\\s*(?::\\s*([1-9][0-9]*)\\s*)?$"));
        const auto match = pattern.match(text);
        if (!match.hasMatch()) return std::nullopt;
        bool lineOk = false, columnOk = true;
        const int line = match.captured(1).toInt(&lineOk);
        const int column = match.captured(2).isEmpty() ? 1 : match.captured(2).toInt(&columnOk);
        if (!lineOk || !columnOk || line > lineCount
            || column > m_doc->lineAt(line - 1).size() + 1)
            return std::nullopt;
        return qce::TextCursor{line - 1, column - 1};
    };
    auto validate = [&dialog, position]() {
        if (auto* buttons = dialog.findChild<QDialogButtonBox*>())
            buttons->button(QDialogButtonBox::Ok)->setEnabled(position(dialog.textValue()).has_value());
    };
    connect(&dialog, &QInputDialog::textValueChanged, &dialog, validate);
    QTimer::singleShot(0, &dialog, [&dialog, validate]() {
        if (auto* input = dialog.findChild<QLineEdit*>()) input->selectAll();
        validate();
    });
    if (dialog.exec() != QDialog::Accepted) return;
    if (const auto target = position(dialog.textValue())) {
        revealRange(*target, *target);
        area->setCursorPosition(*target);
        area->setFocus();
    }
}

void Editor::showContextMenu(const QPoint& position)
{
    auto* area = m_edit->area();
    QMenu menu(this);

    auto* undo = menu.addAction(tr("&Undo"), area, &qce::CodeEditArea::undo);
    undo->setEnabled(area->canUndo() && !area->readOnly());
    auto* redo = menu.addAction(tr("&Redo"), area, &qce::CodeEditArea::redo);
    redo->setEnabled(area->canRedo() && !area->readOnly());
    menu.addSeparator();
    auto* cut = menu.addAction(tr("Cu&t"), this, [this, area]() {
        QApplication::clipboard()->setText(area->selectedText());
        insertAtCursor(QString());
    });
    cut->setEnabled(area->hasSelection() && !area->readOnly());
    auto* copy = menu.addAction(tr("&Copy"), this, [area]() {
        QApplication::clipboard()->setText(area->selectedText());
    });
    copy->setEnabled(area->hasSelection());
    const QString clipboardText = QApplication::clipboard()->text();
    auto* paste = menu.addAction(tr("&Paste"), this, [this, clipboardText]() {
        insertAtCursor(clipboardText);
    });
    paste->setEnabled(!clipboardText.isEmpty() && !area->readOnly());
    menu.addAction(tr("Select &All"), area, &qce::CodeEditArea::selectAll);
    menu.addSeparator();

    auto* syntaxMenu = menu.addMenu(tr("Syntax"));
    auto* choices = new QActionGroup(&menu);
    choices->setExclusive(true);

    auto addChoice = [choices](QMenu* parent, const QString& text, bool checked) {
        auto* action = parent->addAction(text);
        action->setCheckable(true);
        action->setChecked(checked);
        choices->addAction(action);
        return action;
    };

    auto* automatic = addChoice(syntaxMenu, tr("Automatic"),
                                m_syntaxMode == SyntaxMode::Automatic);
    connect(automatic, &QAction::triggered, this, [this]() {
        m_syntaxMode = SyntaxMode::Automatic;
        m_syntaxFile.clear();
        reapplyHighlighter();
    });
    auto* plainText = addChoice(syntaxMenu, tr("Plain Text"),
                                m_syntaxMode == SyntaxMode::PlainText);
    connect(plainText, &QAction::triggered, this, [this]() {
        m_syntaxMode = SyntaxMode::PlainText;
        m_syntaxFile.clear();
        reapplyHighlighter();
    });
    syntaxMenu->addSeparator();

    QMap<QString, QList<qce::kate::LanguageEntry>> sections;
    for (const auto& entry : SyntaxData::index().languages()) {
        if (entry.hidden || entry.unsupported)
            continue;
        sections[entry.section.isEmpty() ? tr("Other") : entry.section].append(entry);
    }
    auto sectionNames = sections.keys();
    std::sort(sectionNames.begin(), sectionNames.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    for (const auto& section : sectionNames) {
        // Index names are literal labels, not Qt mnemonic strings.
        auto* group = syntaxMenu->addMenu(QString(section).replace("&", "&&"));
        auto& entries = sections[section];
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return QString::localeAwareCompare(a.name, b.name) < 0;
        });
        for (const auto& entry : entries) {
            auto* action = addChoice(group, QString(entry.name).replace("&", "&&"),
                m_syntaxMode == SyntaxMode::Manual && m_syntaxFile == entry.file);
            connect(action, &QAction::triggered, this, [this, file = entry.file]() {
                const auto& index = SyntaxData::index();
                const auto* language = index.byFile(file);
                if (!language || language->unsupported
                    || !applyKateHighlighter(index.filePath(*language))) {
                    QMessageBox::warning(this, tr("Syntax"),
                                         tr("Cannot load syntax definition: %1").arg(file));
                    return;
                }
                m_syntaxMode = SyntaxMode::Manual;
                m_syntaxFile = file;
            });
        }
    }
    if (sections.isEmpty())
        syntaxMenu->addAction(tr("No syntax definitions installed"))->setEnabled(false);

    auto* themeMenu = menu.addMenu(tr("Theme"));
    auto* themeChoices = new QActionGroup(&menu);
    themeChoices->setExclusive(true);
    auto addTheme = [&](const QString& name, const QString& path) {
        auto* action = themeMenu->addAction(QString(name).replace("&", "&&"));
        action->setCheckable(true);
        action->setChecked(m_themeFile == path);
        themeChoices->addAction(action);
        connect(action, &QAction::triggered, this, [this, path]() {
            KateTheme theme;
            if (!path.isEmpty()) {
                theme = KateTheme::load(path);
                if (!theme.isValid()) {
                    QMessageBox::warning(this, tr("Theme"),
                                         tr("Cannot load theme: %1").arg(path));
                    return;
                }
            }
            m_themeFile = path;
            m_theme = theme;
            reapplyHighlighter();
        });
    };
    addTheme(tr("Default"), QString());
    themeMenu->addSeparator();
    const auto themes = KateTheme::listThemes(qce::kate::themesDir());
    for (const auto& theme : themes)
        addTheme(theme.first, theme.second);
    if (themes.isEmpty())
        themeMenu->addAction(tr("No themes installed"))->setEnabled(false);

    menu.exec(area->viewport()->mapToGlobal(position));
}

void Editor::applyThemePalette()
{
    QPalette palette = m_defaultPalette;
    if (m_theme.editorBackground.isValid()) {
        palette.setColor(QPalette::Base, m_theme.editorBackground);
        palette.setColor(QPalette::Window, m_theme.editorBackground);
    }
    const auto normal = m_theme.styles.constFind(QStringLiteral("Normal"));
    if (normal != m_theme.styles.constEnd() && normal->fg.isValid()) {
        palette.setColor(QPalette::Text, normal->fg);
        palette.setColor(QPalette::WindowText, normal->fg);
    }
    m_edit->setPalette(palette);
    m_edit->area()->setPalette(palette);
    m_edit->area()->viewport()->setPalette(palette);
    m_edit->area()->viewport()->update();
    // Invalid colours (no theme, or missing in it) are derived from the palette.
    m_edit->setGutterColors({m_theme.iconBorder, m_theme.lineNumbers, m_theme.separator});
}

qce::TextAttribute Editor::themedAttribute(const QString& style,
                                           const qce::TextAttribute& fallback) const
{
    const auto entry = m_theme.styles.constFind(style);
    if (entry == m_theme.styles.constEnd())
        return fallback;
    return {entry->fg.isValid() ? entry->fg : fallback.foreground,
            entry->bg, entry->bold, entry->italic, entry->underline};
}

void Editor::reapplyHighlighter()
{
    if (!m_themeFile.isEmpty()) {
        const auto theme = KateTheme::load(m_themeFile);
        if (theme.isValid())
            m_theme = theme;
    } else {
        // "Default": Breeze Light/Dark to match the desktop, or the editor's
        // built-in black on white when the themes are not downloaded.
        m_theme = KateTheme::loadDefault();
    }
    applyThemePalette();
    if (m_syntaxMode == SyntaxMode::PlainText) {
        clearHighlighter();
    } else if (m_syntaxMode == SyntaxMode::Manual) {
        const auto& index = SyntaxData::index();
        const auto* entry = index.byFile(m_syntaxFile);
        if (!entry || entry->unsupported || !applyKateHighlighter(index.filePath(*entry)))
            clearHighlighter();
    } else {
        applyHighlighterForFile(m_filePath);
    }
    // Setting a highlighter or folding provider may change the wrap mode.
    m_edit->area()->setWordWrap(m_wordWrap);
}

void Editor::applyHighlighterForFile(const QString& path)
{
    // Kate definitions downloaded by qcodeedit, matched by file name and
    // sorted by priority (definitions needing a newer Kate are skipped).
    const auto& index   = SyntaxData::index();
    const auto  matches = index.forFileName(path);
    if (!matches.isEmpty()) {
        if (!applyKateHighlighter(index.filePath(*matches.first())))
            clearHighlighter();
        return;
    }

    // No Kate definition — fall back to built-in programmatic highlighter for C/C++
    if (isCppExtension(QFileInfo(path).suffix())) {
        applyCppHighlighter();
        return;
    }

    clearHighlighter();
}

void Editor::applyCppHighlighter()
{
    using namespace qce;

    auto hl = std::make_unique<RulesHighlighter>();

    const int attrKw      = hl->addAttribute(themedAttribute(QStringLiteral("Keyword"), {QColor(0x00,0x00,0xAA), {}, true}));
    const int attrType    = hl->addAttribute(themedAttribute(QStringLiteral("DataType"), {QColor(0x00,0x80,0x80), {}, true}));
    const int attrString  = hl->addAttribute(themedAttribute(QStringLiteral("String"), {QColor(0xC0,0x10,0x10)}));
    const int attrComment = hl->addAttribute(themedAttribute(QStringLiteral("Comment"), {QColor(0x80,0x80,0x80), {}, false, true}));
    const int attrNumber  = hl->addAttribute(themedAttribute(QStringLiteral("DecVal"), {QColor(0x80,0x40,0x00)}));
    const int attrPP      = hl->addAttribute(themedAttribute(QStringLiteral("Preprocessor"), {QColor(0x60,0x00,0x80)}));

    const int klKeywords = hl->addKeywordList({"keywords", {
        "if","else","return","for","while","do","break","continue",
        "switch","case","default","goto","sizeof","alignof","noexcept",
        "try","catch","throw","new","delete","operator","template",
        "typename","namespace","using","class","struct","enum","union",
        "public","protected","private","virtual","override","final",
        "static","inline","explicit","friend","mutable","volatile",
        "extern","register","typedef","nullptr","true","false","this",
    }, true});

    const int klTypes = hl->addKeywordList({"types", {
        "int","void","char","bool","float","double","long","short",
        "unsigned","signed","size_t","auto","const","constexpr",
        "int8_t","int16_t","int32_t","int64_t",
        "uint8_t","uint16_t","uint32_t","uint64_t",
        "wchar_t","char8_t","char16_t","char32_t",
    }, true});

    // Contexts
    HighlightContext ctxN{"Normal",       -1,          -1, 0, false, -1, {}};
    HighlightContext ctxS{"String",       attrString,  -1, 0, false, -1, {}};
    HighlightContext ctxC{"Char",         attrString,  -1, 0, false, -1, {}};
    HighlightContext ctxB{"BlockComment", attrComment, -1, 0, false, -1, {}};
    HighlightContext ctxL{"LineComment",  attrComment,  0, 1, false, -1, {}};
    HighlightContext ctxP{"Preprocessor", attrPP,      0, 1, false, -1, {}};

    const int normal       = hl->addContext(ctxN);
    const int strCtx       = hl->addContext(ctxS);
    const int charCtx      = hl->addContext(ctxC);
    const int blockComment = hl->addContext(ctxB);
    const int lineComment  = hl->addContext(ctxL);
    const int preprocessor = hl->addContext(ctxP);

    // Folding region IDs
    const int rgCurly = hl->regionIdForName("curly");
    const int rgComm  = hl->regionIdForName("Comment");

    auto& norm = hl->contextRef(normal);

    // Preprocessor lines (#include, #define, …)
    norm.rules.push_back({HighlightRule::DetectChar, '#', {}, {}, true, {}, -1, -1,
                          attrPP, preprocessor, 0, true/*firstNonSpace*/, false});
    // Line comment //
    norm.rules.push_back({HighlightRule::Detect2Chars, '/', '/', {}, true, {}, -1, -1,
                          attrComment, lineComment, 0, false, false});
    // Block comment /* — also starts Comment fold region
    {
        HighlightRule r;
        r.kind = HighlightRule::Detect2Chars;
        r.ch = '/'; r.ch1 = '*';
        r.attributeId = attrComment; r.nextContextId = blockComment;
        r.beginRegionId = rgComm;
        norm.rules.push_back(r);
    }
    // String "
    norm.rules.push_back({HighlightRule::DetectChar, '"', {}, {}, true, {}, -1, -1,
                          attrString, strCtx, 0, false, false});
    // Char '
    norm.rules.push_back({HighlightRule::DetectChar, '\'', {}, {}, true, {}, -1, -1,
                          attrString, charCtx, 0, false, false});
    // Types (before keywords — both match identifiers)
    norm.rules.push_back({HighlightRule::Keyword, {}, {}, {}, true, {}, klTypes, -1,
                          attrType, -1, 0, false, false});
    // Keywords
    norm.rules.push_back({HighlightRule::Keyword, {}, {}, {}, true, {}, klKeywords, -1,
                          attrKw, -1, 0, false, false});
    // Numbers
    { HighlightRule r; r.kind = HighlightRule::Int;   r.attributeId = attrNumber; norm.rules.push_back(r); }
    { HighlightRule r; r.kind = HighlightRule::Float; r.attributeId = attrNumber; norm.rules.push_back(r); }
    // Opening brace { — starts curly fold region
    {
        HighlightRule r;
        r.kind = HighlightRule::DetectChar; r.ch = '{';
        r.beginRegionId = rgCurly;
        norm.rules.push_back(r);
    }
    // Closing brace } — ends curly fold region
    {
        HighlightRule r;
        r.kind = HighlightRule::DetectChar; r.ch = '}';
        r.endRegionId = rgCurly;
        norm.rules.push_back(r);
    }

    // String context
    auto& str = hl->contextRef(strCtx);
    str.rules.push_back({HighlightRule::HlCStringChar, {}, {}, {}, true, {}, -1, -1,
                         -1, -1, 0, false, false});
    str.rules.push_back({HighlightRule::DetectChar, '"', {}, {}, true, {}, -1, -1,
                         attrString, -1, 1, false, false});

    // Char context
    auto& ch = hl->contextRef(charCtx);
    ch.rules.push_back({HighlightRule::HlCStringChar, {}, {}, {}, true, {}, -1, -1,
                        -1, -1, 0, false, false});
    ch.rules.push_back({HighlightRule::DetectChar, '\'', {}, {}, true, {}, -1, -1,
                        attrString, -1, 1, false, false});

    // Block comment context — closing */ ends Comment fold region
    {
        HighlightRule r;
        r.kind = HighlightRule::Detect2Chars; r.ch = '*'; r.ch1 = '/';
        r.attributeId = attrComment; r.nextContextId = -1; r.popCount = 1;
        r.endRegionId = rgComm;
        hl->contextRef(blockComment).rules.push_back(r);
    }

    hl->setInitialContextId(normal);

    // Detach the old provider before its highlighter goes away.
    m_edit->area()->setFoldingProvider(nullptr);
    m_highlighter = std::move(hl);
    m_edit->area()->setHighlighter(m_highlighter.get());

    m_foldProvider = std::make_unique<qce::RuleBasedFoldingProvider>(m_highlighter.get());
    m_foldProvider->setPlaceholderFor("curly",   "{…}");
    m_foldProvider->setPlaceholderFor("Comment", "/*…*/");
    m_edit->area()->setFoldingProvider(m_foldProvider.get());
}

bool Editor::applyKateHighlighter(const QString& xmlPath)
{
    // ##Lang includes (e.g. Doxygen inside C++) are resolved through the index.
    auto hl = KateXmlReader::load(xmlPath, m_theme, SyntaxData::index());
    if (!hl) return false;

    // Detach the old provider before its highlighter goes away.
    m_edit->area()->setFoldingProvider(nullptr);
    m_highlighter = std::move(hl);
    m_edit->area()->setHighlighter(m_highlighter.get());

    m_foldProvider = std::make_unique<qce::RuleBasedFoldingProvider>(m_highlighter.get());
    m_foldProvider->setPlaceholderFor("Brace1",  "{…}");
    m_foldProvider->setPlaceholderFor("brace",   "{…}");
    m_foldProvider->setPlaceholderFor("curly",   "{…}");
    m_foldProvider->setPlaceholderFor("square",  "[…]");
    m_foldProvider->setPlaceholderFor("paren",   "(…)");
    m_foldProvider->setPlaceholderFor("Comment", "/*…*/");
    m_foldProvider->setPlaceholderFor("Region1", "//BEGIN…END");
    m_edit->area()->setFoldingProvider(m_foldProvider.get());
    m_edit->area()->setWordWrap(m_wordWrap);
    return true;
}

void Editor::clearHighlighter()
{
    m_edit->area()->setHighlighter(nullptr);
    m_edit->area()->setFoldingProvider(nullptr);
    m_highlighter.reset();
    m_foldProvider.reset();
}
