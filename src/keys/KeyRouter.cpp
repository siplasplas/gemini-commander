#include "KeyRouter.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QEvent>
#include <QKeyEvent>
#include <QMetaMethod>
#include <QWidget>

#include "KeyMap.h"
#include "ObjectRegistry.h"

KeyRouter & KeyRouter::instance()
{
    static KeyRouter inst;
    return inst;
}

KeyRouter::KeyRouter(QObject* parent)
    : QObject(parent)
{
}

void KeyRouter::installOn(QCoreApplication* app, QObject* owner)
{
    owner_ = owner;
    if (installed_ || !app)
        return;

    app->installEventFilter(this);
    installed_ = true;
}

void KeyRouter::handleNone(QObject *obj, QKeyEvent *keyEvent) {
    qDebug() << "return none";
    auto parentObj = obj->parent();
    if (parentObj) {
        // Create a copy of the key event for the parent
        QKeyEvent *eventCopy =
                new QKeyEvent(keyEvent->type(), keyEvent->key(), keyEvent->modifiers(), keyEvent->nativeScanCode(),
                              keyEvent->nativeVirtualKey(), keyEvent->nativeModifiers(), keyEvent->text(),
                              keyEvent->isAutoRepeat(), keyEvent->count());
        QCoreApplication::postEvent(parentObj, eventCopy);
    }
}

bool KeyRouter::handelWithHandler(QObject *obj, QKeyEvent *keyEvent, Qt::KeyboardModifiers mods, QString handlerName) {
    QObject *codeObj = obj;
    while (codeObj) {
        // Try to call the handler on 'current' with 'obj' as first parameter
        auto r = invokeHandler(codeObj, obj, handlerName, keyEvent);

        if (!r.called) {
            // Handler defined in keymap but not found on this object
            // Continue searching in parent
            codeObj = codeObj->parent();
            continue;
        }

        // Handler was called - consume the event
        return true;
    }
    return false;
}

bool KeyRouter::eventFilter(QObject* obj, QEvent* event)
{
    // Only process key press events
    if (!keyMap_ || event->type() != QEvent::KeyPress)
        return owner_->eventFilter(obj, event);

    auto* keyEvent = static_cast<QKeyEvent*>(event);

    // If operation is in progress, only allow ESC key (except for dialogs)
    if (m_operationInProgress) {
        // Check if there's an active modal dialog - if so, let it handle keys
        QWidget* modalWidget = QApplication::activeModalWidget();
        if (modalWidget && qobject_cast<QDialog*>(modalWidget)) {
            // Active modal dialog - let it handle keys (OK, Cancel, etc.)
            return owner_->eventFilter(obj, event);
        }

        if (keyEvent->key() == Qt::Key_Escape) {
            // Allow ESC to pass through for cancellation
        } else {
            // Block all other keys during operation
            return true;  // Consume the event
        }
    }

    Qt::KeyboardModifiers mods = keyEvent->modifiers();
    QString handlerName;
    QString widgetName;

    widgetName = ObjectRegistry::name(obj);
    QString widgetParent;
    if (widgetName.isEmpty()) {
        auto parentObj = obj->parent();
        while (parentObj) {
            widgetParent = ObjectRegistry::name(parentObj);
            if (!widgetParent.isEmpty()) {
                handlerName = keyMap_->handlerFor(keyEvent->key(), mods, widgetParent);
                if (!handlerName.isEmpty())
                    break;
            }
            parentObj = parentObj->parent();
        }
        if (parentObj && handlerName == "noneWithChildren") {
            handleNone(parentObj, keyEvent);
            return true;
        }
        return owner_->eventFilter(obj, event);
    }
    handlerName = keyMap_->handlerFor(keyEvent->key(), mods, widgetName);

    // "none" = handle in parent object
    if (handlerName == QStringLiteral("none")) {
        handleNone(obj, keyEvent);
        return true;
    }

    // "default" = allow Qt default behavior
    if (handlerName == QStringLiteral("default")) {
        return owner_->eventFilter(obj, event);
    }
    return handelWithHandler(obj, keyEvent, mods, handlerName);
}

HandlerCallResult KeyRouter::invokeHandler(QObject* target,
                                           QObject* eventSource,
                                           const QString& handler,
                                           QKeyEvent* ev)
{
    HandlerCallResult out{false, false};

    // Handler signature: bool handler(QObject* obj, QKeyEvent* keyEvent)
    QByteArray sig = handler.toLatin1() + "(QObject*,QKeyEvent*)";
    int idx = target->metaObject()->indexOfMethod(sig.constData());
    if (idx < 0) {
        return out;    // no handler
    }

    QMetaMethod m = target->metaObject()->method(idx);
    out.called = true;

    bool ret = false;
    m.invoke(target, Qt::DirectConnection,
             Q_RETURN_ARG(bool, ret),
             Q_ARG(QObject*, eventSource),
             Q_ARG(QKeyEvent*, ev));

    out.result = ret;
    return out;
}
