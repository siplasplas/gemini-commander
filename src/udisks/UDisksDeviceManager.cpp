#include "UDisksDeviceManager.h"

#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QRegularExpression>
#include <QDebug>

UDisksDeviceManager::UDisksDeviceManager(QObject *parent)
    : QObject(parent)
    , m_bus(QDBusConnection::systemBus())
    , m_running(false)
{
}

UDisksDeviceManager::~UDisksDeviceManager()
{
    stop();
}

bool UDisksDeviceManager::start()
{
    if (m_running) {
        return true;
    }

    if (!m_bus.isConnected()) {
        emit errorOccurred("start", "Cannot connect to system D-Bus");
        return false;
    }

    // Check if UDisks2 service is available
    QDBusInterface manager(UDISKS2_SERVICE, UDISKS2_MANAGER_PATH,
                           "org.freedesktop.UDisks2.Manager", m_bus);
    if (!manager.isValid()) {
        emit errorOccurred("start", "UDisks2 service is not available");
        return false;
    }

    // Connect to InterfacesAdded signal for new devices
    bool connected = m_bus.connect(
        UDISKS2_SERVICE,
        UDISKS2_PATH,
        DBUS_OBJECT_MANAGER,
        "InterfacesAdded",
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath, QMap<QString,QVariantMap>))
    );

    if (!connected) {
        emit errorOccurred("start", "Failed to connect to InterfacesAdded signal");
        return false;
    }

    // Connect to InterfacesRemoved signal for removed devices
    connected = m_bus.connect(
        UDISKS2_SERVICE,
        UDISKS2_PATH,
        DBUS_OBJECT_MANAGER,
        "InterfacesRemoved",
        this,
        SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList))
    );

    if (!connected) {
        emit errorOccurred("start", "Failed to connect to InterfacesRemoved signal");
        return false;
    }

    m_running = true;

    // Initial enumeration
    enumerateDevices();

    return true;
}

void UDisksDeviceManager::stop()
{
    if (!m_running) {
        return;
    }

    // Disconnect signals
    m_bus.disconnect(UDISKS2_SERVICE, UDISKS2_PATH, DBUS_OBJECT_MANAGER,
                     "InterfacesAdded", this,
                     SLOT(onInterfacesAdded(QDBusObjectPath, QMap<QString,QVariantMap>)));
    
    m_bus.disconnect(UDISKS2_SERVICE, UDISKS2_PATH, DBUS_OBJECT_MANAGER,
                     "InterfacesRemoved", this,
                     SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));

    m_devices.clear();
    m_running = false;
}

void UDisksDeviceManager::enumerateDevices()
{
    m_devices.clear();

    // Use introspection to find all block devices
    // The block devices are under /org/freedesktop/UDisks2/block_devices/
    QDBusMessage introspectCall = QDBusMessage::createMethodCall(
        UDISKS2_SERVICE,
        "/org/freedesktop/UDisks2/block_devices",
        "org.freedesktop.DBus.Introspectable",
        "Introspect"
    );

    QDBusMessage reply = m_bus.call(introspectCall);

    if (reply.type() == QDBusMessage::ErrorMessage) {
        emit errorOccurred("enumerate", reply.errorMessage());
        return;
    }

    if (reply.arguments().isEmpty()) {
        return;
    }

    // Parse the introspection XML to extract child node names
    QString xml = reply.arguments().first().toString();
    QStringList deviceNames;

    // Simple parsing: find all <node name="..."/> entries
    QRegularExpression nodeRegex("<node name=\"([^\"]+)\"");
    QRegularExpressionMatchIterator it = nodeRegex.globalMatch(xml);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        deviceNames.append(match.captured(1));
    }

    // Now query each device
    for (const QString &deviceName : deviceNames) {
        QString objectPath = "/org/freedesktop/UDisks2/block_devices/" + deviceName;

        BlockDeviceInfo info = buildDeviceInfo(objectPath);

        if (shouldShowDevice(info)) {
            m_devices.insert(objectPath, info);
            connectToDeviceSignals(objectPath);
        }
    }
}

void UDisksDeviceManager::refresh()
{
    enumerateDevices();
}

QVariantMap UDisksDeviceManager::getInterfaceProperties(const QString &objectPath, 
                                                         const QString &interface)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        UDISKS2_SERVICE,
        objectPath,
        DBUS_PROPERTIES,
        "GetAll"
    );
    call << interface;

    QDBusReply<QVariantMap> reply = m_bus.call(call);
    
    if (!reply.isValid()) {
        return QVariantMap();
    }

    return reply.value();
}

BlockDeviceInfo UDisksDeviceManager::buildDeviceInfo(const QString &objectPath)
{
    BlockDeviceInfo info;
    info.objectPath = objectPath;

    // Get Block interface properties
    QVariantMap blockProps = getInterfaceProperties(objectPath, BLOCK_INTERFACE);
    
    if (blockProps.isEmpty()) {
        return info;
    }

    // Device path (stored as byte array, needs conversion)
    QByteArray deviceBytes = blockProps.value("Device").toByteArray();
    info.device = QString::fromLatin1(deviceBytes).trimmed();
    // Remove null terminator if present
    info.device = info.device.replace(QChar('\0'), QString());

    info.size = blockProps.value("Size").toULongLong();

    // ID properties from Block interface
    info.label = blockProps.value("IdLabel").toString();
    info.uuid = blockProps.value("IdUUID").toString();
    info.fsType = blockProps.value("IdType").toString();

    // Get Filesystem interface properties (if available)
    QVariantMap fsProps = getInterfaceProperties(objectPath, FILESYSTEM_INTERFACE);

    if (!fsProps.isEmpty()) {
        // MountPoints is array of byte arrays - use QDBusArgument properly
        QVariant mountPointsVar = fsProps.value("MountPoints");

        if (mountPointsVar.canConvert<QDBusArgument>()) {
            const QDBusArgument arg = mountPointsVar.value<QDBusArgument>();

            if (arg.currentType() == QDBusArgument::ArrayType) {
                arg.beginArray();
                while (!arg.atEnd()) {
                    QByteArray mountBytes;
                    arg >> mountBytes;
                    if (!mountBytes.isEmpty()) {
                        // Remove null terminator and convert
                        int nullPos = mountBytes.indexOf('\0');
                        if (nullPos >= 0) {
                            mountBytes.truncate(nullPos);
                        }
                        QString mp = QString::fromUtf8(mountBytes);
                        if (!mp.isEmpty()) {
                            info.mountPoint = mp;
                            break; // Take first mount point
                        }
                    }
                }
                arg.endArray();
            }
        }
    }

    info.isMounted = !info.mountPoint.isEmpty();

    // Check if this is a partition
    QVariantMap partProps = getInterfaceProperties(objectPath, PARTITION_INTERFACE);
    info.isPartition = !partProps.isEmpty();

    // Get drive information
    QString drivePath = getDriveObjectPath(objectPath);
    if (!drivePath.isEmpty()) {
        QVariantMap driveProps = getInterfaceProperties(drivePath, DRIVE_INTERFACE);
        info.driveModel = driveProps.value("Model").toString();
        info.driveVendor = driveProps.value("Vendor").toString();
        info.isRemovable = driveProps.value("Removable").toBool() ||
                          driveProps.value("MediaRemovable").toBool();
    }

    return info;
}

QString UDisksDeviceManager::getDriveObjectPath(const QString &blockObjectPath)
{
    QVariantMap blockProps = getInterfaceProperties(blockObjectPath, BLOCK_INTERFACE);
    QDBusObjectPath drivePath = blockProps.value("Drive").value<QDBusObjectPath>();
    
    QString path = drivePath.path();
    
    // "/" means no associated drive
    if (path == "/" || path.isEmpty()) {
        return QString();
    }
    
    return path;
}

bool UDisksDeviceManager::shouldShowDevice(const BlockDeviceInfo &info) const
{
    // Skip devices without filesystem
    if (info.fsType.isEmpty()) {
        return false;
    }

    // Skip very small partitions (< 1MB) - likely boot sectors
    if (info.size < 1024 * 1024) {
        return false;
    }

    // Skip loop devices
    if (info.device.startsWith("/dev/loop")) {
        return false;
    }

    // Skip ram disks
    if (info.device.startsWith("/dev/ram") || info.device.startsWith("/dev/zram")) {
        return false;
    }

    // Skip swap partitions
    if (info.fsType == "swap") {
        return false;
    }

    // Skip ZFS system pools (bpool, rpool)
    if (info.label == "bpool" || info.label == "rpool") {
        return false;
    }

    return true;
}

void UDisksDeviceManager::connectToDeviceSignals(const QString &objectPath)
{
    // Connect to PropertiesChanged for this specific object
    m_bus.connect(
        UDISKS2_SERVICE,
        objectPath,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList))
    );
}

QMap<QString, BlockDeviceInfo> UDisksDeviceManager::getDevices(bool includeSystem) const
{
    if (includeSystem) {
        return m_devices;
    }

    // Filter out system partitions - keep removable and user-mounted drives
    QMap<QString, BlockDeviceInfo> filtered;

    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        const BlockDeviceInfo &info = it.value();

        // Skip root and common system mount points
        if (info.mountPoint == "/" ||
            info.mountPoint.startsWith("/boot") ||
            info.mountPoint.startsWith("/var") ||
            info.mountPoint.startsWith("/usr") ||
            info.mountPoint.startsWith("/snap")) {
            continue;
        }

        // Keep devices mounted in /media or /run/media (user mounts)
        // Keep unmounted devices (they're likely external)
        // Keep removable devices
        filtered.insert(it.key(), info);
    }
    return filtered;
}

BlockDeviceInfo UDisksDeviceManager::getDeviceInfo(const QString &objectPath) const
{
    return m_devices.value(objectPath);
}

BlockDeviceInfo UDisksDeviceManager::findDeviceByIdentifier(const QString &identifier) const
{
    for (const BlockDeviceInfo &info : m_devices) {
        if (info.label == identifier || info.uuid == identifier) {
            return info;
        }
    }
    return BlockDeviceInfo();
}

QString UDisksDeviceManager::mountDevice(const QString &objectPath, const QVariantMap &options)
{
    QDBusInterface filesystem(
        UDISKS2_SERVICE,
        objectPath,
        FILESYSTEM_INTERFACE,
        m_bus
    );

    if (!filesystem.isValid()) {
        emit errorOccurred("mount", QString("Device %1 does not support mounting").arg(objectPath));
        return QString();
    }

    // Prepare options - UDisks2 will mount to /media/<user>/<label|uuid> by default
    QVariantMap mountOptions = options;
    
    // Ensure proper encoding for non-ASCII characters on NTFS/exFAT
    BlockDeviceInfo info = m_devices.value(objectPath);
    if (info.fsType == "ntfs" || info.fsType == "exfat" || info.fsType == "vfat") {
        if (!mountOptions.contains("options")) {
            mountOptions["options"] = "utf8";
        }
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        UDISKS2_SERVICE,
        objectPath,
        FILESYSTEM_INTERFACE,
        "Mount"
    );
    call << mountOptions;

    QDBusReply<QString> reply = m_bus.call(call);

    if (!reply.isValid()) {
        QString errorMsg = reply.error().message();
        emit errorOccurred("mount", errorMsg);
        return QString();
    }

    QString mountPoint = reply.value();

    // Update local cache
    if (m_devices.contains(objectPath)) {
        m_devices[objectPath].mountPoint = mountPoint;
        m_devices[objectPath].isMounted = true;
        emit deviceMounted(objectPath, mountPoint);
    }

    return mountPoint;
}

bool UDisksDeviceManager::unmountDevice(const QString &objectPath, bool force)
{
    QDBusInterface filesystem(
        UDISKS2_SERVICE,
        objectPath,
        FILESYSTEM_INTERFACE,
        m_bus
    );

    if (!filesystem.isValid()) {
        emit errorOccurred("unmount", QString("Device %1 does not support unmounting").arg(objectPath));
        return false;
    }

    QVariantMap options;
    if (force) {
        options["force"] = true;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        UDISKS2_SERVICE,
        objectPath,
        FILESYSTEM_INTERFACE,
        "Unmount"
    );
    call << options;

    QDBusReply<void> reply = m_bus.call(call);

    if (!reply.isValid()) {
        emit errorOccurred("unmount", reply.error().message());
        return false;
    }

    // Update local cache
    if (m_devices.contains(objectPath)) {
        m_devices[objectPath].mountPoint.clear();
        m_devices[objectPath].isMounted = false;
        emit deviceUnmounted(objectPath);
    }

    return true;
}

bool UDisksDeviceManager::ejectDrive(const QString &objectPath)
{
    QString drivePath = getDriveObjectPath(objectPath);
    
    if (drivePath.isEmpty()) {
        emit errorOccurred("eject", "Cannot find drive for device");
        return false;
    }

    // First unmount all partitions on this drive
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        if (getDriveObjectPath(it.key()) == drivePath && it.value().isMounted) {
            if (!unmountDevice(it.key())) {
                return false; // Failed to unmount, abort eject
            }
        }
    }

    // Now power off the drive
    QDBusInterface drive(
        UDISKS2_SERVICE,
        drivePath,
        DRIVE_INTERFACE,
        m_bus
    );

    if (!drive.isValid()) {
        emit errorOccurred("eject", "Cannot access drive interface");
        return false;
    }

    QVariantMap options;
    QDBusMessage call = QDBusMessage::createMethodCall(
        UDISKS2_SERVICE,
        drivePath,
        DRIVE_INTERFACE,
        "PowerOff"
    );
    call << options;

    QDBusReply<void> reply = m_bus.call(call);

    if (!reply.isValid()) {
        // PowerOff might not be supported, try Eject instead
        call = QDBusMessage::createMethodCall(
            UDISKS2_SERVICE,
            drivePath,
            DRIVE_INTERFACE,
            "Eject"
        );
        call << options;
        reply = m_bus.call(call);
        
        if (!reply.isValid()) {
            emit errorOccurred("eject", reply.error().message());
            return false;
        }
    }

    return true;
}

void UDisksDeviceManager::onInterfacesAdded(const QDBusObjectPath &objectPath,
                                            const QMap<QString, QVariantMap> &interfaces)
{
    const QString path = objectPath.path();

    // Check if this is a block device with filesystem
    if (!interfaces.contains(BLOCK_INTERFACE)) {
        return;
    }

    BlockDeviceInfo info = buildDeviceInfo(path);

    if (!shouldShowDevice(info)) {
        return;
    }

    m_devices.insert(path, info);
    connectToDeviceSignals(path);

    emit deviceAdded(info);
}

void UDisksDeviceManager::onInterfacesRemoved(const QDBusObjectPath &objectPath,
                                              const QStringList &interfaces)
{
    const QString path = objectPath.path();

    if (!interfaces.contains(BLOCK_INTERFACE)) {
        return;
    }

    if (m_devices.contains(path)) {
        QString displayId = m_devices[path].displayId();
        m_devices.remove(path);
        emit deviceRemoved(path, displayId);
    }
}

void UDisksDeviceManager::onPropertiesChanged(const QString &interface,
                                              const QVariantMap &changedProperties,
                                              const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties)

    // We're mainly interested in Filesystem.MountPoints changes
    if (interface != FILESYSTEM_INTERFACE) {
        return;
    }

    // Find which device this signal belongs to
    // The sender path contains the object path
    QString senderPath;
    
    // Try to get sender from message context
    if (QDBusMessage *msg = const_cast<QDBusMessage*>(
            reinterpret_cast<const QDBusMessage*>(sender()))) {
        senderPath = msg->path();
    }

    // Alternative: search for device with changed mount state
    if (senderPath.isEmpty() || !m_devices.contains(senderPath)) {
        // Refresh all devices to catch the change
        for (const QString &path : m_devices.keys()) {
            BlockDeviceInfo newInfo = buildDeviceInfo(path);
            if (newInfo.isMounted != m_devices[path].isMounted ||
                newInfo.mountPoint != m_devices[path].mountPoint) {
                
                bool wasMounted = m_devices[path].isMounted;
                m_devices[path] = newInfo;
                
                if (newInfo.isMounted && !wasMounted) {
                    emit deviceMounted(path, newInfo.mountPoint);
                } else if (!newInfo.isMounted && wasMounted) {
                    emit deviceUnmounted(path);
                }
                
                emit deviceChanged(newInfo);
            }
        }
        return;
    }

    // Update specific device
    BlockDeviceInfo newInfo = buildDeviceInfo(senderPath);
    bool wasMounted = m_devices[senderPath].isMounted;
    m_devices[senderPath] = newInfo;

    if (newInfo.isMounted && !wasMounted) {
        emit deviceMounted(senderPath, newInfo.mountPoint);
    } else if (!newInfo.isMounted && wasMounted) {
        emit deviceUnmounted(senderPath);
    }

    emit deviceChanged(newInfo);
}
