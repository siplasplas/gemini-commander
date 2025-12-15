#include "SearchWorker.h"
#include "SortedDirIterator.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

SearchWorker::SearchWorker(const SearchCriteria& criteria, QObject* parent)
    : QObject(parent)
    , m_criteria(criteria)
    , m_shouldStop(false)
{
    // Convert wildcard pattern to regex
    QString pattern = m_criteria.fileNamePattern;
    if (pattern.isEmpty())
        pattern = "*";

    // Add wildcards for "part of name" mode
    if (m_criteria.partOfName && !pattern.startsWith("*"))
        pattern = "*" + pattern;
    if (m_criteria.partOfName && !pattern.endsWith("*"))
        pattern = pattern + "*";

    // Escape special regex chars and convert wildcards
    pattern.replace("\\", "\\\\");
    pattern.replace(".", "\\.");
    pattern.replace("*", ".*");
    pattern.replace("?", ".");
    pattern = "^" + pattern + "$";

    QRegularExpression::PatternOption option = m_criteria.fileNameCaseSensitive
        ? QRegularExpression::NoPatternOption
        : QRegularExpression::CaseInsensitiveOption;

    m_fileNameRegex = QRegularExpression(pattern, option);
}

void SearchWorker::startSearch()
{
    m_shouldStop = false;

    QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden;

    SortedDirIterator it(m_criteria.searchPath, filters);

    int searchedFiles = 0;
    int foundFiles = 0;

    while (it.hasNext() && !m_shouldStop) {
        QFileInfo info = it.next();

        bool isDir = info.isDir();
        bool isFile = info.isFile();

        // Count files and directories
        if (isFile || isDir)
            searchedFiles++;

        // Update progress periodically (every 1000 items)
        if (searchedFiles % 1000 == 0)
            emit progressUpdate(searchedFiles, foundFiles);

        // Directories only filter
        if (m_criteria.directoriesOnly && !isDir)
            continue;

        // Check file/directory name pattern
        if (!matchesFileName(info.fileName()))
            continue;

        // For files: check size and content
        if (isFile) {
            // Check file size
            if (!matchesFileSize(info.size()))
                continue;

            // Check containing text if specified
            if (!m_criteria.containingText.isEmpty()) {
                if (!matchesContainingText(info.absoluteFilePath()))
                    continue;
            }
        }

        // For directories: skip size/content checks, but report if name matches
        // Item (file or directory) matches all applicable criteria
        foundFiles++;
        emit resultFound(info.absoluteFilePath(), info.size(), info.lastModified());
    }

    // Final update
    emit progressUpdate(searchedFiles, foundFiles);
    emit searchFinished();
}

void SearchWorker::stopSearch()
{
    m_shouldStop = true;
}

bool SearchWorker::matchesFileName(const QString& fileName) const
{
    return m_fileNameRegex.match(fileName).hasMatch();
}

bool SearchWorker::matchesFileSize(qint64 size) const
{
    if (m_criteria.minSize >= 0 && size < m_criteria.minSize)
        return false;

    if (m_criteria.maxSize >= 0 && size > m_criteria.maxSize)
        return false;

    return true;
}

bool SearchWorker::matchesContainingText(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    Qt::CaseSensitivity cs = m_criteria.textCaseSensitive
        ? Qt::CaseSensitive
        : Qt::CaseInsensitive;

    // For whole words, create a regex pattern
    if (m_criteria.wholeWords) {
        QString searchText = QRegularExpression::escape(m_criteria.containingText);
        QString pattern = "\\b" + searchText + "\\b";
        QRegularExpression::PatternOption option = m_criteria.textCaseSensitive
            ? QRegularExpression::NoPatternOption
            : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression regex(pattern, option);

        while (!in.atEnd() && !m_shouldStop) {
            QString line = in.readLine();
            if (regex.match(line).hasMatch())
                return true;
        }
    } else {
        // Simple substring search
        while (!in.atEnd() && !m_shouldStop) {
            QString line = in.readLine();
            if (line.contains(m_criteria.containingText, cs))
                return true;
        }
    }

    return false;
}