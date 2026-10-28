#pragma once

#include <QDBusAbstractAdaptor>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class MainWindow;

/**
 * @class DBusService
 * @brief Lets other programs (e.g. ducat) open a directory in gemini-commander
 *
 * On the session bus each instance owns `io.github.siplasplas.GeminiCommander.p<PID>`
 * and takes over `io.github.siplasplas.GeminiCommander` when it starts and
 * whenever its main window is activated, so the shared name always reaches
 * the instance used last. The object `/io/github/siplasplas/GeminiCommander`
 * implements org.freedesktop.Application:
 *
 *   Open(as uris, a{sv} platform_data): the window is activated and the first
 *       file:// URI or path goes to the current panel (a directory is
 *       entered; for a file its directory is, with the file selected) - once
 *       no modal dialog is open and no operation runs.
 *   Activate(a{sv} platform_data): activates the window.
 *
 * platform_data["activation-token"] (xdg-activation, Wayland) lets the window
 * come to the front; without it GNOME only notifies that it is ready.
 */
class DBusService : public QObject {
    Q_OBJECT
public:
    static constexpr const char* kServiceName = "io.github.siplasplas.GeminiCommander";
    static constexpr const char* kObjectPath = "/io/github/siplasplas/GeminiCommander";

    explicit DBusService(MainWindow* window);

    void open(const QStringList& uris, const QVariantMap& platformData);
    void activate(const QVariantMap& platformData);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void claimSharedName();
    // Opens m_pendingPath unless a modal dialog is open or an operation runs;
    // then it tries again later.
    void openPendingWhenIdle();

    MainWindow* m_window;
    bool m_registered = false;
    QString m_pendingPath;
    QTimer m_retryTimer;
};

// org.freedesktop.Application on DBusService.
class ApplicationAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Application")
public:
    explicit ApplicationAdaptor(DBusService* service);

public slots:
    void Activate(const QVariantMap& platform_data);
    void Open(const QStringList& uris, const QVariantMap& platform_data);
    void ActivateAction(const QString& action_name, const QVariantList& parameter,
                        const QVariantMap& platform_data);

private:
    DBusService* m_service;
};
