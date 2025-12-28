#include "ProcMountsManager.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <unistd.h>

namespace {
    const QSet<QString> excludedFsTypes = {
        "tmpfs", "devtmpfs", "sysfs", "proc", "securityfs", "cgroup2",
        "pstore", "efivarfs", "bpf", "configfs", "autofs", "tracefs",
        "debugfs", "mqueue", "hugetlbfs", "fusectl", "squashfs", "nsfs",
        "binfmt_misc", "devpts", "cgroup", "ramfs", "overlay"
    };

    QString unescapeMountPoint(const QString& escaped) {
        QString result = escaped;
        result.replace("\\040", " ");
        result.replace("\\011", "\t");
        result.replace("\\012", "\n");
        result.replace("\\134", "\\");
        return result;
    }
}

QString MountInfo::displayLabel() const {
    if (mountPoint.isEmpty())
        return device;

    // Extract last component of mount point as label
    QString label = mountPoint;
    if (label.endsWith('/'))
        label.chop(1);
    int lastSlash = label.lastIndexOf('/');
    if (lastSlash >= 0)
        label = label.mid(lastSlash + 1);

    return label.isEmpty() ? device : label;
}

ProcMountsManager::ProcMountsManager(QObject *parent)
    : QObject(parent)
{
}

bool ProcMountsManager::start()
{
    if (m_running)
        return true;

    // Watch /proc/mounts for changes
    m_procMountsWatcher = new QFileSystemWatcher(this);
    if (!m_procMountsWatcher->addPath("/proc/mounts")) {
        qWarning() << "Failed to watch /proc/mounts";
    }
    connect(m_procMountsWatcher, &QFileSystemWatcher::fileChanged,
            this, &ProcMountsManager::onProcMountsChanged);

    // Watch GVFS directory for changes
    QString gvfsPath = getGvfsPath();
    if (!gvfsPath.isEmpty() && QDir(gvfsPath).exists()) {
        m_gvfsWatcher = new QFileSystemWatcher(this);
        if (m_gvfsWatcher->addPath(gvfsPath)) {
            connect(m_gvfsWatcher, &QFileSystemWatcher::directoryChanged,
                    this, &ProcMountsManager::onGvfsDirectoryChanged);
        }
    }

    // Initial parse
    parseProcMounts();

    m_running = true;
    return true;
}

void ProcMountsManager::stop()
{
    if (!m_running)
        return;

    delete m_procMountsWatcher;
    m_procMountsWatcher = nullptr;

    delete m_gvfsWatcher;
    m_gvfsWatcher = nullptr;

    m_mounts.clear();
    m_running = false;
}

QList<MountInfo> ProcMountsManager::getMounts() const
{
    return m_mounts;
}

void ProcMountsManager::setUDisksMountPoints(const QSet<QString>& mountPoints)
{
    m_udisksMountPoints = mountPoints;
}

void ProcMountsManager::refresh()
{
    parseProcMounts();
}

void ProcMountsManager::onProcMountsChanged(const QString& path)
{
    Q_UNUSED(path)

    // Re-add path to watcher (QFileSystemWatcher may remove it after change)
    if (m_procMountsWatcher && !m_procMountsWatcher->files().contains("/proc/mounts")) {
        m_procMountsWatcher->addPath("/proc/mounts");
    }

    parseProcMounts();
    emit mountsChanged();
}

void ProcMountsManager::onGvfsDirectoryChanged(const QString& path)
{
    Q_UNUSED(path)

    // GVFS content changed - reparse to update visibility
    parseProcMounts();
    emit mountsChanged();
}

void ProcMountsManager::parseProcMounts()
{
    m_mounts.clear();

    QFile file("/proc/mounts");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open /proc/mounts";
        return;
    }

    // /proc/mounts is a virtual file with size 0, must use readAll()
    QByteArray content = file.readAll();
    file.close();

    QStringList lines = QString::fromUtf8(content).split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QStringList parts = line.split(' ');
        if (parts.size() < 4)
            continue;

        QString device = parts[0];
        QString mountPoint = unescapeMountPoint(parts[1]);
        QString fsType = parts[2];
        QString options = parts[3];

        if (!shouldShowMount(device, mountPoint, fsType))
            continue;

        // Skip if already in UDisks
        if (m_udisksMountPoints.contains(mountPoint))
            continue;

        MountInfo info;
        info.device = device;
        info.mountPoint = mountPoint;
        info.fsType = fsType;
        info.options = options;

        m_mounts.append(info);
    }
}

bool ProcMountsManager::shouldShowMount(const QString& device,
                                         const QString& mountPoint,
                                         const QString& fsType) const
{
    // Excluded filesystem types
    if (excludedFsTypes.contains(fsType))
        return false;

    // Excluded mount paths
    if (mountPoint.startsWith("/dev/") ||
        mountPoint.startsWith("/sys/") ||
        mountPoint.startsWith("/proc/") ||
        mountPoint.startsWith("/boot") ||
        mountPoint.startsWith("/run/credentials/") ||
        mountPoint.startsWith("/run/lock") ||
        mountPoint.startsWith("/run/snapd/") ||
        mountPoint.startsWith("/snap/") ||
        mountPoint.startsWith("/var/") ||
        mountPoint.startsWith("/usr/") ||
        mountPoint.startsWith("/home") ||
        mountPoint.startsWith("/root") ||
        mountPoint.startsWith("/srv") ||
        mountPoint.startsWith("/tmp") ||
        mountPoint == "/") {
        return false;
    }

    // Exclude ZFS system pools
    if (device.startsWith("bpool/") || device.startsWith("rpool/"))
        return false;

    // Exclude swap
    if (fsType == "swap")
        return false;

    // Exclude portal fuse
    if (fsType == "fuse.portal")
        return false;

    // GVFS - only show when non-empty
    if (fsType == "fuse.gvfsd-fuse") {
        QDir gvfsDir(mountPoint);
        QStringList entries = gvfsDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        return !entries.isEmpty();
    }

    return true;
}

QString ProcMountsManager::extractLabel(const QString& mountPoint) const
{
    if (mountPoint.isEmpty())
        return QString();

    QString label = mountPoint;
    if (label.endsWith('/'))
        label.chop(1);

    int lastSlash = label.lastIndexOf('/');
    if (lastSlash >= 0)
        label = label.mid(lastSlash + 1);

    return label;
}

QString ProcMountsManager::getGvfsPath() const
{
    QString uid = QString::number(getuid());
    return QString("/run/user/%1/gvfs").arg(uid);
}
