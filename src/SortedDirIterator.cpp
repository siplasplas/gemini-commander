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

void SortedDirIterator::pushDir(const QString& path)
{
    QDir dir(path);
    if (!dir.exists())
        return;

    Frame f;
    f.dir = dir;
    f.entries = dir.entryInfoList(m_filters, QDir::NoSort); // bez sortowania Qt
    std::sort(f.entries.begin(), f.entries.end(), m_cmp);   // nasz sort
    f.index = 0;

    m_stack.push_back(std::move(f));
}

bool SortedDirIterator::hasNext() const
{
    // pomijamy puste ramki na końcu
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
            m_stack.removeLast();
            continue;
        }

        QFileInfo fi = f.entries.at(f.index++);
        if (fi.isDir()) {
            // wchodzimy w podkatalog po zwróceniu QFileInfo
            // (DFS, katalog przed zawartością)
            pushDir(fi.absoluteFilePath());
        }
        m_fileInfo = fi;
        return fi;
    }

    m_fileInfo = QFileInfo(); // brak elementów
    return m_fileInfo;
}

bool SortedDirIterator::defaultComparator(const QFileInfo& a, const QFileInfo& b)
{
    const bool aDir = a.isDir();
    const bool bDir = b.isDir();

    if (aDir != bDir)
        return bDir;

    // alfabetycznie, locale-aware
    return a.fileName().localeAwareCompare(b.fileName()) < 0;
}
