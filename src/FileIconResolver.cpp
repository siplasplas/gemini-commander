#include "FileIconResolver.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QPixmap>
#include <algorithm>

FileIconResolver& FileIconResolver::instance()
{
    static FileIconResolver instance;
    return instance;
}

bool FileIconResolver::isElf(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray header = file.read(4);
    file.close();

    // ELF magic: 0x7f 'E' 'L' 'F'
    return header.size() >= 4 &&
           header[0] == 0x7f &&
           header[1] == 'E' &&
           header[2] == 'L' &&
           header[3] == 'F';
}

bool FileIconResolver::isShebangScript(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray header = file.read(2);
    file.close();

    return header.startsWith("#!");
}

QString FileIconResolver::getExecutableName(const QString& filePath)
{
    return QFileInfo(filePath).fileName();
}

QStringList FileIconResolver::extractSuffixes(const QString& fileName)
{
    QStringList suffixes;
    QString name = fileName;

    // Remove leading dot for hidden files
    if (name.startsWith('.') && name.size() > 1) {
        name = name.mid(1);
    }

    // Find all dots and extract suffixes
    int lastDot = name.lastIndexOf('.');
    while (lastDot > 0) {
        QString suffix = name.mid(lastDot + 1).toLower();
        if (!suffix.isEmpty()) {
            // Build compound suffix (e.g., "tar.gz")
            QString compound;
            int pos = lastDot;
            while (pos > 0) {
                int prevDot = name.lastIndexOf('.', pos - 1);
                if (prevDot < 0) break;

                QString part = name.mid(prevDot + 1, pos - prevDot - 1).toLower();
                if (compound.isEmpty()) {
                    compound = name.mid(prevDot + 1).toLower();
                }
                // Add compound suffix if it's longer
                if (!compound.isEmpty() && !suffixes.contains(compound)) {
                    suffixes.append(compound);
                }
                pos = prevDot;
            }

            // Add simple suffix
            if (!suffixes.contains(suffix)) {
                suffixes.append(suffix);
            }
        }
        lastDot = name.lastIndexOf('.', lastDot - 1);
    }

    // Sort by length descending (longest first)
    std::sort(suffixes.begin(), suffixes.end(), [](const QString& a, const QString& b) {
        return a.length() > b.length();
    });

    return suffixes;
}

std::optional<QString> FileIconResolver::findCachedSuffix(const QString& fileName) const
{
    QStringList suffixes = extractSuffixes(fileName);

    // Check cache for longest suffix first
    for (const QString& suffix : suffixes) {
        if (m_suffixCache.contains(suffix)) {
            return suffix;
        }
    }

    return std::nullopt;
}

QString FileIconResolver::findIconInTheme(const QString& iconName)
{
    // Themes to search, in priority order
    static const QStringList themes = { "Adwaita", "hicolor", "mate", "Yaru" };

    // Categories to search (apps for executables, mimetypes for file types)
    static const QStringList categories = { "apps", "mimetypes" };

    // Sizes to search, preferring scalable/larger first (also includes symbolic)
    static const QStringList sizes = { "scalable", "symbolic", "256x256", "128x128", "64x64", "48x48", "32x32", "24x24", "22x22", "16x16" };

    static const QStringList basePaths = {
        QDir::homePath() + "/.local/share/icons",
        "/usr/share/icons"
    };

    static const QStringList extensions = { ".svg", ".png", ".xpm" };

    for (const QString& basePath : basePaths) {
        for (const QString& theme : themes) {
            for (const QString& size : sizes) {
                for (const QString& category : categories) {
                    for (const QString& ext : extensions) {
                        QString path = basePath + "/" + theme + "/" + size + "/" + category + "/" + iconName + ext;
                        if (QFile::exists(path)) {
                            return path;
                        }
                    }
                }
            }
        }
    }

    // Pixmaps as last resort
    for (const QString& ext : extensions) {
        QString path = "/usr/share/pixmaps/" + iconName + ext;
        if (QFile::exists(path)) {
            return path;
        }
    }

    return QString();
}

QIcon FileIconResolver::lookupElfIcon(const QString& filePath)
{
    // Check cache first
    auto it = m_elfCache.find(filePath);
    if (it != m_elfCache.end()) {
        return it.value();
    }

    QString execName = getExecutableName(filePath);
    QIcon icon;

    // Try Qt theme first (most reliable)
    icon = QIcon::fromTheme(execName);
    if (!isIconValid(icon)) {
        icon = QIcon::fromTheme(execName.toLower());
    }

    // Try with common suffixes removed
    if (!isIconValid(icon) && execName.contains('-')) {
        QString baseName = execName.section('-', 0, 0);
        icon = QIcon::fromTheme(baseName);
        if (!isIconValid(icon)) {
            icon = QIcon::fromTheme(baseName.toLower());
        }
    }

    // Fallback to manual search in hicolor
    if (!isIconValid(icon)) {
        QString iconPath = findIconInTheme(execName);
        if (iconPath.isEmpty()) {
            iconPath = findIconInTheme(execName.toLower());
        }
        if (!iconPath.isEmpty()) {
            icon = QIcon(iconPath);
        }
    }

    m_elfCache.insert(filePath, icon);
    return icon;
}

QIcon FileIconResolver::lookupMimeIcon(const QString& filePath)
{
    QFileInfo info(filePath);
    QString fileName = info.fileName();

    // Check suffix cache first (longest suffix wins)
    auto cachedSuffix = findCachedSuffix(fileName);
    if (cachedSuffix.has_value()) {
        return m_suffixCache.value(cachedSuffix.value());
    }

    // Get MIME type
    QMimeType mt = m_mimeDb.mimeTypeForFile(filePath, QMimeDatabase::MatchExtension);

    // Try MIME generic icon (e.g., "text-x-generic", "image-x-generic")
    // These are well-supported in Adwaita and other themes
    QIcon icon;

    // First try the specific icon name
    if (!isIconValid(icon)) {
        icon = QIcon::fromTheme(mt.iconName());
    }
    // Then try generic icon name (better coverage in themes)
    if (!isIconValid(icon) && !mt.genericIconName().isEmpty()) {
        icon = QIcon::fromTheme(mt.genericIconName());
    }

    // Fallback to manual search for MIME icon
    if (!isIconValid(icon)) {
        QString iconPath = findIconInTheme(mt.iconName());
        if (!iconPath.isEmpty()) {
            icon = QIcon(iconPath);
        }
    }
    if (!isIconValid(icon) && !mt.genericIconName().isEmpty()) {
        QString iconPath = findIconInTheme(mt.genericIconName());
        if (!iconPath.isEmpty()) {
            icon = QIcon(iconPath);
        }
    }

    // Cache by suffix (use longest suffix) - but only if we found a valid icon
    QStringList suffixes = extractSuffixes(fileName);
    if (!suffixes.isEmpty() && isIconValid(icon)) {
        m_suffixCache.insert(suffixes.first(), icon);
    }

    return icon;
}

QIcon FileIconResolver::getIcon(const QString& filePath, bool checkContent)
{
    QFileInfo info(filePath);
    if (!info.exists() || info.isDir()) {
        return QIcon();
    }

    // Check if it's an executable
    bool isExec = info.isExecutable();

    if (checkContent || isExec) {
        // Check for ELF
        if (isElf(filePath)) {
            QIcon elfIcon = lookupElfIcon(filePath);
            if (isIconValid(elfIcon)) {
                return elfIcon;
            }
            // Fall through to MIME lookup if no specific icon found
        }
    }

    // Standard MIME-based lookup
    return lookupMimeIcon(filePath);
}

QIcon FileIconResolver::getIconByName(const QString& fileName)
{
    // Check suffix cache first
    auto cachedSuffix = findCachedSuffix(fileName);
    if (cachedSuffix.has_value()) {
        return m_suffixCache.value(cachedSuffix.value());
    }

    // Get MIME type by extension only
    QMimeType mt = m_mimeDb.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension);

    // Try MIME generic icon (e.g., "text-x-generic", "image-x-generic")
    QIcon icon;

    // First try the specific icon name
    if (!isIconValid(icon)) {
        icon = QIcon::fromTheme(mt.iconName());
    }
    // Then try generic icon name (better coverage in themes)
    if (!isIconValid(icon) && !mt.genericIconName().isEmpty()) {
        icon = QIcon::fromTheme(mt.genericIconName());
    }

    // Fallback to manual search for MIME icon
    if (!isIconValid(icon)) {
        QString iconPath = findIconInTheme(mt.iconName());
        if (!iconPath.isEmpty()) {
            icon = QIcon(iconPath);
        }
    }
    if (!isIconValid(icon) && !mt.genericIconName().isEmpty()) {
        QString iconPath = findIconInTheme(mt.genericIconName());
        if (!iconPath.isEmpty()) {
            icon = QIcon(iconPath);
        }
    }

    // Cache by suffix - but only if we found a valid icon
    QStringList suffixes = extractSuffixes(fileName);
    if (!suffixes.isEmpty() && isIconValid(icon)) {
        m_suffixCache.insert(suffixes.first(), icon);
    }

    return icon;
}

void FileIconResolver::clearCache()
{
    m_suffixCache.clear();
    m_elfCache.clear();
}

bool FileIconResolver::isIconValid(const QIcon& icon)
{
    if (icon.isNull())
        return false;

    // Check if the icon actually has any available sizes/pixmaps
    QList<QSize> sizes = icon.availableSizes();
    if (sizes.isEmpty()) {
        // Try to get a pixmap - if it's empty, the icon is not valid
        QPixmap pm = icon.pixmap(24, 24);
        return !pm.isNull() && pm.width() > 0;
    }
    return true;
}