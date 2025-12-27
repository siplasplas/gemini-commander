#include "ViewerFrame.h"

#include <QKeyEvent>
#include <QVBoxLayout>
#include <QFile>
#include <QShowEvent>
#include <QCloseEvent>
#include "../../external/textviewers/wid/TextViewer.h"

#include "keys/ObjectRegistry.h"
#include "../Config.h"
#include <QDebug>

ViewerFrame::ViewerFrame(const QString& filePath, QWidget *parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(filePath);
    resize(800, 600);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    file = std::make_unique<QFile>(filePath);
    if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "error open file";
        return;
    }
    if (file->size()==0)
        return;
    uchar *addr = file->map(0, file->size());
    m_viewer = new wid::TextViewer((char *) addr, file->size(), this);
    m_layout->addWidget(m_viewer);
    setFocusProxy(m_viewer);
}

ViewerFrame::~ViewerFrame() = default;

void ViewerFrame::openFile(const QString& filePath) {
    // Clean up previous viewer
    if (m_viewer) {
        m_layout->removeWidget(m_viewer);
        delete m_viewer;
        m_viewer = nullptr;
    }

    // Close previous file
    file.reset();

    setWindowTitle(filePath);

    file = std::make_unique<QFile>(filePath);
    if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "error open file";
        return;
    }
    if (file->size() == 0)
        return;
    uchar *addr = file->map(0, file->size());
    m_viewer = new wid::TextViewer((char *) addr, file->size(), this);
    m_layout->addWidget(m_viewer);
    setFocusProxy(m_viewer);
}

void ViewerFrame::closeEvent(QCloseEvent* event)
{
    saveGeometryToConfig();
    QDialog::closeEvent(event);
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
