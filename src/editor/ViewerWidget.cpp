#include "ViewerWidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QUrl>
#include "../../external/textviewers/wid/TextViewer.h"

// KTextEditor includes
#include <KTextEditor/Editor>
#include <KTextEditor/Document>
#include <KTextEditor/View>

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
    if (filePath == m_currentFile && (m_textViewer || m_kteView))
        return;

    clearViewer();
    m_file.reset();

    m_currentFile = filePath;

    QFileInfo fileInfo(filePath);
    qint64 fileSize = fileInfo.size();

    if (fileSize == 0) {
        auto* label = new QLabel(tr("(empty file)"), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    // Large files: use wid::TextViewer (memory-mapped)
    if (fileSize > SmallFileThreshold) {
        m_file = std::make_unique<QFile>(filePath);
        if (!m_file->open(QIODevice::ReadOnly)) {
            auto* label = new QLabel(tr("Cannot open file:\n%1").arg(filePath), this);
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
    } else {
        // Small files: use KTextEditor
        createKTextEditorView(filePath);
    }
}

void ViewerWidget::clear()
{
    clearViewer();
    m_file.reset();
    m_currentFile.clear();
}

void ViewerWidget::createTextViewer(uchar* data, qint64 size)
{
    m_textViewer = new wid::TextViewer(reinterpret_cast<char*>(data), size, this);
    m_layout->addWidget(m_textViewer);
    setFocusProxy(m_textViewer);
}

void ViewerWidget::createKTextEditorView(const QString& filePath)
{
    KTextEditor::Editor* editor = KTextEditor::Editor::instance();
    if (!editor) {
        auto* label = new QLabel(tr("KTextEditor not available"), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    m_kteDocument = editor->createDocument(this);
    if (!m_kteDocument) {
        auto* label = new QLabel(tr("Cannot create document"), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    QUrl fileUrl = QUrl::fromLocalFile(filePath);
    if (!m_kteDocument->openUrl(fileUrl)) {
        auto* label = new QLabel(tr("Cannot open file:\n%1").arg(filePath), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        delete m_kteDocument;
        m_kteDocument = nullptr;
        return;
    }

    // Set read-only mode for viewing
    m_kteDocument->setReadWrite(false);

    m_kteView = m_kteDocument->createView(this);
    if (!m_kteView) {
        auto* label = new QLabel(tr("Cannot create view"), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        delete m_kteDocument;
        m_kteDocument = nullptr;
        return;
    }

    m_layout->addWidget(m_kteView);
    setFocusProxy(m_kteView);
}

void ViewerWidget::clearViewer()
{
    if (m_textViewer) {
        m_layout->removeWidget(m_textViewer);
        delete m_textViewer;
        m_textViewer = nullptr;
    }

    if (m_kteView) {
        m_layout->removeWidget(m_kteView);
        delete m_kteView;
        m_kteView = nullptr;
    }

    if (m_kteDocument) {
        delete m_kteDocument;
        m_kteDocument = nullptr;
    }

    // Also remove any labels
    while (m_layout->count() > 0) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }
}
