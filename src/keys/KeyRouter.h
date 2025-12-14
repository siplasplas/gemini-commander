#pragma once

#include <QObject>
#include <qevent.h>

class QCoreApplication;
class KeyMap;

struct HandlerCallResult {
    bool called;   // handler exists and was invoked
    bool result;   // handler returned true/false
};

// Central global key routing object.
// Install once on QCoreApplication:
//
//   KeyRouter::instance().setKeyMap(&keyMap);
//   KeyRouter::instance().installOn(qApp);
//
// It receives all key events from the entire application.
class KeyRouter : public QObject
{
    Q_OBJECT
public:
    static KeyRouter& instance();

    // Install event filter on application (only once)
    void installOn(QCoreApplication* app, QObject* owner);

    // Assign key map that resolves (widget, key, modifiers) → handler
    void setKeyMap(KeyMap* map) { keyMap_ = map; }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    HandlerCallResult invokeHandler(QObject* target, QObject* eventSource,
                                    const QString& handler, QKeyEvent* ev);

private:
    explicit KeyRouter(QObject* parent = nullptr);
    Q_DISABLE_COPY(KeyRouter)

    QObject* owner_ = nullptr;
    bool installed_ = false;
    KeyMap* keyMap_ = nullptr;
};
