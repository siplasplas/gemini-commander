#include "SearchIgnore.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {

QString withSlash(const QString& dir)
{
    return dir.endsWith('/') ? dir : dir + '/';
}

QVector<QRegularExpression> compileNames(const QStringList& patterns)
{
    QVector<QRegularExpression> result;
    for (const QString& pattern : patterns) {
        const QString trimmed = pattern.trimmed();
        if (!trimmed.isEmpty())
            result.append(QRegularExpression(SearchIgnore::globToRegex(trimmed)));
    }
    return result;
}

bool anyMatch(const QVector<QRegularExpression>& patterns, const QString& name)
{
    for (const auto& pattern : patterns)
        if (pattern.match(name).hasMatch()) return true;
    return false;
}

} // namespace

SearchIgnore::SearchIgnore(const QString& root, const QStringList& dirPatterns,
                           const QStringList& filePatterns, bool useGitignore)
    : m_root(QDir::cleanPath(QFileInfo(root).absoluteFilePath()))
    , m_dirPatterns(compileNames(dirPatterns))
    , m_filePatterns(compileNames(filePatterns))
    , m_useGitignore(useGitignore)
{
}

bool SearchIgnore::isEmpty() const
{
    return m_dirPatterns.isEmpty() && m_filePatterns.isEmpty() && !m_useGitignore;
}

bool SearchIgnore::isIgnored(const QFileInfo& entry)
{
    const bool isDir = entry.isDir();
    const QString name = entry.fileName();
    if (anyMatch(isDir ? m_dirPatterns : m_filePatterns, name))
        return true;
    if (!m_useGitignore)
        return false;
    if (isDir && name == QLatin1String(".git"))
        return true;
    return gitignored(entry);
}

QString SearchIgnore::globToRegex(const QString& glob)
{
    QString regex;
    for (qsizetype i = 0; i < glob.size(); ++i) {
        const QChar c = glob[i];
        if (c == '*') {
            if (i + 1 < glob.size() && glob[i + 1] == '*') {
                // "**/" = any directories (also none), "**" elsewhere = anything.
                if (i + 2 < glob.size() && glob[i + 2] == '/') {
                    regex += QStringLiteral("(?:.*/)?");
                    i += 2;
                } else {
                    regex += QStringLiteral(".*");
                    ++i;
                }
            } else {
                regex += QStringLiteral("[^/]*");
            }
        } else if (c == '?') {
            regex += QStringLiteral("[^/]");
        } else if (c == '[') {
            const qsizetype end = glob.indexOf(']', i + 2);
            if (end < 0) {
                regex += QStringLiteral("\\[");
                continue;
            }
            QString set = glob.mid(i + 1, end - i - 1);
            if (set.startsWith('!')) set[0] = '^';
            set.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
            regex += '[' + set + ']';
            i = end;
        } else if (c == '\\' && i + 1 < glob.size()) {
            regex += QRegularExpression::escape(QString(glob[++i]));
        } else {
            regex += QRegularExpression::escape(QString(c));
        }
    }
    return '^' + regex + '$';
}

QVector<SearchIgnore::Rule> SearchIgnore::readGitignore(const QString& dir) const
{
    QVector<Rule> rules;
    QFile file(dir + QStringLiteral("/.gitignore"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return rules;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        // Trailing spaces are ignored unless escaped.
        while (line.endsWith(' ') && !line.endsWith(QStringLiteral("\\ "))) line.chop(1);
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        Rule rule;
        rule.base = withSlash(dir);
        if (line.startsWith('!')) {
            rule.negate = true;
            line.remove(0, 1);
        } else if (line.startsWith(QStringLiteral("\\!")) || line.startsWith(QStringLiteral("\\#"))) {
            line.remove(0, 1);
        }
        if (line.endsWith('/')) {
            rule.dirOnly = true;
            line.chop(1);
        }
        if (line.startsWith('/')) {
            rule.anchored = true;
            line.remove(0, 1);
        } else if (line.contains('/')) {
            rule.anchored = true; // a slash inside makes it relative to this directory
        }
        if (line.isEmpty())
            continue;
        rule.regex = QRegularExpression(globToRegex(line));
        rules.append(rule);
    }
    return rules;
}

std::shared_ptr<const SearchIgnore::Level> SearchIgnore::levelFor(const QString& dir)
{
    if (const auto it = m_levels.constFind(dir); it != m_levels.cend())
        return it.value();
    auto level = std::make_shared<Level>();
    // .gitignore files count from the search root down.
    if (dir != m_root && dir.startsWith(withSlash(m_root)))
        level->parent = levelFor(QFileInfo(dir).path());
    level->rules = readGitignore(dir);
    std::shared_ptr<const Level> result = level;
    m_levels.insert(dir, result);
    return result;
}

bool SearchIgnore::gitignored(const QFileInfo& entry)
{
    const QString path = QDir::cleanPath(entry.absoluteFilePath());
    const QString name = entry.fileName();
    const bool isDir = entry.isDir();
    // The last matching rule wins: deepest .gitignore first, its last line first.
    for (auto level = levelFor(QFileInfo(path).path()); level; level = level->parent) {
        for (auto rule = level->rules.crbegin(); rule != level->rules.crend(); ++rule) {
            if (rule->dirOnly && !isDir) continue;
            const QString subject = rule->anchored ? path.mid(rule->base.size()) : name;
            if (rule->regex.match(subject).hasMatch())
                return !rule->negate;
        }
    }
    return false;
}
