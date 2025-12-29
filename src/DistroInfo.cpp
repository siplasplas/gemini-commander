#include "DistroInfo.h"

#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QStandardPaths>

QString DistroInfo::desktopEnvironment()
{
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
}

QString DistroInfo::packageManager()
{
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

    return QString();
}

QString DistroInfo::suggestedTerminal()
{
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
}

QString DistroInfo::installCommand(const QString& package)
{
    QString pm = packageManager();

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
    return parseOsRelease().value("ID");
}

QString DistroInfo::distroName()
{
    return parseOsRelease().value("NAME");
}

QString DistroInfo::distroVersion()
{
    return parseOsRelease().value("VERSION_ID");
}

QString DistroInfo::distroPrettyName()
{
    return parseOsRelease().value("PRETTY_NAME");
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

    return lines.join('\n');
}