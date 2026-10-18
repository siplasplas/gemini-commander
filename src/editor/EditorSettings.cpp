#include "EditorSettings.h"

#include "../Config.h"

namespace {
const QString kSpelling = QStringLiteral("spelling/enabled");
const QString kRecentDictionaries = QStringLiteral("spelling/recentDictionaries");
const QString kLegacyLanguages = QStringLiteral("encoding/legacyLanguages");
const QString kRecentFiles = QStringLiteral("recentFiles");
const QString kWordWrap = QStringLiteral("view/wordWrap");
const QString kShowWhitespace = QStringLiteral("view/showWhitespace");
const QString kAskToReload = QStringLiteral("files/askToReloadModified");
const QString kDeclinedDownload = QStringLiteral("syntaxData/declinedDownload");
const QString kTrimWhitespace = QStringLiteral("editing/trimTrailingWhitespace");
}

QVariant EditorSettings::value(const QString& key, const QVariant& defaultValue) const
{
    const Config& cfg = Config::instance();
    if (key == kSpelling) return cfg.editorSpellCheck();
    if (key == kRecentDictionaries) return cfg.editorRecentSpellDictionaries();
    if (key == kLegacyLanguages) return cfg.editorLegacyLanguages();
    if (key == kRecentFiles) return cfg.editorMruPaths();
    if (key == kWordWrap) return cfg.editorWordWrap();
    if (key == kShowWhitespace) return cfg.editorShowWhitespace();
    if (key == kAskToReload) return cfg.editorAskToReloadModified();
    if (key == kDeclinedDownload) return cfg.syntaxDownloadDeclined();
    if (key == kTrimWhitespace) return cfg.editorTrimTrailingWhitespace();
    return m_session.value(key, defaultValue);
}

void EditorSettings::setValue(const QString& key, const QVariant& value)
{
    Config& cfg = Config::instance();
    if (key == kSpelling) cfg.setEditorSpellCheck(value.toBool());
    else if (key == kRecentDictionaries) cfg.setEditorRecentSpellDictionaries(value.toStringList());
    else if (key == kLegacyLanguages) cfg.setEditorLegacyLanguages(value.toStringList());
    else if (key == kRecentFiles) cfg.setEditorMruPaths(value.toStringList().mid(0, cfg.editorMruMaxCount()));
    else if (key == kWordWrap) cfg.setEditorWordWrap(value.toBool());
    else if (key == kShowWhitespace) cfg.setEditorShowWhitespace(value.toBool());
    else if (key == kAskToReload) cfg.setEditorAskToReloadModified(value.toBool());
    else if (key == kDeclinedDownload) cfg.setSyntaxDownloadDeclined(value.toBool());
    else if (key == kTrimWhitespace) cfg.setEditorTrimTrailingWhitespace(value.toString());
    else {
        m_session.insert(key, value);
        return;
    }
    cfg.save();
}

void EditorSettings::remove(const QString& key)
{
    Config& cfg = Config::instance();
    if (key == kSpelling) cfg.setEditorSpellCheck(true);
    else if (key == kRecentDictionaries) cfg.setEditorRecentSpellDictionaries({});
    else if (key == kLegacyLanguages) cfg.setEditorLegacyLanguages({});
    else if (key == kRecentFiles) cfg.setEditorMruPaths({});
    else if (key == kWordWrap) cfg.setEditorWordWrap(true);
    else if (key == kShowWhitespace) cfg.setEditorShowWhitespace(false);
    else if (key == kAskToReload) cfg.setEditorAskToReloadModified(false);
    else if (key == kDeclinedDownload) cfg.setSyntaxDownloadDeclined(false);
    else if (key == kTrimWhitespace) cfg.setEditorTrimTrailingWhitespace(QStringLiteral("changed"));
    else {
        m_session.remove(key);
        return;
    }
    cfg.save();
}
