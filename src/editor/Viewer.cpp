#include "Viewer.h"

#include <QVBoxLayout>

Viewer::Viewer(const QString& fileName, QWidget *parent):BaseViewer(parent) {
    m_filePath = fileName;
    file = std::make_unique<QFile>(m_filePath);
    if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "error open file";
        return;
    }
    uchar *addr = file->map(0, file->size());
    widget = new wid::TextViewer((char *) addr, file->size(), this);
    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(widget);
    setLayout(mainLayout);
}

#include "Viewer_impl.inc"