#include "SortedDirIterator.h"
#include <algorithm>

SortedDirIterator::SortedDirIterator(const QString& rootPath,
                                     QDir::Filters filters,
                                     Comparator cmp)
    : m_filters(filters)
    , m_cmp(cmp ? cmp : defaultComparator)
{
    pushDir(rootPath);
}

bool SortedDirIterator::canEnter(const QString& canonicalPath) const
{
    if (canonicalPath.isEmpty())
        return false;  // cannot resolve path

    return m_visitedPaths.find(canonicalPath) == m_visitedPaths.end();  // true if no cycle
}

void SortedDirIterator::pushDir(const QString& path)
{
    QDir dir(path);
    if (!dir.exists())
        return;

    // Get canonical path and check for cycles
    QString canonicalPath = QFileInfo(path).canonicalFilePath();
    if (!canEnter(canonicalPath))
        return;  // cycle detected - don't enter

    Frame f;
    f.dir = dir;
    f.canonicalPath = canonicalPath;
    f.entries = dir.entryInfoList(m_filters, QDir::NoSort); // no Qt sorting
    std::sort(f.entries.begin(), f.entries.end(), m_cmp);   // our custom sort
    f.index = 0;

    m_stack.push_back(std::move(f));
    m_visitedPaths.insert(canonicalPath);  // mark as visited
}

void SortedDirIterator::popFrame()
{
    if (m_stack.isEmpty())
        return;

    const Frame& f = m_stack.last();
    m_visitedPaths.erase(f.canonicalPath);  // remove from visited
    m_stack.removeLast();
}

bool SortedDirIterator::hasNext() const
{
    // skip empty frames at the end
    int i = m_stack.size() - 1;
    while (i >= 0) {
        const Frame& f = m_stack[i];
        if (f.index < f.entries.size())
            return true;
        --i;
    }
    return false;
}

QFileInfo SortedDirIterator::next()
{
    while (!m_stack.isEmpty()) {
        Frame& f = m_stack.last();

        if (f.index >= f.entries.size()) {
            popFrame();  // remove Frame and update m_visitedPaths
            continue;
        }

        QFileInfo fi = f.entries.at(f.index++);
        if (fi.isDir()) {
            // enter subdirectory after returning QFileInfo
            // (DFS, directory before its contents)
            pushDir(fi.absoluteFilePath());  // automatically detects cycles
        }
        m_fileInfo = fi;
        return fi;
    }

    m_fileInfo = QFileInfo(); // no more elements
    return m_fileInfo;
}

bool SortedDirIterator::defaultComparator(const QFileInfo& a, const QFileInfo& b)
{
    const bool aDir = a.isDir();
    const bool bDir = b.isDir();

    if (aDir != bDir)
        return bDir;

    // alphabetically, locale-aware
    return a.fileName().localeAwareCompare(b.fileName()) < 0;
}
