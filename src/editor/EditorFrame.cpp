#include "EditorFrame.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QShowEvent>
#include <qceditor/EditorTab.h>

#include "../Config.h"
#include "keys/ObjectRegistry.h"

namespace {
qceditor::EditorWindowOptions editorOptions()
{
    qceditor::EditorWindowOptions options;
    options.tabLimit = 16;
    // A modified file changed on disk is left alone unless the user turns on
    // Tools > Ask to Reload Modified Files.
    options.askToReloadModified = false;
    return options;
}
}

EditorFrame::EditorFrame(QWidget* parent)
    : qceditor::EditorWindow(editorOptions(), parent)
{
    // gemini's config.toml must stay valid TOML: the save is refused otherwise.
    setSaveValidator([this](qceditor::EditorTab* tab, const QString& path) {
        if (!Config::instance().isConfigFile(path))
            return true;
        QString error;
        if (Config::validateToml(tab->document()->toPlainText(), error))
            return true;
        QMessageBox::critical(this, tr("Invalid Configuration"),
                              tr("Cannot save: TOML syntax error.\n\n%1").arg(error));
        return false;
    });
    connect(this, &qceditor::EditorWindow::tabSaved, this, [this](qceditor::EditorTab*, const QString& path) {
        if (Config::instance().isConfigFile(path))
            emit configFileSaved();
    });
    // Keys of the editor tabs follow the [Editor] section of keys.toml.
    connect(tabWidget(), &MruTabWidget::tabCountChanged, this, [this]() {
        for (int i = 0; i < tabWidget()->count(); ++i)
            if (auto* tab = tabAt(i)) ObjectRegistry::add(tab, "Editor");
    });
}

void EditorFrame::showEvent(QShowEvent* event)
{
    qceditor::EditorWindow::showEvent(event);
    if (m_geometryRestored)
        return;
    m_geometryRestored = true;
    const Config& cfg = Config::instance();
    resize(cfg.editorWidth(), cfg.editorHeight());
    // Position relative to the parent window.
    if (parentWidget()) {
        const QPoint parentPos = parentWidget()->pos();
        move(parentPos.x() + cfg.editorX(), parentPos.y() + cfg.editorY());
    }
}

void EditorFrame::closeEvent(QCloseEvent* event)
{
    qceditor::EditorWindow::closeEvent(event);
    if (event->isAccepted())
        saveGeometryToConfig();
}

void EditorFrame::saveGeometryToConfig()
{
    Config& cfg = Config::instance();
    int relX = 0;
    int relY = 0;
    // Position relative to the parent window.
    if (parentWidget()) {
        const QPoint parentPos = parentWidget()->pos();
        relX = x() - parentPos.x();
        relY = y() - parentPos.y();
    }
    cfg.setEditorGeometry(relX, relY, width(), height());
    cfg.save();
}
