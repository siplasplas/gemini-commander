#pragma once

#include <QDir>
#include <QFileInfo>
#include <QVector>
#include <unordered_set>
#include <functional>

class SortedDirIterator {
public:
    using Comparator = std::function<bool(const QFileInfo&, const QFileInfo&)>;

    SortedDirIterator(const QString& rootPath,
                      QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot,
                      Comparator cmp = defaultComparator);

    bool hasNext() const;
    QFileInfo fileInfo() {return m_fileInfo;};
    QFileInfo next();
    QString filePath() { return m_fileInfo.filePath();}
    QString fileName() { return m_fileInfo.fileName();}

private:
    struct Frame {
        QDir dir;
        QString canonicalPath;  // canonical path for cycle detection
        QFileInfoList entries;
        int index = 0;
    };

    QVector<Frame> m_stack;   // directory stack
    std::unordered_set<QString> m_visitedPaths;  // canonical paths on stack (cycle detection)
    QDir::Filters m_filters;
    Comparator m_cmp;
    QFileInfo m_fileInfo;

    bool canEnter(const QString& canonicalPath) const;
    void pushDir(const QString& path);
    void popFrame();
    static bool defaultComparator(const QFileInfo& a, const QFileInfo& b);
};
