#include "KeyRouter.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
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

    // Skip routing for modal dialogs (QInputDialog, QMessageBox, etc.)
    // Let them handle their own key events
    QWidget* modalWidget = QApplication::activeModalWidget();
    if (modalWidget && qobject_cast<QDialog*>(modalWidget)) {
        // Check if event source is inside the modal dialog
        QObject* check = obj;
        while (check) {
            if (check == modalWidget) {
                // Event is for modal dialog - let it handle natively
                return false;
            }
            check = check->parent();
        }
    }

    qDebug() << "-------------------";
    qDebug()<<obj;
    qDebug()<<"name obj = " << ObjectRegistry::name(obj);
    auto* keyEvent = static_cast<QKeyEvent*>(event);
    qDebug() << "key =" << keyEvent->key();
    Qt::KeyboardModifiers mods = keyEvent->modifiers();
    QObject* dataObj = obj;
    QString handlerName;
    QString widgetName;
    while (dataObj) {
        widgetName = ObjectRegistry::name(dataObj);
        qDebug() << "  walk:" << dataObj << "name=" << widgetName;
        if (!widgetName.isEmpty()) {
            handlerName = keyMap_->handlerFor(keyEvent->key(), mods, widgetName);
            qDebug() << "    handlerFor(" << keyEvent->key() << "," << mods << "," << widgetName << ") =" << handlerName;
            if (!handlerName.isEmpty())
                break;
        }
        dataObj = dataObj->parent();
    }
    if (!dataObj || handlerName.isEmpty())
        return owner_->eventFilter(obj, event);

    // "none" = consume event, do nothing
    if (handlerName == QStringLiteral("none")) {
        qDebug() << "return none";
        return true;
    }

    // "default" = allow Qt default behavior
    if (handlerName == QStringLiteral("default")) {
        qDebug() << "return default";
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

    // No binding found in hierarchy - delegate to owner
    return owner_->eventFilter(obj, event);
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
