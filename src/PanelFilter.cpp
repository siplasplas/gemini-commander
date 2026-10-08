#include "PanelFilter.h"

#include "Config.h"

#include <QObject>

bool PanelFilter::showsAll() const
{
    return mode == Mode::AllFiles
        || (mode == Mode::Custom && masks.isEmpty() && !useGitignore);
}

QString PanelFilter::description() const
{
    switch (mode) {
    case Mode::AllFiles:
        return {};
    case Mode::Programs:
        return QObject::tr("programs");
    case Mode::Custom: {
        QStringList parts = masks;
        if (useGitignore)
            parts << QStringLiteral("!gitignore");  // "!": without what it ignores
        return parts.join(';');
    }
    }
    return {};
}

PanelFilterMatcher::PanelFilterMatcher(const PanelFilter& filter, const QString& root)
    : m_mode(filter.mode)
{
    if (filter.mode != PanelFilter::Mode::Custom)
        return;
    // Masks as in Find Files, case-insensitive.
    for (const QString& mask : filter.masks)
        m_masks.append(QRegularExpression(SearchIgnore::globToRegex(mask),
                                          QRegularExpression::CaseInsensitiveOption));
    if (filter.useGitignore)
        m_ignore.emplace(root, QStringList(), QStringList(), true, Config::instance().gitignoreOptions());
}

bool PanelFilterMatcher::accepts(const QFileInfo& entry)
{
    if (m_ignore && m_ignore->isIgnored(entry))
        return false;
    if (entry.isDir())
        return true;
    switch (m_mode) {
    case PanelFilter::Mode::AllFiles:
        return true;
    case PanelFilter::Mode::Programs:
        return isProgram(entry);
    case PanelFilter::Mode::Custom:
        if (m_masks.isEmpty())
            return true;
        for (const auto& mask : m_masks)
            if (mask.match(entry.fileName()).hasMatch())
                return true;
        return false;
    }
    return true;
}

bool PanelFilterMatcher::isProgram(const QFileInfo& file)
{
#ifdef Q_OS_WIN
    static const QStringList suffixes{"exe", "com", "bat", "cmd", "ps1"};
    return suffixes.contains(file.suffix(), Qt::CaseInsensitive);
#else
    return file.permissions().testFlag(QFileDevice::ExeUser);
#endif
}
