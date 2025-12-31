#include "VerticalToolButton.h"
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QToolBar>
#include <QTimer>

VerticalToolButton::VerticalToolButton(QWidget* parent)
    : QToolButton(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    connectToToolbar();
}

VerticalToolButton::VerticalToolButton(const QString& text, QWidget* parent)
    : QToolButton(parent)
{
    setText(text);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    connectToToolbar();
}

void VerticalToolButton::connectToToolbar()
{
    // Defer connection until we're added to a toolbar
    QTimer::singleShot(0, this, [this]() {
        if (QToolBar* toolbar = qobject_cast<QToolBar*>(parentWidget())) {
            connect(toolbar, &QToolBar::orientationChanged, this, [this]() {
                updateGeometry();
                update();
            });
        }
    });
}

Qt::Orientation VerticalToolButton::orientation() const
{
    QToolBar* toolbar = qobject_cast<QToolBar*>(parentWidget());
    if (toolbar) {
        return toolbar->orientation();
    }
    return Qt::Horizontal;
}

QSize VerticalToolButton::sizeHint() const
{
    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(text()) + 12;  // padding
    int textHeight = fm.height() + 8;

    if (orientation() == Qt::Vertical) {
        // Swap width and height for vertical orientation
        return QSize(textHeight, textWidth);
    }
    return QSize(textWidth, textHeight);
}

QSize VerticalToolButton::minimumSizeHint() const
{
    return sizeHint();
}

void VerticalToolButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Get style options
    QStyleOptionToolButton opt;
    initStyleOption(&opt);

    // Draw button background/frame
    bool isHovered = opt.state & QStyle::State_MouseOver;
    bool isPressed = opt.state & QStyle::State_Sunken;

    if (isPressed) {
        painter.fillRect(rect(), palette().dark());
    } else if (isHovered) {
        painter.fillRect(rect(), palette().midlight());
    }

    // Draw text
    painter.setPen(palette().buttonText().color());

    if (orientation() == Qt::Vertical) {
        // Rotate text 90° counter-clockwise (reads bottom-to-top)
        painter.save();
        painter.translate(0, height());
        painter.rotate(-90);

        QRect textRect(0, 0, height(), width());
        painter.drawText(textRect, Qt::AlignCenter, text());

        painter.restore();
    } else {
        painter.drawText(rect(), Qt::AlignCenter, text());
    }
}

// VerticalToolButtonAction implementation

VerticalToolButtonAction::VerticalToolButtonAction(QObject* parent)
    : QWidgetAction(parent)
{
}

VerticalToolButtonAction::VerticalToolButtonAction(const QString& text, QObject* parent)
    : QWidgetAction(parent)
{
    setText(text);
}

QWidget* VerticalToolButtonAction::createWidget(QWidget* parent)
{
    VerticalToolButton* btn = new VerticalToolButton(text(), parent);
    btn->setToolTip(toolTip());

    // Connect button click to action trigger
    connect(btn, &QToolButton::clicked, this, &QAction::trigger);

    // Update button when action changes
    connect(this, &QAction::changed, btn, [this, btn]() {
        btn->setText(text());
        btn->setToolTip(toolTip());
        btn->setEnabled(isEnabled());

        // Update font if action has italic font (for unmounted drives)
        QFont f = btn->font();
        f.setItalic(font().italic());
        btn->setFont(f);
    });

    return btn;
}

// VerticalLabel implementation

VerticalLabel::VerticalLabel(QWidget* parent)
    : QWidget(parent)
{
    connectToToolbar();
}

VerticalLabel::VerticalLabel(const QString& text, QWidget* parent)
    : QWidget(parent)
    , m_text(text)
{
    connectToToolbar();
}

void VerticalLabel::setText(const QString& text)
{
    m_text = text;
    updateGeometry();
    update();
}

void VerticalLabel::connectToToolbar()
{
    // Defer connection until we're added to a toolbar
    QTimer::singleShot(0, this, [this]() {
        if (QToolBar* toolbar = qobject_cast<QToolBar*>(parentWidget())) {
            connect(toolbar, &QToolBar::orientationChanged, this, [this]() {
                updateGeometry();
                update();
            });
        }
    });
}

Qt::Orientation VerticalLabel::orientation() const
{
    QToolBar* toolbar = qobject_cast<QToolBar*>(parentWidget());
    if (toolbar) {
        return toolbar->orientation();
    }
    return Qt::Horizontal;
}

QSize VerticalLabel::sizeHint() const
{
    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(m_text) + 12;  // padding
    int textHeight = fm.height() + 4;

    if (orientation() == Qt::Vertical) {
        return QSize(textHeight, textWidth);
    }
    return QSize(textWidth, textHeight);
}

QSize VerticalLabel::minimumSizeHint() const
{
    return sizeHint();
}

void VerticalLabel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(palette().windowText().color());

    if (orientation() == Qt::Vertical) {
        painter.save();
        painter.translate(0, height());
        painter.rotate(-90);

        QRect textRect(0, 0, height(), width());
        painter.drawText(textRect, Qt::AlignCenter, m_text);

        painter.restore();
    } else {
        painter.drawText(rect(), Qt::AlignCenter, m_text);
    }
}