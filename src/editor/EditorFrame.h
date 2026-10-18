#ifndef EDITORFRAME_H
#define EDITORFRAME_H

#include <qceditor/EditorWindow.h>

/**
 * @class EditorFrame
 * @brief gemini-commander's editor window: qceditor's EditorWindow
 *
 * Adds what is gemini's own: the window geometry kept in its configuration
 * (relative to the main window), the TOML check of config.toml before saving
 * and configFileSaved() after it, and the editor tabs registered for gemini's
 * key handling ("Editor" in keys.toml).
 */
class EditorFrame : public qceditor::EditorWindow
{
    Q_OBJECT

public:
    explicit EditorFrame(QWidget* parent = nullptr);

signals:
    /// gemini-commander's config.toml was saved from the editor.
    void configFileSaved();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void saveGeometryToConfig();

    bool m_geometryRestored = false;
};

#endif // EDITORFRAME_H
