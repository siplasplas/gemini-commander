#include "quitls.h"
#include <QFile>
#include <QFileInfo>

bool isTextFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Check first 512 bytes for non-text characters
    QByteArray sample = file.read(512);
    file.close();

    if (sample.isEmpty()) {
        return false;
    }

    // Simple heuristic: if most bytes are printable, it's text
    int printable = 0;
    int total = sample.size();

    for (char c : sample) {
        if ((c >= 0x20 && c <= 0x7E) || c == '\n' || c == '\r' || c == '\t') {
            printable++;
        }
    }

    return (printable * 100 / total) > 85;  // >85% printable = text
}


ExecutableType getExecutableType(const QString& filePath) {
    QFileInfo fileInfo(filePath);

    if (!fileInfo.isExecutable()) {
        return ExecutableType::Unknown;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return ExecutableType::Unknown;
    }

    QByteArray header = file.read(4);
    file.close();

    if (header.size() < 2) {
        return ExecutableType::Unknown;
    }

    // Check for ELF magic bytes
    if (header.size() >= 4 &&
        header[0] == 0x7f &&
        header[1] == 'E' &&
        header[2] == 'L' &&
        header[3] == 'F') {
        return ExecutableType::ELFBinary;
        }

    // Check for shebang
    if (header[0] == '#' && header[1] == '!') {
        return ExecutableType::ScriptWithShebang;
    }

    // If it's text but no shebang, it's probably a shell script
    if (isTextFile(filePath)) {
        return ExecutableType::TextExecutable;
    }

    return ExecutableType::Unknown;
}