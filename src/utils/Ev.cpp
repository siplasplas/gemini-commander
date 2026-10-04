// Ev.cpp
#include "Ev.h"
#include <QMouseEvent>
#include <QHoverEvent>
#include <QWheelEvent>
#include <QDebug>
#include <cassert>

namespace {
    template<typename T>
    QPoint getLocalPosImpl(T* event) {
        return event->position().toPoint();
    }
} // namespace

QPoint Ev::global(QEvent* e) {
    assert(e && "Null event in Ev::global");
    auto* mouseEvent = dynamic_cast<QMouseEvent*>(e);
    if (!mouseEvent) {
        qCritical("Ev::global requires QMouseEvent");
        return QPoint();
    }
    return mouseEvent->globalPosition().toPoint();
}

QPoint Ev::local(QEvent* e) {
    assert(e && "Null event in Ev::local");

    switch(e->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
        return getLocalPosImpl(dynamic_cast<QMouseEvent*>(e));

    case QEvent::HoverMove:
        return getLocalPosImpl(dynamic_cast<QHoverEvent*>(e));

    case QEvent::Wheel:
        return getLocalPosImpl(dynamic_cast<QWheelEvent*>(e));

    default:
        qWarning("Unsupported event type: %d", e->type());
        return QPoint();
    }
}