#pragma once

#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

/**
 * @class SearchIgnore
 * @brief Decides which entries a file search or a copy skips (and does not descend into)
 *
 * Two sources:
 * - name patterns for directories and for files, with `*` and `?`, matched
 *   against the entry name only (no paths), e.g. `.git`, `cmake-build*`;
 * - optionally the `.gitignore` files from the search root down: deeper
 *   files add rules, the last matching rule wins and `!` re-includes. The
 *   `.git` directory is skipped too.
 *
 * Not thread-safe; a search worker or a copy operation owns one.
 */
class SearchIgnore {
public:
    SearchIgnore(const QString& root, const QStringList& dirPatterns,
                 const QStringList& filePatterns, bool useGitignore);

    /// True when nothing would ever be skipped.
    bool isEmpty() const;

    /// True when `entry` (inside the search root) is to be skipped.
    bool isIgnored(const QFileInfo& entry);

    /// Regular expression for a wildcard (`*`, `?`, `**`, `[...]`); `*` and
    /// `?` do not cross '/'. Exposed for name patterns elsewhere.
    static QString globToRegex(const QString& glob);

private:
    struct Rule {
        QRegularExpression regex;
        bool negate = false;
        bool dirOnly = false;
        bool anchored = false; ///< matched against the path from `base`, not the name
        QString base;          ///< directory of the .gitignore, with a trailing '/'
    };
    /// Rules of one .gitignore and of all .gitignore files above it (up to the root).
    struct Level {
        std::shared_ptr<const Level> parent;
        QVector<Rule> rules;
    };

    std::shared_ptr<const Level> levelFor(const QString& dir);
    QVector<Rule> readGitignore(const QString& dir) const;
    bool gitignored(const QFileInfo& entry);

    QString m_root;
    QVector<QRegularExpression> m_dirPatterns, m_filePatterns;
    bool m_useGitignore = false;
    QHash<QString, std::shared_ptr<const Level>> m_levels; ///< by directory path
};
