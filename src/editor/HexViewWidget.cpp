#include "HexViewWidget.h"
#include <QPainter>
#include <QScrollBar>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

HexViewWidget::HexViewWidget(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFont(QFont("Monospace", 10));
    QFontMetrics fm(font());
    m_charWidth = fm.horizontalAdvance('0');
    m_charHeight = fm.height();

    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
}

HexViewWidget::~HexViewWidget()
{
}

void HexViewWidget::setData(const char* data, qint64 size)
{
    m_data = data;
    m_size = size;
    m_cursorPosition = 0;
    updateScrollbars();
    viewport()->update();
}

void HexViewWidget::clear()
{
    m_data = nullptr;
    m_size = 0;
    m_cursorPosition = 0;
    viewport()->update();
}

void HexViewWidget::setBytesPerLine(int bytes)
{
    m_bytesPerLine = bytes;
    updateScrollbars();
    viewport()->update();
}

int HexViewWidget::bytesPerLine() const
{
    return m_bytesPerLine;
}

void HexViewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!m_data || m_size == 0)
        return;

    QPainter painter(viewport());
    painter.setFont(font());

    int lineHeight = m_charHeight;
    int offsetWidth = 10 * m_charWidth;  // Address column
    int hexWidth = (3 * m_bytesPerLine) * m_charWidth;
    int gapWidth = 2 * m_charWidth;
    int asciiX = offsetWidth + hexWidth + gapWidth;

    qint64 firstLine = verticalScrollBar()->value();
    qint64 visibleLines = viewport()->height() / lineHeight + 1;
    qint64 totalLines = (m_size + m_bytesPerLine - 1) / m_bytesPerLine;
    qint64 lastLine = qMin(firstLine + visibleLines, totalLines);

    for (qint64 line = firstLine; line < lastLine; ++line) {
        int y = static_cast<int>((line - firstLine) * lineHeight + m_charHeight);

        // Draw address
        painter.setPen(Qt::gray);
        painter.drawText(0, y, QString("%1").arg(line * m_bytesPerLine, 8, 16, QChar('0')).toUpper());

        // Draw hex and ascii
        painter.setPen(Qt::black);
        for (int i = 0; i < m_bytesPerLine; ++i) {
            qint64 pos = line * m_bytesPerLine + i;
            if (pos >= m_size)
                break;

            unsigned char byte = static_cast<unsigned char>(m_data[pos]);

            int hexX = offsetWidth + i * 3 * m_charWidth;

            // Highlight cursor in hex area
            if (pos == m_cursorPosition && m_focusArea == HexArea) {
                painter.fillRect(hexX, y - m_charHeight + 2,
                               2 * m_charWidth, m_charHeight,
                               QColor(200, 200, 255));
            }

            painter.drawText(hexX, y, QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());

            // ASCII area
            int asciiPos = asciiX + i * m_charWidth;
            if (pos == m_cursorPosition && m_focusArea == AsciiArea) {
                painter.fillRect(asciiPos, y - m_charHeight + 2,
                               m_charWidth, m_charHeight,
                               QColor(200, 200, 255));
            }

            QChar ch = (byte >= 32 && byte < 127) ? QChar(byte) : QChar('.');
            painter.drawText(asciiPos, y, ch);
        }
    }
}

void HexViewWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollbars();
}

void HexViewWidget::mousePressEvent(QMouseEvent *event)
{
    qint64 pos = positionAtPoint(event->pos());
    if (pos >= 0 && pos < m_size) {
        m_cursorPosition = pos;
        viewport()->update();
    }
}

void HexViewWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_data || m_size == 0)
        return;

    switch (event->key()) {
    case Qt::Key_Left:
        if (m_cursorPosition > 0) {
            m_cursorPosition--;
            viewport()->update();
        }
        break;
    case Qt::Key_Right:
        if (m_cursorPosition < m_size - 1) {
            m_cursorPosition++;
            viewport()->update();
        }
        break;
    case Qt::Key_Up:
        if (m_cursorPosition >= m_bytesPerLine) {
            m_cursorPosition -= m_bytesPerLine;
            viewport()->update();
        }
        break;
    case Qt::Key_Down:
        if (m_cursorPosition + m_bytesPerLine < m_size) {
            m_cursorPosition += m_bytesPerLine;
            viewport()->update();
        }
        break;
    case Qt::Key_PageUp:
        {
            qint64 pageSize = (viewport()->height() / m_charHeight) * m_bytesPerLine;
            m_cursorPosition = qMax(0LL, m_cursorPosition - pageSize);
            viewport()->update();
        }
        break;
    case Qt::Key_PageDown:
        {
            qint64 pageSize = (viewport()->height() / m_charHeight) * m_bytesPerLine;
            m_cursorPosition = qMin(m_size - 1, m_cursorPosition + pageSize);
            viewport()->update();
        }
        break;
    case Qt::Key_Home:
        if (event->modifiers() & Qt::ControlModifier) {
            m_cursorPosition = 0;
        } else {
            m_cursorPosition = (m_cursorPosition / m_bytesPerLine) * m_bytesPerLine;
        }
        viewport()->update();
        break;
    case Qt::Key_End:
        if (event->modifiers() & Qt::ControlModifier) {
            m_cursorPosition = m_size - 1;
        } else {
            m_cursorPosition = qMin(m_size - 1,
                (m_cursorPosition / m_bytesPerLine + 1) * m_bytesPerLine - 1);
        }
        viewport()->update();
        break;
    case Qt::Key_Tab:
        m_focusArea = (m_focusArea == HexArea) ? AsciiArea : HexArea;
        viewport()->update();
        break;
    default:
        QAbstractScrollArea::keyPressEvent(event);
        break;
    }

    // Ensure cursor is visible
    qint64 cursorLine = m_cursorPosition / m_bytesPerLine;
    qint64 firstVisible = verticalScrollBar()->value();
    qint64 visibleLines = viewport()->height() / m_charHeight;

    if (cursorLine < firstVisible) {
        verticalScrollBar()->setValue(static_cast<int>(cursorLine));
    } else if (cursorLine >= firstVisible + visibleLines) {
        verticalScrollBar()->setValue(static_cast<int>(cursorLine - visibleLines + 1));
    }
}

void HexViewWidget::wheelEvent(QWheelEvent *event)
{
    QAbstractScrollArea::wheelEvent(event);
}

void HexViewWidget::updateScrollbars()
{
    if (!m_data || m_size == 0) {
        verticalScrollBar()->setRange(0, 0);
        return;
    }

    qint64 totalLines = (m_size + m_bytesPerLine - 1) / m_bytesPerLine;
    int visibleLines = viewport()->height() / m_charHeight;

    verticalScrollBar()->setRange(0, qMax(0LL, totalLines - visibleLines));
    verticalScrollBar()->setPageStep(visibleLines);
}

qint64 HexViewWidget::positionAtPoint(const QPoint &point)
{
    int offsetWidth = 10 * m_charWidth;
    int hexWidth = (3 * m_bytesPerLine) * m_charWidth;
    int gapWidth = 2 * m_charWidth;
    int asciiX = offsetWidth + hexWidth + gapWidth;

    qint64 line = verticalScrollBar()->value() + point.y() / m_charHeight;

    if (point.x() >= offsetWidth && point.x() < offsetWidth + hexWidth) {
        int column = (point.x() - offsetWidth) / (3 * m_charWidth);
        m_focusArea = HexArea;
        return line * m_bytesPerLine + column;
    } else if (point.x() >= asciiX) {
        int column = (point.x() - asciiX) / m_charWidth;
        if (column < m_bytesPerLine) {
            m_focusArea = AsciiArea;
            return line * m_bytesPerLine + column;
        }
    }

    return -1;
}
