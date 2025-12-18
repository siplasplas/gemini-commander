#pragma once

#include <QDir>
#include <QFileInfo>
#include <QVector>
#include <unordered_set>
#include <functional>

class SortedDirIterator {
public:
    using Comparator = std::function<bool(const QFileInfo&, const QFileInfo&)>;

    enum class Options {
        None = 0,
        FollowSymlinks = 1 << 0,     // Follow symbolic links to directories
        DetectCycles = 1 << 1         // Detect and prevent directory cycles
    };

    // Enable bitwise operations on Options
    friend inline Options operator|(Options a, Options b) {
        return static_cast<Options>(static_cast<int>(a) | static_cast<int>(b));
    }
    friend inline Options operator&(Options a, Options b) {
        return static_cast<Options>(static_cast<int>(a) & static_cast<int>(b));
    }
    friend inline bool hasFlag(Options options, Options flag) {
        return (options & flag) == flag;
    }

    SortedDirIterator(const QString& rootPath,
                      QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot,
                      Comparator cmp = defaultComparator,
                      Options options = Options::None);

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
    Options m_options;
    QFileInfo m_fileInfo;

    bool canEnter(const QString& canonicalPath) const;
    void pushDir(const QString& path);
    void popFrame();
    static bool defaultComparator(const QFileInfo& a, const QFileInfo& b);
};
