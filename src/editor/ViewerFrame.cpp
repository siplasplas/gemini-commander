#include "ViewerFrame.h"
#include "ViewerWidget.h"

#include <QVBoxLayout>
#include <QShowEvent>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

#include "../Config.h"

ViewerFrame::ViewerFrame(const QString& filePath, QWidget *parent)
    : QDialog(parent)
{
    // Don't use WA_DeleteOnClose - we want to keep the widget for mode persistence
    setWindowTitle(filePath);
    resize(800, 600);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    setupMenuBar();

    m_viewerWidget = new ViewerWidget(this);
    m_layout->addWidget(m_viewerWidget);
    setFocusProxy(m_viewerWidget);

    if (!filePath.isEmpty()) {
        m_viewerWidget->openFile(filePath);
    }
}

ViewerFrame::~ViewerFrame() = default;

void ViewerFrame::openFile(const QString& filePath)
{
    setWindowTitle(filePath);
    m_viewerWidget->openFile(filePath);
}

void ViewerFrame::setupMenuBar()
{
    m_menuBar = new QMenuBar(this);
    m_layout->setMenuBar(m_menuBar);

    QMenu* viewMenu = m_menuBar->addMenu(tr("&View"));

    m_textAction = viewMenu->addAction(tr("&Text"));
    m_textAction->setCheckable(true);
    m_textAction->setChecked(true);
    m_textAction->setShortcut(QKeySequence(Qt::Key_T));
    connect(m_textAction, &QAction::triggered, this, &ViewerFrame::onTextModeTriggered);

    m_hexAction = viewMenu->addAction(tr("&Hex"));
    m_hexAction->setCheckable(true);
    m_hexAction->setShortcut(QKeySequence(Qt::Key_H));
    connect(m_hexAction, &QAction::triggered, this, &ViewerFrame::onHexModeTriggered);
}

void ViewerFrame::onTextModeTriggered()
{
    m_viewerWidget->setViewMode(ViewerWidget::ViewMode::Text);
    updateMenuChecks();
}

void ViewerFrame::onHexModeTriggered()
{
    m_viewerWidget->setViewMode(ViewerWidget::ViewMode::Hex);
    updateMenuChecks();
}

void ViewerFrame::updateMenuChecks()
{
    bool isText = (m_viewerWidget->viewMode() == ViewerWidget::ViewMode::Text);
    m_textAction->setChecked(isText);
    m_hexAction->setChecked(!isText);
}

void ViewerFrame::closeEvent(QCloseEvent* event)
{
    saveGeometryToConfig();
    // Just hide instead of close - preserves view mode for next F3
    event->ignore();
    hide();
}

void ViewerFrame::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    if (!m_geometryRestored) {
        m_geometryRestored = true;

        Config& cfg = Config::instance();
        int w = cfg.viewerWidth();
        int h = cfg.viewerHeight();
        int relX = cfg.viewerX();
        int relY = cfg.viewerY();

        resize(w, h);

        // Position relative to parent window
        if (parentWidget()) {
            QPoint parentPos = parentWidget()->pos();
            move(parentPos.x() + relX, parentPos.y() + relY);
        }
    }
}

void ViewerFrame::saveGeometryToConfig()
{
    Config& cfg = Config::instance();

    int relX = 0;
    int relY = 0;

    // Calculate position relative to parent window
    if (parentWidget()) {
        QPoint parentPos = parentWidget()->pos();
        relX = x() - parentPos.x();
        relY = y() - parentPos.y();
    }

    cfg.setViewerGeometry(relX, relY, width(), height());
    cfg.save();
}
