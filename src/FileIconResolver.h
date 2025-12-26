#ifndef FILEICONRESOLVER_H
#define FILEICONRESOLVER_H

#include <QIcon>
#include <QString>
#include <QHash>
#include <QMimeDatabase>
#include <optional>

/**
 * FileIconResolver - resolves icons for files based on their name and content.
 *
 * Features:
 * - Suffix-based caching with longest-suffix-first matching (e.g., .tar.gz before .gz)
 * - ELF executable icon lookup from system theme directories
 * - MIME-based icon lookup using associated application icons
 * - Magic byte detection for file type identification
 */
class FileIconResolver
{
public:
    static FileIconResolver& instance();

    /**
     * Get icon for a file.
     * @param filePath Full path to the file
     * @param checkContent If true, read first bytes to detect file type (slower)
     * @return QIcon for the file, or null icon if not found
     */
    QIcon getIcon(const QString& filePath, bool checkContent = false);

    /**
     * Get icon for a file by name only (no content check).
     * @param fileName File name (with extension)
     * @return QIcon for the file type, or null icon if not found
     */
    QIcon getIconByName(const QString& fileName);

    /**
     * Clear the icon cache.
     */
    void clearCache();

    /**
     * Check if file is ELF executable by reading magic bytes.
     * @param filePath Full path to the file
     * @return true if file is ELF
     */
    static bool isElf(const QString& filePath);

    /**
     * Check if file is a script with shebang.
     * @param filePath Full path to the file
     * @return true if file starts with #!
     */
    static bool isShebangScript(const QString& filePath);

    /**
     * Get executable name for ELF file (basename).
     * @param filePath Full path to the ELF file
     * @return Executable name (used for icon lookup)
     */
    static QString getExecutableName(const QString& filePath);

private:
    FileIconResolver() = default;

    // Extract all suffixes from filename, sorted by length descending
    // e.g., "file.tar.gz" -> ["tar.gz", "gz"]
    static QStringList extractSuffixes(const QString& fileName);

    // Get longest matching suffix from cache
    std::optional<QString> findCachedSuffix(const QString& fileName) const;

    // Icon lookup methods
    QIcon lookupElfIcon(const QString& filePath);
    QIcon lookupMimeIcon(const QString& filePath);
    QIcon lookupIconFromApp(const QString& mimeType);

    // Find icon file in icon theme directories
    static QString findIconInTheme(const QString& iconName);

    // Get default app .desktop file for MIME type
    QString getDesktopFileForMime(const QString& mimeType);

    // Get Icon= value from .desktop file
    static QString getIconFromDesktopFile(const QString& desktopFilePath);

    // Cache: suffix -> icon (e.g., "tar.gz" -> icon, "gz" -> icon)
    QHash<QString, QIcon> m_suffixCache;

    // Cache: full path -> icon (for ELF executables)
    QHash<QString, QIcon> m_elfCache;

    // Cache: MIME type -> icon
    QHash<QString, QIcon> m_mimeCache;

    QMimeDatabase m_mimeDb;

    // Helper to check if icon is actually valid (has pixmaps)
    static bool isIconValid(const QIcon& icon);
};

#endif // FILEICONRESOLVER_H