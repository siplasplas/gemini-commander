#pragma once

#include <QDir>
#include <QFileInfo>
#include <QVector>
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
        QFileInfoList entries;
        int index = 0;
    };

    QVector<Frame> m_stack;   // stos katalogów
    QDir::Filters m_filters;
    Comparator m_cmp;
    QFileInfo m_fileInfo;

    void pushDir(const QString& path);
    static bool defaultComparator(const QFileInfo& a, const QFileInfo& b);
};
