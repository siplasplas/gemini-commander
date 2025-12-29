#include "DistroInfo.h"

#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>

#ifdef Q_OS_WIN
#include <QSettings>
#endif

QString DistroInfo::desktopEnvironment()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Windows Shell (Explorer)");
#else
    QString de = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (!de.isEmpty())
        return de;

    de = qEnvironmentVariable("XDG_SESSION_DESKTOP");
    if (!de.isEmpty())
        return de;

    de = qEnvironmentVariable("DESKTOP_SESSION");
    if (!de.isEmpty())
        return de;

    return QStringLiteral("Unknown");
#endif
}

QString DistroInfo::packageManager()
{
#ifdef Q_OS_WIN
    // Windows package managers
    if (!QStandardPaths::findExecutable("winget").isEmpty())
        return QStringLiteral("winget");
    if (!QStandardPaths::findExecutable("choco").isEmpty())
        return QStringLiteral("choco");
    if (!QStandardPaths::findExecutable("scoop").isEmpty())
        return QStringLiteral("scoop");
#else
    // Linux package managers
    if (!QStandardPaths::findExecutable("apt").isEmpty())
        return QStringLiteral("apt");
    if (!QStandardPaths::findExecutable("dnf").isEmpty())
        return QStringLiteral("dnf");
    if (!QStandardPaths::findExecutable("pacman").isEmpty())
        return QStringLiteral("pacman");
    if (!QStandardPaths::findExecutable("zypper").isEmpty())
        return QStringLiteral("zypper");
    if (!QStandardPaths::findExecutable("apk").isEmpty())
        return QStringLiteral("apk");
    if (!QStandardPaths::findExecutable("emerge").isEmpty())
        return QStringLiteral("emerge");
#endif

    return QString();
}

QString DistroInfo::suggestedTerminal()
{
#ifdef Q_OS_WIN
    // Windows Terminal is preferred if installed
    if (!QStandardPaths::findExecutable("wt").isEmpty())
        return QStringLiteral("wt");
    // PowerShell via cmd /c start - opens visible window
    return QStringLiteral("powershell");
#else
    QString de = desktopEnvironment().toLower();

    if (de.contains("gnome") || de.contains("cinnamon") || de.contains("unity"))
        return QStringLiteral("gnome-terminal");
    if (de.contains("kde") || de.contains("plasma"))
        return QStringLiteral("konsole");
    if (de.contains("xfce"))
        return QStringLiteral("xfce4-terminal");
    if (de.contains("mate"))
        return QStringLiteral("mate-terminal");
    if (de.contains("lxqt") || de.contains("lxde"))
        return QStringLiteral("qterminal");

    return QStringLiteral("xterm");
#endif
}

QString DistroInfo::installCommand(const QString& package)
{
    QString pm = packageManager();

    // Windows package managers
    if (pm == "winget")
        return QString("winget install %1").arg(package);
    if (pm == "choco")
        return QString("choco install %1").arg(package);
    if (pm == "scoop")
        return QString("scoop install %1").arg(package);

    // Linux package managers
    if (pm == "apt")
        return QString("sudo apt install %1").arg(package);
    if (pm == "dnf")
        return QString("sudo dnf install %1").arg(package);
    if (pm == "pacman")
        return QString("sudo pacman -S %1").arg(package);
    if (pm == "zypper")
        return QString("sudo zypper install %1").arg(package);
    if (pm == "apk")
        return QString("sudo apk add %1").arg(package);
    if (pm == "emerge")
        return QString("sudo emerge %1").arg(package);

    return QString();
}

QMap<QString, QString> DistroInfo::parseOsRelease()
{
    QMap<QString, QString> result;

    QFile file("/etc/os-release");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        int eq = line.indexOf('=');
        if (eq > 0) {
            QString key = line.left(eq);
            QString value = line.mid(eq + 1);
            // Remove quotes
            if (value.startsWith('"') && value.endsWith('"'))
                value = value.mid(1, value.length() - 2);
            result[key] = value;
        }
    }

    return result;
}

QMap<QString, QString> DistroInfo::osRelease()
{
    return parseOsRelease();
}

QString DistroInfo::distroId()
{
#ifdef Q_OS_WIN
    return QStringLiteral("windows");
#else
    return parseOsRelease().value("ID");
#endif
}

QString DistroInfo::distroName()
{
#ifdef Q_OS_WIN
    return QSysInfo::productType();  // "windows"
#else
    return parseOsRelease().value("NAME");
#endif
}

QString DistroInfo::distroVersion()
{
#ifdef Q_OS_WIN
    return QSysInfo::productVersion();  // e.g. "10", "11"
#else
    return parseOsRelease().value("VERSION_ID");
#endif
}

QString DistroInfo::distroPrettyName()
{
#ifdef Q_OS_WIN
    return QSysInfo::prettyProductName();  // e.g. "Windows 11 Version 23H2"
#else
    return parseOsRelease().value("PRETTY_NAME");
#endif
}

QString DistroInfo::lsbRelease()
{
    if (QStandardPaths::findExecutable("lsb_release").isEmpty())
        return QString();

    QProcess proc;
    proc.start("lsb_release", QStringList() << "-a");
    proc.waitForFinished(3000);

    if (proc.exitCode() != 0)
        return QString();

    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

QString DistroInfo::fullReport()
{
    QStringList lines;

#ifdef Q_OS_WIN
    lines << QStringLiteral("=== System ===");
    lines << QString("Name: %1").arg(distroPrettyName());
    lines << QString("Kernel: %1 %2").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
    lines << QString("Architecture: %1").arg(QSysInfo::currentCpuArchitecture());

    lines << QString();
    lines << QStringLiteral("=== Shell ===");
    lines << QString("DE: %1").arg(desktopEnvironment());
    lines << QString("Suggested terminal: %1").arg(suggestedTerminal());

    // Check if Windows Terminal is available
    if (QStandardPaths::findExecutable("wt").isEmpty()) {
        lines << QString();
        lines << QStringLiteral("=== Install Windows Terminal ===");
        QString pm = packageManager();
        if (pm == "winget")
            lines << QStringLiteral("winget install Microsoft.WindowsTerminal");
        else if (pm == "choco")
            lines << QStringLiteral("choco install microsoft-windows-terminal");
        else if (pm == "scoop")
            lines << QStringLiteral("scoop install windows-terminal");
        else
            lines << QStringLiteral("Install from Microsoft Store or GitHub");
    }

    lines << QString();
    lines << QStringLiteral("=== Package Manager ===");
    QString pm = packageManager();
    if (!pm.isEmpty()) {
        lines << QString("Package manager: %1").arg(pm);
        lines << QString("Install example: %1").arg(installCommand("package-name"));
    } else {
        lines << QStringLiteral("Package manager: Not found");
        lines << QStringLiteral("Install winget: comes with App Installer from Microsoft Store");
    }
#else
    lines << QStringLiteral("=== Distribution ===");
    QString pretty = distroPrettyName();
    if (!pretty.isEmpty())
        lines << QString("Name: %1").arg(pretty);
    else
        lines << QString("Name: %1 %2").arg(distroName(), distroVersion());

    QString id = distroId();
    if (!id.isEmpty())
        lines << QString("ID: %1").arg(id);

    lines << QString();
    lines << QStringLiteral("=== Desktop Environment ===");
    lines << QString("DE: %1").arg(desktopEnvironment());
    lines << QString("Suggested terminal: %1").arg(suggestedTerminal());

    lines << QString();
    lines << QStringLiteral("=== Package Manager ===");
    QString pm = packageManager();
    if (!pm.isEmpty()) {
        lines << QString("Package manager: %1").arg(pm);
        lines << QString("Install example: %1").arg(installCommand("package-name"));
    } else {
        lines << QStringLiteral("Package manager: Unknown");
    }

    QString lsb = lsbRelease();
    if (!lsb.isEmpty()) {
        lines << QString();
        lines << QStringLiteral("=== lsb_release -a ===");
        lines << lsb;
    }
#endif

    return lines.join('\n');
}