#include "ProcMountsManager.h"

#ifndef _WIN32

#include <QFile>
#include <QDir>
#include <QDebug>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

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

ProcMountsManager::~ProcMountsManager()
{
    stop();
}

bool ProcMountsManager::setupNetlinkSocket()
{
    // Create netlink socket for kernel events
    m_netlinkFd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                         NETLINK_KOBJECT_UEVENT);
    if (m_netlinkFd < 0) {
        qWarning() << "Failed to create netlink socket:" << strerror(errno);
        return false;
    }

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 1; // Kernel events multicast group

    if (bind(m_netlinkFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        qWarning() << "Failed to bind netlink socket:" << strerror(errno);
        close(m_netlinkFd);
        m_netlinkFd = -1;
        return false;
    }

    // Create QSocketNotifier to integrate with Qt event loop
    m_netlinkNotifier = new QSocketNotifier(m_netlinkFd, QSocketNotifier::Read, this);
    connect(m_netlinkNotifier, &QSocketNotifier::activated,
            this, &ProcMountsManager::onNetlinkEvent);

    return true;
}

bool ProcMountsManager::start()
{
    if (m_running)
        return true;

    // Setup netlink socket for mount events
    if (!setupNetlinkSocket()) {
        qWarning() << "Netlink socket setup failed, mount changes won't be detected automatically";
    }

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

    if (m_netlinkNotifier) {
        delete m_netlinkNotifier;
        m_netlinkNotifier = nullptr;
    }

    if (m_netlinkFd >= 0) {
        close(m_netlinkFd);
        m_netlinkFd = -1;
    }

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

void ProcMountsManager::onNetlinkEvent()
{
    // Read and discard the netlink message
    char buffer[4096];
    while (recv(m_netlinkFd, buffer, sizeof(buffer), MSG_DONTWAIT) > 0) {
        // Check if this is a block device event (mount/unmount related)
        // Events contain strings like "ACTION=add", "SUBSYSTEM=block", etc.
        QString event = QString::fromUtf8(buffer);
        if (event.contains("SUBSYSTEM=block") ||
            event.contains("mount") ||
            event.contains("loop")) {
            // Mount-related event detected
            emit mountsChanged();
            return;
        }
    }
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

#endif // _WIN32
