#include "ViewerFrame.h"
#include "Viewer.h"

#include <QKeyEvent>
#include <QVBoxLayout>

#include "keys/ObjectRegistry.h"

ViewerFrame::ViewerFrame(const QString& filePath, QWidget *parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(filePath);
    resize(800, 600);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_viewer = new Viewer(filePath, this);
    ObjectRegistry::add(m_viewer, "Viewer");
    m_layout->addWidget(m_viewer);
}

ViewerFrame::~ViewerFrame() = default;
