#include "ViewerWidget.h"
#include "HexViewWidget.h"
#include "editor.h"
#include "../Config.h"

#include <QApplication>
#include <QClipboard>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QMenu>
#include <QContextMenuEvent>
#include <QEvent>
#include <qce/CodeEdit.h>
#include <qce/CodeEditArea.h>
#include "wid/TextViewer.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
// Get system RAM in bytes
qint64 getSystemRamBytes()
{
#ifdef Q_OS_LINUX
    // /proc/meminfo is a virtual file with size 0, must use readAll()
    QFile meminfo("/proc/meminfo");
    if (meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray content = meminfo.readAll();
        meminfo.close();

        QStringList lines = QString::fromUtf8(content).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            if (line.startsWith("MemTotal:")) {
                // Format: "MemTotal:       16384000 kB"
                QStringList parts = line.simplified().split(' ');
                if (parts.size() >= 2) {
                    bool ok;
                    qint64 kB = parts[1].toLongLong(&ok);
                    if (ok) {
                        return kB * 1024;  // Convert KB to bytes
                    }
                }
                break;
            }
        }
    }
    return 4LL * 1024 * 1024 * 1024;  // Fallback: 4 GB
#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return static_cast<qint64>(memStatus.ullTotalPhys);
    }
    return 4LL * 1024 * 1024 * 1024;  // Fallback: 4 GB
#else
    return 4LL * 1024 * 1024 * 1024;  // Fallback: 4 GB
#endif
}
} // namespace

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
    if (filePath == m_currentFile && (m_textViewer || m_codeView || m_hexViewer)) {
        QFileInfo fi(filePath);
        if (!fi.exists()) {
            emit fileDeleted(filePath);
            return;
        }
        if (fi.lastModified() == m_loadedModified && fi.size() == m_loadedSize)
            return;
        // File changed on disk - reload it
    }

    clearViewer();
    m_file.reset();

    m_currentFile = filePath;

    // Open file (needed for mmap in TextViewer/HexViewer, not for the code view)
    m_file = std::make_unique<QFile>(filePath);
    if (!m_file->open(QIODevice::ReadOnly)) {
        auto* label = new QLabel(tr("Cannot open file:\n%1").arg(filePath), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    QFileInfo fi(filePath);
    m_loadedModified = fi.lastModified();
    m_loadedSize = m_file->size();

    if (m_viewMode == ViewMode::Hex) {
        showHexView();
    } else {
        showTextView();
    }
}

void ViewerWidget::refreshIfChanged()
{
    if (!m_currentFile.isEmpty())
        openFile(m_currentFile);
}

void ViewerWidget::clear()
{
    clearViewer();
    m_file.reset();
    m_currentFile.clear();
}

void ViewerWidget::setViewMode(ViewMode mode)
{
    if (mode == m_viewMode)
        return;

    m_viewMode = mode;

    // If we have a file open, switch view
    if (!m_currentFile.isEmpty() && m_file && m_file->isOpen()) {
        clearViewer();
        if (mode == ViewMode::Hex) {
            showHexView();
        } else {
            showTextView();
        }
    }
}

bool ViewerWidget::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);
    if (event->type() == QEvent::ContextMenu) {
        auto* contextEvent = static_cast<QContextMenuEvent*>(event);
        QMenu menu(this);

        // Add view mode options
        QAction* textAction = menu.addAction(tr("Text"));
        textAction->setCheckable(true);
        textAction->setChecked(m_viewMode == ViewMode::Text);

        QAction* hexAction = menu.addAction(tr("Hex"));
        hexAction->setCheckable(true);
        hexAction->setChecked(m_viewMode == ViewMode::Hex);

        menu.addSeparator();

        // Add editor-specific actions
        QAction* copyAction = nullptr;
        if (m_codeView) {
            copyAction = menu.addAction(tr("Copy"));
            copyAction->setEnabled(m_codeView->codeEdit()->area()->hasSelection());
        } else if (m_textViewer || m_hexViewer) {
            copyAction = menu.addAction(tr("Copy"));
        }

        QAction* selected = menu.exec(contextEvent->globalPos());

        if (selected == textAction) {
            setViewMode(ViewMode::Text);
        } else if (selected == hexAction) {
            setViewMode(ViewMode::Hex);
        } else if (selected == copyAction) {
            if (m_codeView) {
                QApplication::clipboard()->setText(m_codeView->codeEdit()->area()->selectedText());
            }
            // For TextViewer - PaintArea handles copy via its own mechanism
            // For HexViewer - TODO: implement copy
        }

        return true;  // Event handled
    }

    return QWidget::eventFilter(watched, event);
}

void ViewerWidget::showTextView()
{
    if (!m_file)
        return;

    qint64 fileSize = m_file->size();

    // Virtual file (size 0) - try readAll(), then decide based on content size
    if (fileSize == 0) {
        m_virtualFileContent = m_file->readAll();
        if (m_virtualFileContent.isEmpty()) {
            auto* label = new QLabel(tr("(empty file)"), this);
            label->setAlignment(Qt::AlignCenter);
            m_layout->addWidget(label);
            return;
        }
        fileSize = m_virtualFileContent.size();
    }

    // Calculate effective threshold: min(config threshold, 10% of RAM)
    double configThresholdMB = Config::instance().kteThresholdMB();
    auto configThresholdBytes = static_cast<qint64>(configThresholdMB * 1024.0 * 1024.0);
    qint64 ramLimitBytes = getSystemRamBytes() / 10;  // 10% of RAM
    qint64 effectiveThreshold = qMin(configThresholdBytes, ramLimitBytes);

    // Large files (> threshold): use wid::TextViewer
    if (fileSize > effectiveThreshold) {
        uchar* addr = nullptr;
        if (!m_virtualFileContent.isEmpty()) {
            // Virtual file already read
            addr = reinterpret_cast<uchar*>(m_virtualFileContent.data());
        } else {
            // Regular file - use mmap
            addr = m_file->map(0, fileSize);
            if (!addr) {
                auto* label = new QLabel(tr("Cannot map file:\n%1").arg(m_currentFile), this);
                label->setAlignment(Qt::AlignCenter);
                m_layout->addWidget(label);
                return;
            }
        }
        createTextViewer(addr, fileSize);
    } else {
        // Small files (<= threshold): use the highlighting code view
        createCodeView(m_currentFile);
    }
}

void ViewerWidget::showHexView()
{
    if (!m_file)
        return;

    if (m_file->size() == 0) {
        // Virtual file (like /proc/*) - try readAll()
        m_virtualFileContent = m_file->readAll();
        if (m_virtualFileContent.isEmpty()) {
            auto* label = new QLabel(tr("(empty file)"), this);
            label->setAlignment(Qt::AlignCenter);
            m_layout->addWidget(label);
            return;
        }
        createHexViewer(reinterpret_cast<uchar*>(m_virtualFileContent.data()),
                        m_virtualFileContent.size());
        return;
    }

    uchar* addr = m_file->map(0, m_file->size());
    if (!addr) {
        auto* label = new QLabel(tr("Cannot map file:\n%1").arg(m_currentFile), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    createHexViewer(addr, m_file->size());
}

void ViewerWidget::createTextViewer(uchar* data, qint64 size)
{
    m_textViewer = new wid::TextViewer(reinterpret_cast<char*>(data), size, this);
    // Install event filter on TextViewer and all its children (including PaintArea)
    m_textViewer->installEventFilter(this);
    for (QWidget* child : m_textViewer->findChildren<QWidget*>()) {
        child->installEventFilter(this);
    }
    m_layout->addWidget(m_textViewer);
    setFocusProxy(m_textViewer);
}

void ViewerWidget::createCodeView(const QString& filePath)
{
    m_codeView = new Editor(this);
    if (!m_codeView->loadFile(filePath)) {
        delete m_codeView;
        m_codeView = nullptr;
        auto* label = new QLabel(tr("Cannot open file:\n%1").arg(filePath), this);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }
    m_codeView->setReadOnly(true);
    m_codeView->setWordWrap(Config::instance().editorWordWrap());

    // Install event filter on the code view and all its children
    m_codeView->installEventFilter(this);
    for (QWidget* child : m_codeView->findChildren<QWidget*>()) {
        child->installEventFilter(this);
    }
    m_layout->addWidget(m_codeView);
    setFocusProxy(m_codeView);
}

void ViewerWidget::createHexViewer(uchar* data, qint64 size)
{
    m_hexViewer = new HexViewWidget(this);
    m_hexViewer->setData(reinterpret_cast<const char*>(data), size);
    // Install event filter on HexViewer and its viewport
    m_hexViewer->installEventFilter(this);
    m_hexViewer->viewport()->installEventFilter(this);
    m_layout->addWidget(m_hexViewer);
    setFocusProxy(m_hexViewer);
}

void ViewerWidget::clearViewer()
{
    if (m_textViewer) {
        m_layout->removeWidget(m_textViewer);
        delete m_textViewer;
        m_textViewer = nullptr;
    }

    if (m_codeView) {
        m_layout->removeWidget(m_codeView);
        delete m_codeView;
        m_codeView = nullptr;
    }

    if (m_hexViewer) {
        m_layout->removeWidget(m_hexViewer);
        delete m_hexViewer;
        m_hexViewer = nullptr;
    }

    // Clear virtual file content buffer
    m_virtualFileContent.clear();

    // Also remove any labels
    while (m_layout->count() > 0) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }
}
