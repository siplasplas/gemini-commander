#include "ViewerWidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include "../../external/textviewers/wid/TextViewer.h"

ViewerWidget::ViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
}

ViewerWidget::~ViewerWidget()
{
    clear();
}

void ViewerWidget::openFile(const QString& filePath)
{
    // Same file - do nothing
    if (filePath == m_currentFile && m_viewer)
        return;

    clearViewer();
    m_file.reset();

    m_currentFile = filePath;

    m_file = std::make_unique<QFile>(filePath);
    if (!m_file->open(QIODevice::ReadOnly)) {
        // Show error label
        auto* label = new QLabel(tr("Cannot open file:\n%1").arg(filePath), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    if (m_file->size() == 0) {
        auto* label = new QLabel(tr("(empty file)"), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    uchar* addr = m_file->map(0, m_file->size());
    if (!addr) {
        auto* label = new QLabel(tr("Cannot map file:\n%1").arg(filePath), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    createTextViewer(addr, m_file->size());
}

void ViewerWidget::clear()
{
    clearViewer();
    m_file.reset();
    m_currentFile.clear();
}

void ViewerWidget::createTextViewer(uchar* data, qint64 size)
{
    // TODO: In future, use KTextEditor for small files
    m_viewer = new wid::TextViewer(reinterpret_cast<char*>(data), size, this);
    m_layout->addWidget(m_viewer);
    setFocusProxy(m_viewer);
}

void ViewerWidget::clearViewer()
{
    if (m_viewer) {
        m_layout->removeWidget(m_viewer);
        delete m_viewer;
        m_viewer = nullptr;
    }

    // Also remove any labels
    while (m_layout->count() > 0) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }
}
