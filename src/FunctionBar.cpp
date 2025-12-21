#include "FunctionBar.h"

#include <QHBoxLayout>
#include <QPushButton>

FunctionBar::FunctionBar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

QPushButton* FunctionBar::createButton(const QString& label)
{
    auto* btn = new QPushButton(label, this);
    btn->setFlat(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setStyleSheet(
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
    m_buttons.append(btn);
    return btn;
}

void FunctionBar::setupUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);

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
        layout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, def.signal);
    }

    setFixedHeight(24);
}