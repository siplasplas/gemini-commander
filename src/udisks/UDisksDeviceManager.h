#ifndef UDISKSDEVICEMANAGER_H
#define UDISKSDEVICEMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>

/**
 * @brief Information about a block device detected via UDisks2
 */
struct BlockDeviceInfo {
    QString objectPath;      // D-Bus object path (e.g., /org/freedesktop/UDisks2/block_devices/sdb1)
    QString device;          // Device path (e.g., /dev/sdb1)
    QString label;           // Filesystem label (may be empty)
    QString uuid;            // Filesystem UUID
    QString fsType;          // Filesystem type (ext4, ntfs, exfat, etc.)
    QString mountPoint;      // Current mount point (empty if not mounted)
    quint64 size;            // Size in bytes
    bool isPartition;        // True if this is a partition
    bool isMounted;          // True if currently mounted
    bool isRemovable;        // True if on removable media
    QString driveModel;      // Drive model name
    QString driveVendor;     // Drive vendor name

    /**
     * @brief Returns display identifier: label if available, otherwise UUID
     */
    QString displayId() const {
        return label.isEmpty() ? uuid : label;
    }

    /**
     * @brief Returns unique identifier (always UUID for consistency)
     */
    QString uniqueId() const {
        return uuid;
    }
};

/**
 * @brief Manager class for detecting and controlling block devices via UDisks2 D-Bus interface
 * 
 * This class provides:
 * - Detection of all block devices (mounted and unmounted)
 * - Real-time notifications when devices are added/removed
 * - Mount/unmount operations
 * - Device information retrieval
 * 
 * Usage:
 *   UDisksDeviceManager manager;
 *   connect(&manager, &UDisksDeviceManager::deviceAdded, this, &MyClass::onDeviceAdded);
 *   manager.start();
 */
class UDisksDeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit UDisksDeviceManager(QObject *parent = nullptr);
    ~UDisksDeviceManager();

    /**
     * @brief Start monitoring for device changes
     * @return true if successfully connected to UDisks2
     */
    bool start();

    /**
     * @brief Stop monitoring and disconnect from D-Bus
     */
    void stop();

    /**
     * @brief Check if manager is actively monitoring
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief Get list of all detected block devices
     * @param includeSystem If true, include system partitions (/, /boot, etc.)
     * @return Map of object paths to device info
     */
    QMap<QString, BlockDeviceInfo> getDevices(bool includeSystem = false) const;

    /**
     * @brief Get info about specific device by object path
     * @param objectPath D-Bus object path
     * @return Device info (empty if not found)
     */
    BlockDeviceInfo getDeviceInfo(const QString &objectPath) const;

    /**
     * @brief Find device by label or UUID
     * @param identifier Label or UUID to search for
     * @return Device info (empty if not found)
     */
    BlockDeviceInfo findDeviceByIdentifier(const QString &identifier) const;

    /**
     * @brief Mount a device
     * @param objectPath D-Bus object path of the device
     * @param options Mount options (optional, e.g., "ro" for read-only)
     * @return Mount point on success, empty string on failure
     * 
     * By default mounts to /media/<username>/<label|uuid>
     */
    QString mountDevice(const QString &objectPath, const QVariantMap &options = QVariantMap());

    /**
     * @brief Unmount a device
     * @param objectPath D-Bus object path of the device
     * @param force If true, force unmount even if busy
     * @return true on success
     */
    bool unmountDevice(const QString &objectPath, bool force = false);

    /**
     * @brief Safely eject/power off a drive
     * @param objectPath D-Bus object path of the device (will find parent drive)
     * @return true on success
     */
    bool ejectDrive(const QString &objectPath);

    /**
     * @brief Refresh device list from UDisks2
     */
    void refresh();

signals:
    /**
     * @brief Emitted when a new device is detected
     * @param info Information about the new device
     */
    void deviceAdded(const BlockDeviceInfo &info);

    /**
     * @brief Emitted when a device is removed
     * @param objectPath D-Bus object path of the removed device
     * @param displayId Label or UUID of the removed device
     */
    void deviceRemoved(const QString &objectPath, const QString &displayId);

    /**
     * @brief Emitted when a device is mounted
     * @param objectPath D-Bus object path
     * @param mountPoint Where the device was mounted
     */
    void deviceMounted(const QString &objectPath, const QString &mountPoint);

    /**
     * @brief Emitted when a device is unmounted
     * @param objectPath D-Bus object path
     */
    void deviceUnmounted(const QString &objectPath);

    /**
     * @brief Emitted when device properties change (e.g., mount state)
     * @param info Updated device information
     */
    void deviceChanged(const BlockDeviceInfo &info);

    /**
     * @brief Emitted when an error occurs
     * @param operation What was being attempted
     * @param errorMessage Error description
     */
    void errorOccurred(const QString &operation, const QString &errorMessage);

private slots:
    void onInterfacesAdded(const QDBusObjectPath &objectPath, 
                           const QMap<QString, QVariantMap> &interfaces);
    void onInterfacesRemoved(const QDBusObjectPath &objectPath, 
                             const QStringList &interfaces);
    void onPropertiesChanged(const QString &interface,
                             const QVariantMap &changedProperties,
                             const QStringList &invalidatedProperties);

private:
    // D-Bus interface names
    static constexpr const char* UDISKS2_SERVICE = "org.freedesktop.UDisks2";
    static constexpr const char* UDISKS2_PATH = "/org/freedesktop/UDisks2";
    static constexpr const char* UDISKS2_MANAGER_PATH = "/org/freedesktop/UDisks2/Manager";
    static constexpr const char* BLOCK_INTERFACE = "org.freedesktop.UDisks2.Block";
    static constexpr const char* FILESYSTEM_INTERFACE = "org.freedesktop.UDisks2.Filesystem";
    static constexpr const char* PARTITION_INTERFACE = "org.freedesktop.UDisks2.Partition";
    static constexpr const char* DRIVE_INTERFACE = "org.freedesktop.UDisks2.Drive";
    static constexpr const char* DBUS_PROPERTIES = "org.freedesktop.DBus.Properties";
    static constexpr const char* DBUS_OBJECT_MANAGER = "org.freedesktop.DBus.ObjectManager";

    /**
     * @brief Query all block devices from UDisks2
     */
    void enumerateDevices();

    /**
     * @brief Get all properties from a D-Bus interface
     */
    QVariantMap getInterfaceProperties(const QString &objectPath, const QString &interface);

    /**
     * @brief Build BlockDeviceInfo from D-Bus properties
     */
    BlockDeviceInfo buildDeviceInfo(const QString &objectPath);

    /**
     * @brief Check if device should be shown (filter system partitions)
     */
    bool shouldShowDevice(const BlockDeviceInfo &info) const;

    /**
     * @brief Get drive object path for a block device
     */
    QString getDriveObjectPath(const QString &blockObjectPath);

    /**
     * @brief Connect to property change signals for a device
     */
    void connectToDeviceSignals(const QString &objectPath);

    QDBusConnection m_bus;
    QMap<QString, BlockDeviceInfo> m_devices;
    bool m_running;
};

#endif // UDISKSDEVICEMANAGER_H
