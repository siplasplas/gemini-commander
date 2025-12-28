#ifndef PROCMOUNTSMANAGER_H
#define PROCMOUNTSMANAGER_H

#ifndef _WIN32

#include <QObject>
#include <QList>
#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QSet>

struct MountInfo {
    QString device;      // np. /dev/loop11, vbshare
    QString mountPoint;  // np. /media/andrzej/VBox_GAs_7.2.2
    QString fsType;      // np. iso9660, vboxsf
    QString options;     // opcje montowania

    QString displayLabel() const;
};

class ProcMountsManager : public QObject
{
    Q_OBJECT
public:
    explicit ProcMountsManager(QObject *parent = nullptr);
    ~ProcMountsManager();

    bool start();
    void stop();

    QList<MountInfo> getMounts() const;

    void setUDisksMountPoints(const QSet<QString>& mountPoints);
    void refresh();

signals:
    void mountsChanged();

private slots:
    void onNetlinkEvent();
    void onGvfsDirectoryChanged(const QString& path);

private:
    bool setupNetlinkSocket();
    void parseProcMounts();
    bool shouldShowMount(const QString& device, const QString& mountPoint, const QString& fsType) const;
    QString extractLabel(const QString& mountPoint) const;
    QString getGvfsPath() const;

    int m_netlinkFd = -1;
    QSocketNotifier* m_netlinkNotifier = nullptr;
    QFileSystemWatcher* m_gvfsWatcher = nullptr;
    QList<MountInfo> m_mounts;
    QSet<QString> m_udisksMountPoints;
    bool m_running = false;
};

#endif // _WIN32

#endif // PROCMOUNTSMANAGER_H
