#include "DBusService.h"

#include "MainWindow.h"
#include "keys/KeyRouter.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusVariant>
#include <QEvent>
#include <QUrl>

namespace {

// a{sv} values may arrive wrapped in QDBusVariant.
QString stringValue(const QVariantMap& map, const QString& key)
{
    QVariant value = map.value(key);
    if (value.canConvert<QDBusVariant>())
        value = qvariant_cast<QDBusVariant>(value).variant();
    return value.toString();
}

// A file:// URI or a plain path.
QString localPath(const QString& uri)
{
    const QUrl url(uri);
    if (url.isLocalFile())
        return url.toLocalFile();
    return uri;
}

} // namespace

DBusService::DBusService(MainWindow* window)
    : QObject(window)
    , m_window(window)
{
    m_retryTimer.setInterval(300);
    connect(&m_retryTimer, &QTimer::timeout, this, &DBusService::openPendingWhenIdle);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    new ApplicationAdaptor(this);
    if (!bus.registerObject(QString::fromLatin1(kObjectPath), this))
        return;
    m_registered = true;
    // This instance's own name, to find it when the shared one is free.
    bus.registerService(QStringLiteral("%1.p%2").arg(QLatin1String(kServiceName))
                            .arg(QCoreApplication::applicationPid()));
    claimSharedName();
    m_window->installEventFilter(this);
}

void DBusService::claimSharedName()
{
    if (!m_registered)
        return;
    // The last activated instance owns it; the previous owner just loses it.
    QDBusConnection::sessionBus().interface()->registerService(
        QString::fromLatin1(kServiceName),
        QDBusConnectionInterface::ReplaceExistingService,
        QDBusConnectionInterface::AllowReplacement);
}

bool DBusService::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window && event->type() == QEvent::ActivationChange
        && m_window->isActiveWindow())
        claimSharedName();
    return QObject::eventFilter(watched, event);
}

void DBusService::open(const QStringList& uris, const QVariantMap& platformData)
{
    for (const QString& uri : uris) {
        const QString path = localPath(uri);
        if (!path.isEmpty()) {
            m_pendingPath = path;  // one panel, one path; a later call wins
            break;
        }
    }
    // At once, so that a token is used and an open dialog is seen.
    activate(platformData);
    openPendingWhenIdle();
}

void DBusService::openPendingWhenIdle()
{
    if (m_pendingPath.isEmpty())
        return;
    // Not under a modal dialog (Copy, Overwrite?, ...) nor during an operation
    // (copying, directory sizes): it works on the panel's entries, which
    // loading another directory would replace. Tried again until idle.
    if (QApplication::activeModalWidget() || KeyRouter::instance().isOperationInProgress()) {
        if (!m_retryTimer.isActive())
            m_retryTimer.start();
        return;
    }
    m_retryTimer.stop();
    const QString path = m_pendingPath;
    m_pendingPath.clear();
    m_window->openInCurrentPanel(path);
}

void DBusService::activate(const QVariantMap& platformData)
{
    if (m_window->isMinimized())
        m_window->showNormal();
    else if (!m_window->isVisible())
        m_window->show();
    const QString token = stringValue(platformData, QStringLiteral("activation-token"));
    if (!token.isEmpty()) {
        // QtWayland's xdg-shell activates with XDG_ACTIVATION_TOKEN when it is
        // set (and clears it). Used even when the window is active: a token
        // left unused keeps GNOME's busy cursor of the caller for ~15 s.
        qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
        m_window->raise();
        m_window->activateWindow();
        return;
    }
    // Without a token: enough on X11; on Wayland GNOME shows "is ready".
    if (!m_window->isActiveWindow()) {
        m_window->raise();
        m_window->activateWindow();
    }
}

ApplicationAdaptor::ApplicationAdaptor(DBusService* service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

void ApplicationAdaptor::Activate(const QVariantMap& platform_data)
{
    m_service->activate(platform_data);
}

void ApplicationAdaptor::Open(const QStringList& uris, const QVariantMap& platform_data)
{
    m_service->open(uris, platform_data);
}

void ApplicationAdaptor::ActivateAction(const QString& action_name, const QVariantList& parameter,
                                        const QVariantMap& platform_data)
{
    Q_UNUSED(action_name);
    Q_UNUSED(parameter);
    // No actions; the window is activated, which also uses up a token.
    m_service->activate(platform_data);
}
