#include "ViewerFrame.h"

#include <QKeyEvent>
#include <QVBoxLayout>
#include <QFile>
#include "../../external/textviewers/wid/TextViewer.h"

#include "keys/ObjectRegistry.h"

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
}

ViewerFrame::~ViewerFrame() = default;
