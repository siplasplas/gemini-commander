#include "FunctionBar.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QStyleOptionButton>
#include <QToolBar>

// FunctionButton implementation

FunctionButton::FunctionButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
{
    setFlat(true);
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet(
        "QPushButton {"
        "  background-color: #4a90a4;"
        "  color: white;"
        "  border: 1px solid #3a7a94;"
        "  padding: 2px 4px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #5aa0b4;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #3a8094;"
        "}"
    );
}

void FunctionButton::setVertical(bool vertical)
{
    if (m_vertical != vertical) {
        m_vertical = vertical;
        updateGeometry();
        update();
    }
}

QSize FunctionButton::sizeHint() const
{
    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(text()) + 8;
    int textHeight = fm.height() + 6;

    if (m_vertical) {
        return QSize(textHeight, textWidth);
    }
    return QSize(textWidth, textHeight);
}

QSize FunctionButton::minimumSizeHint() const
{
    return sizeHint();
}

void FunctionButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Get style options for hover/pressed state
    QStyleOptionButton opt;
    initStyleOption(&opt);

    bool isHovered = opt.state & QStyle::State_MouseOver;
    bool isPressed = opt.state & QStyle::State_Sunken;

    // Draw background
    QColor bgColor("#4a90a4");
    if (isPressed) {
        bgColor = QColor("#3a8094");
    } else if (isHovered) {
        bgColor = QColor("#5aa0b4");
    }

    painter.fillRect(rect(), bgColor);

    // Draw border
    painter.setPen(QColor("#3a7a94"));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    // Draw text
    painter.setPen(Qt::white);

    if (m_vertical) {
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

// FunctionBar implementation

FunctionBar::FunctionBar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

FunctionButton* FunctionBar::createButton(const QString& label)
{
    auto* btn = new FunctionButton(label, this);
    m_buttons.append(btn);
    return btn;
}

void FunctionBar::setupUi()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(1);

    struct ButtonDef {
        QString label;
        void (FunctionBar::*signal)();
    };

    const ButtonDef buttons[] = {
        {"F3 View",      &FunctionBar::viewClicked},
        {"F4 Edit",      &FunctionBar::editClicked},
        {"F5 Copy",      &FunctionBar::copyClicked},
        {"F6 Move",      &FunctionBar::moveClicked},
        {"F7 Mkdir",     &FunctionBar::mkdirClicked},
        {"F8 Delete",    &FunctionBar::deleteClicked},
        {"F9 Terminal",  &FunctionBar::terminalClicked},
        {"Alt+F4 Exit",  &FunctionBar::exitClicked},
    };

    for (const auto& def : buttons) {
        auto* btn = createButton(def.label);
        m_layout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, def.signal);
    }

    updateLayout();
}

void FunctionBar::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation != orientation) {
        m_orientation = orientation;
        updateLayout();
    }
}

void FunctionBar::updateLayout()
{
    // Update button orientations
    bool vertical = (m_orientation == Qt::Vertical);
    for (auto* btn : m_buttons) {
        btn->setVertical(vertical);
        if (vertical) {
            btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        } else {
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    }

    // Change layout direction
    if (vertical) {
        m_layout->setDirection(QBoxLayout::TopToBottom);
        setMinimumWidth(0);
        setMaximumWidth(QWIDGETSIZE_MAX);
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        m_layout->setDirection(QBoxLayout::LeftToRight);
        QFontMetrics fm(font());
        int h = fm.height() + 8;
        setFixedHeight(h);
        setMinimumWidth(0);
        setMaximumWidth(QWIDGETSIZE_MAX);
    }

    updateGeometry();
}
