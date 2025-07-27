// Ev.h
#pragma once
#include <QEvent>
#include <QPoint>

namespace Ev {
    QPoint global(QEvent* e);  // Only for mouse events (global pos)
    QPoint local(QEvent* e);   // For all events with a local position
}