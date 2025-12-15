#pragma once

#include <QObject>
#include <QString>
#include <QRegularExpression>
#include <QDateTime>

struct SearchCriteria {
    QString searchPath;
    QString fileNamePattern;      // wildcard pattern (e.g., "*.txt")
    bool fileNameCaseSensitive = false;
    bool partOfName = true;       // add * before and after pattern
    QString containingText;
    bool textCaseSensitive = false;
    bool wholeWords = false;
    qint64 minSize = -1;          // -1 means no limit
    qint64 maxSize = -1;          // -1 means no limit
};

class SearchWorker : public QObject {
    Q_OBJECT

public:
    explicit SearchWorker(const SearchCriteria& criteria, QObject* parent = nullptr);

public slots:
    void startSearch();
    void stopSearch();

signals:
    void resultFound(const QString& path, qint64 size, const QDateTime& modified);
    void searchFinished();
    void progressUpdate(int searchedFiles, int foundFiles);

private:
    bool matchesFileName(const QString& fileName) const;
    bool matchesFileSize(qint64 size) const;
    bool matchesContainingText(const QString& filePath) const;

    SearchCriteria m_criteria;
    QRegularExpression m_fileNameRegex;
    bool m_shouldStop;
};