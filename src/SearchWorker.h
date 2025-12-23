#pragma once

#include <QObject>
#include <QString>
#include <QRegularExpression>
#include <QDateTime>
#include <QVector>

enum class ItemTypeFilter {
    FilesAndDirectories,  // Search both files and directories
    FilesOnly,            // Only files
    DirectoriesOnly       // Only directories
};

enum class FileContentFilter {
    Any,           // Don't check file content (can also search directories)
    TextFile,      // Text file (not binary)
    ELFBinary,     // ELF executable
    HasShebang,    // Script with #! header
    ZeroFilled     // Zero-filled file (USB write failure)
};

struct SearchCriteria {
    QString searchPath;
    QString fileNamePattern;      // wildcard pattern (e.g., "*.txt")
    bool fileNameCaseSensitive = false;
    bool partOfName = true;       // add * before and after pattern
    bool negateFileName = false;  // Invert filename match

    QString containingText;
    bool textCaseSensitive = false;
    bool wholeWords = false;
    bool negateContainingText = false;  // Invert text content match

    qint64 minSize = -1;          // -1 means no limit
    qint64 maxSize = -1;          // -1 means no limit

    ItemTypeFilter itemTypeFilter = ItemTypeFilter::FilesAndDirectories;  // Replaces directoriesOnly

    // File content filter (unified: text, ELF, shebang, zero-filled)
    FileContentFilter fileContentFilter = FileContentFilter::Any;

    // Executable bits filter
    enum class ExecutableBitsFilter {
        NotSpecified,     // Don't check (default)
        Executable,       // Any exec bit set
        NotExecutable,    // No exec bits set
        AllExecutable     // Owner+Group+Other all set
    };
    ExecutableBitsFilter executableBits = ExecutableBitsFilter::NotSpecified;

    // Search in results mode
    bool searchInResults = false;       // Hybrid filtering mode
    QVector<QString> previousResultPaths;  // Full paths from previous search
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
    bool matchesItemType(bool isDir, bool isFile) const;
    bool matchesFileContentFilter(const QString& filePath, qint64 fileSize) const;
    bool matchesExecutableBits(const QString& filePath) const;

    SearchCriteria m_criteria;
    QRegularExpression m_fileNameRegex;
    bool m_shouldStop;
};