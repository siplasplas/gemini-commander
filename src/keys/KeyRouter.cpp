#include "KeyRouter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QMetaMethod>

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

bool KeyRouter::eventFilter(QObject* obj, QEvent* event)
{
    // Only process key press events
    if (!keyMap_ || event->type() != QEvent::KeyPress)
        return owner_->eventFilter(obj, event);
    qDebug()<<obj;
    auto* keyEvent = static_cast<QKeyEvent*>(event);
    Qt::KeyboardModifiers mods = keyEvent->modifiers();
    if (keyEvent->key()==Qt::Key_PageUp)
        qDebug() << "Key_Tab";

    QObject* dataObj = obj;
    QString handlerName;
    QString widgetName;
    while (dataObj) {
        widgetName = ObjectRegistry::name(dataObj);
        if (!widgetName.isEmpty()) {
            handlerName = keyMap_->handlerFor(keyEvent->key(), mods, widgetName);
            if (!handlerName.isEmpty())
                break;
        }
        dataObj = dataObj->parent();
    }
    if (!dataObj || handlerName.isEmpty())
        return QObject::eventFilter(obj, event);

    // "none" = consume event, do nothing
    if (handlerName == QStringLiteral("none")) {
        return true;
    }

    // "default" = allow Qt default behavior
    if (handlerName == QStringLiteral("default")) {
        return false;
    }

    QObject* codeObj = dataObj;
    while (codeObj) {
        // Try to call the handler on 'current' with 'obj' as first parameter
        auto r = invokeHandler(codeObj, dataObj, handlerName, keyEvent);

        if (!r.called) {
            // Handler defined in keymap but not found on this object
            // Continue searching in parent
            codeObj = codeObj->parent();
            continue;
        }

        qDebug()
            << "[KeyRouter]"
            << "event_obj =" << ObjectRegistry::name(obj)
            << "handler_obj =" << widgetName
            << "key =" << keyEvent->key()
            << "mods =" << mods
            << "handler =" << handlerName
            << "result =" << r.result;

        // Handler was called - consume the event
        return true;
    }

    // No binding found in hierarchy - allow Qt to process normally
    return QObject::eventFilter(obj, event);
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
