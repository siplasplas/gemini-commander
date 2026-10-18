#pragma once

#include <QHash>
#include <qceditor/Settings.h>

// The editor's settings (qceditor's library) in gemini-commander's TOML
// configuration, [editor] section. Keys the configuration has no place for
// are kept for the session only.
class EditorSettings : public qceditor::Settings
{
public:
    QVariant value(const QString& key, const QVariant& defaultValue = {}) const override;
    void setValue(const QString& key, const QVariant& value) override;
    void remove(const QString& key) override;

private:
    QHash<QString, QVariant> m_session;
};
