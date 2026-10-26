#include "ViewerFrame.h"
#include "ViewerWidget.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QEvent>

#include "../Config.h"

ViewerFrame::ViewerFrame(const QString& filePath, QWidget *anchor)
    : QDialog(nullptr, Qt::Window)  // no parent: not kept above the main window
    , m_anchor(anchor)
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
        focusView();
    }

    connect(m_viewerWidget, &ViewerWidget::fileDeleted, this, [this]() {
        hide();
    });
}

ViewerFrame::~ViewerFrame() = default;

void ViewerFrame::openFile(const QString& filePath)
{
    setWindowTitle(filePath);
    m_viewerWidget->openFile(filePath);
    focusView();
}

void ViewerFrame::focusView()
{
    // Through the focus proxies to the text, code or hex view. In a hidden or
    // inactive window it takes effect when the window is activated.
    m_viewerWidget->setFocus(Qt::OtherFocusReason);
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

    // The keys belong to the view's find bar / search (shortcuts within the
    // view); here they are only shown, as shortcuts on these actions too
    // would be ambiguous and Qt would run neither.
    QMenu* searchMenu = m_menuBar->addMenu(tr("&Search"));
    const QList<QAction*> searchActions = {
        searchMenu->addAction(tr("&Find...") + QStringLiteral("\tCtrl+F"),
                              this, [this]() { m_viewerWidget->showSearch(); }),
        searchMenu->addAction(tr("Find &Next") + QStringLiteral("\tF3"),
                              this, [this]() { m_viewerWidget->findNext(); }),
        searchMenu->addAction(tr("Find &Previous") + QStringLiteral("\tShift+F3"),
                              this, [this]() { m_viewerWidget->findNext(true); }),
    };
    connect(searchMenu, &QMenu::aboutToShow, this, [this, searchActions]() {
        for (QAction* action : searchActions)
            action->setEnabled(m_viewerWidget->canSearch());
    });
}

void ViewerFrame::onTextModeTriggered()
{
    m_viewerWidget->setViewMode(ViewerWidget::ViewMode::Text);
    updateMenuChecks();
    focusView();
}

void ViewerFrame::onHexModeTriggered()
{
    m_viewerWidget->setViewMode(ViewerWidget::ViewMode::Hex);
    updateMenuChecks();
    focusView();
}

void ViewerFrame::updateMenuChecks()
{
    bool isText = (m_viewerWidget->viewMode() == ViewerWidget::ViewMode::Text);
    m_textAction->setChecked(isText);
    m_hexAction->setChecked(!isText);
}

void ViewerFrame::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        // A reload recreates the view; keep the focus in it.
        const bool hadFocus = m_viewerWidget->isAncestorOf(QApplication::focusWidget());
        m_viewerWidget->refreshIfChanged();
        if (hadFocus || !focusWidget())
            focusView();
    }
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

        // Position relative to the main window
        if (m_anchor) {
            QPoint anchorPos = m_anchor->pos();
            move(anchorPos.x() + relX, anchorPos.y() + relY);
        }
    }
}

void ViewerFrame::saveGeometryToConfig()
{
    Config& cfg = Config::instance();

    int relX = 0;
    int relY = 0;

    // Calculate position relative to the main window
    if (m_anchor) {
        QPoint anchorPos = m_anchor->pos();
        relX = x() - anchorPos.x();
        relY = y() - anchorPos.y();
    }

    cfg.setViewerGeometry(relX, relY, width(), height());
    cfg.save();
}
