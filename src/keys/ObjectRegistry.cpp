#include "ObjectRegistry.h"
#include <QMutexLocker>

ObjectRegistry& ObjectRegistry::instance()
{
    static ObjectRegistry inst;
    return inst;
}

ObjectRegistry::ObjectRegistry(QObject* parent)
    : QObject(parent)
{
}

void ObjectRegistry::addInternal(QObject* obj, const QString& name)
{
    if (!obj)
        return;

    {
        QMutexLocker locker(&mutex_);
        map_[obj] = name;
    }

    // Auto-remove when destroyed
    QObject::connect(
        obj,
        &QObject::destroyed,
        this,
        &ObjectRegistry::onObjectDestroyed,
        Qt::UniqueConnection
    );
}

QString ObjectRegistry::nameInternal(const QObject* obj) const
{
    if (!obj)
        return {};

    QMutexLocker locker(&mutex_);
    auto it = map_.find(obj);
    if (it != map_.end())
        return it.value();

    return {};
}

void ObjectRegistry::removeInternal(QObject* obj)
{
    QMutexLocker locker(&mutex_);
    map_.remove(obj);
}

void ObjectRegistry::onObjectDestroyed(QObject* obj)
{
    removeInternal(obj);
}
