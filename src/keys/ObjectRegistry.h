#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QMutex>

// Global registry mapping QObject* → QString (human-readable names).
// Usage:
//   ObjectRegistry::add(this, "MainWindow");
//   QString name = ObjectRegistry::name(somePtr);
//
// Automatically removes objects on QObject::destroyed().
class ObjectRegistry : public QObject
{
    Q_OBJECT
public:
    // Access singleton instance
    static ObjectRegistry& instance();

    // Register object with a given name.
    // If the object is registered again, name is updated.
    static void add(QObject* obj, const QString& name)
    {
        instance().addInternal(obj, name);
    }

    // Alias
    static void reg(QObject* obj, const QString& name)
    {
        instance().addInternal(obj, name);
    }

    // Return object name or empty string if not registered
    static QString name(const QObject* obj)
    {
        return instance().nameInternal(obj);
    }

private:
    explicit ObjectRegistry(QObject* parent = nullptr);
    Q_DISABLE_COPY(ObjectRegistry)

    void addInternal(QObject* obj, const QString& name);
    QString nameInternal(const QObject* obj) const;
    void removeInternal(QObject* obj);

    QHash<const QObject*, QString> map_;
    mutable QMutex mutex_;

private slots:
    void onObjectDestroyed(QObject* obj);
};
