#ifndef DISTROINFO_H
#define DISTROINFO_H

#include <QString>
#include <QMap>

class DistroInfo
{
public:
    // Desktop environment (GNOME, KDE, XFCE, etc.)
    static QString desktopEnvironment();

    // Package manager command (apt, dnf, pacman, zypper)
    static QString packageManager();

    // Suggested terminal for current DE
    static QString suggestedTerminal();

    // Install command for a package
    static QString installCommand(const QString& package);

    // Distribution info from /etc/os-release
    static QString distroId();          // e.g. "ubuntu", "fedora"
    static QString distroName();        // e.g. "Ubuntu"
    static QString distroVersion();     // e.g. "24.04"
    static QString distroPrettyName();  // e.g. "Ubuntu 24.04 LTS"

    // All info from /etc/os-release as map
    static QMap<QString, QString> osRelease();

    // lsb_release -a output (runs command)
    static QString lsbRelease();

    // Full report for display
    static QString fullReport();

private:
    static QMap<QString, QString> parseOsRelease();
};

#endif // DISTROINFO_H