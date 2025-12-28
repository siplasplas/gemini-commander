#ifndef PROCMOUNTSMANAGER_H
#define PROCMOUNTSMANAGER_H

#include <QObject>
#include <QList>
#include <QFileSystemWatcher>
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

    bool start();
    void stop();

    QList<MountInfo> getMounts() const;

    void setUDisksMountPoints(const QSet<QString>& mountPoints);
    void refresh();

signals:
    void mountsChanged();

private slots:
    void onProcMountsChanged(const QString& path);
    void onGvfsDirectoryChanged(const QString& path);

private:
    void parseProcMounts();
    bool shouldShowMount(const QString& device, const QString& mountPoint, const QString& fsType) const;
    QString extractLabel(const QString& mountPoint) const;
    QString getGvfsPath() const;

    QFileSystemWatcher* m_procMountsWatcher = nullptr;
    QFileSystemWatcher* m_gvfsWatcher = nullptr;
    QList<MountInfo> m_mounts;
    QSet<QString> m_udisksMountPoints;
    bool m_running = false;
};

#endif // PROCMOUNTSMANAGER_H
