#include "SearchWorker.h"
#include "SortedDirIterator.h"
#include "quitls.h"

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

    // ─────────────────────────────────────────────────────────
    // MODE 1: Search in results
    // ─────────────────────────────────────────────────────────
    if (m_criteria.searchInResults && !m_criteria.previousResultPaths.isEmpty()) {
        int searchedFiles = 0;
        int foundFiles = 0;

        for (const QString& path : m_criteria.previousResultPaths) {
            if (m_shouldStop)
                break;

            QFileInfo info(path);
            if (!info.exists())
                continue;

            searchedFiles++;

            // Update progress periodically
            if (searchedFiles % 100 == 0)
                emit progressUpdate(searchedFiles, foundFiles);

            bool isDir = info.isDir();
            bool isFile = info.isFile();

            // Apply all filters
            if (!matchesItemType(isDir, isFile))
                continue;

            // Filename pattern (with negation)
            bool nameMatches = matchesFileName(info.fileName());
            if (m_criteria.negateFileName)
                nameMatches = !nameMatches;
            if (!nameMatches)
                continue;

            // Size filter (files only)
            if (isFile && !matchesFileSize(info.size()))
                continue;

            // Text content filter (files only)
            if (isFile && !m_criteria.containingText.isEmpty()) {
                bool textMatches = matchesContainingText(info.absoluteFilePath());
                if (m_criteria.negateContainingText)
                    textMatches = !textMatches;
                if (!textMatches)
                    continue;
            }

            // File type filters (files only)
            if (isFile && !matchesFileType(info.absoluteFilePath()))
                continue;

            // Executable bits filter
            if (!matchesExecutableBits(info.absoluteFilePath()))
                continue;

            // Shebang filter (files only)
            if (isFile && !matchesShebang(info.absoluteFilePath()))
                continue;

            // All filters passed
            foundFiles++;
            emit resultFound(info.absoluteFilePath(), info.size(), info.lastModified());
        }

        emit progressUpdate(searchedFiles, foundFiles);
        emit searchFinished();
        return;
    }

    // ─────────────────────────────────────────────────────────
    // MODE 2: Normal filesystem search
    // ─────────────────────────────────────────────────────────
    QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden;

    SortedDirIterator it(m_criteria.searchPath, filters);

    int searchedFiles = 0;
    int foundFiles = 0;

    while (it.hasNext() && !m_shouldStop) {
        QFileInfo info = it.next();

        bool isDir = info.isDir();
        bool isFile = info.isFile();

        if (isFile || isDir)
            searchedFiles++;

        // Update progress periodically
        if (searchedFiles % 1000 == 0)
            emit progressUpdate(searchedFiles, foundFiles);

        // Item type filter
        if (!matchesItemType(isDir, isFile))
            continue;

        // Filename pattern (with negation)
        bool nameMatches = matchesFileName(info.fileName());
        if (m_criteria.negateFileName)
            nameMatches = !nameMatches;
        if (!nameMatches)
            continue;

        // For files: check size, content, and advanced filters
        if (isFile) {
            // Size filter
            if (!matchesFileSize(info.size()))
                continue;

            // Text content filter
            if (!m_criteria.containingText.isEmpty()) {
                bool textMatches = matchesContainingText(info.absoluteFilePath());
                if (m_criteria.negateContainingText)
                    textMatches = !textMatches;
                if (!textMatches)
                    continue;
            }

            // File type filters
            if (!matchesFileType(info.absoluteFilePath()))
                continue;

            // Shebang filter
            if (!matchesShebang(info.absoluteFilePath()))
                continue;
        }

        // Executable bits filter (applies to both files and directories)
        if (!matchesExecutableBits(info.absoluteFilePath()))
            continue;

        // All filters passed
        foundFiles++;
        emit resultFound(info.absoluteFilePath(), info.size(), info.lastModified());
    }

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

bool SearchWorker::matchesItemType(bool isDir, bool isFile) const
{
    switch (m_criteria.itemTypeFilter) {
        case ItemTypeFilter::FilesAndDirectories:
            return isFile || isDir;
        case ItemTypeFilter::FilesOnly:
            return isFile && !isDir;
        case ItemTypeFilter::DirectoriesOnly:
            return isDir && !isFile;
    }
    return false;
}

bool SearchWorker::matchesFileType(const QString& filePath) const
{
    // Text file check
    if (m_criteria.filterTextFiles) {
        bool isText = isTextFile(filePath);
        bool result = m_criteria.negateTextFiles ? !isText : isText;
        if (!result)
            return false;
    }

    // ELF binary check
    if (m_criteria.filterELFBinaries) {
        ExecutableType type = getExecutableType(filePath);
        bool isELF = (type == ExecutableType::ELFBinary);
        bool result = m_criteria.negateELFBinaries ? !isELF : isELF;
        if (!result)
            return false;
    }

    return true;
}

bool SearchWorker::matchesExecutableBits(const QString& filePath) const
{
    using EBF = SearchCriteria::ExecutableBitsFilter;

    if (m_criteria.executableBits == EBF::NotSpecified)
        return true;  // Don't check

    QFileInfo info(filePath);
    QFile::Permissions perms = info.permissions();

    bool ownerExec = perms & QFile::ExeOwner;
    bool groupExec = perms & QFile::ExeGroup;
    bool otherExec = perms & QFile::ExeOther;

    bool anyExec = ownerExec || groupExec || otherExec;
    bool allExec = ownerExec && groupExec && otherExec;

    switch (m_criteria.executableBits) {
        case EBF::Executable:
            return anyExec;
        case EBF::NotExecutable:
            return !anyExec;
        case EBF::AllExecutable:
            return allExec;
        case EBF::NotSpecified:
        default:
            return true;
    }
}

bool SearchWorker::matchesShebang(const QString& filePath) const
{
    if (!m_criteria.filterShebang)
        return true;  // Don't check

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return m_criteria.negateShebang;  // Can't read - treat as no shebang

    QByteArray header = file.read(2);
    file.close();

    bool hasShebang = (header.size() >= 2 && header[0] == '#' && header[1] == '!');

    return m_criteria.negateShebang ? !hasShebang : hasShebang;
}