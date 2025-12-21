#pragma once

#include <QPair>

class QString;
class QFileInfo;

// Split filename into base name and extension
// Handles hidden files like ".gitignore" correctly:
// - ".gitignore" -> {".gitignore", ""}
// - ".bashrc.backup" -> {".bashrc", "backup"}
// - "file.txt" -> {"file", "txt"}
// - "file" -> {"file", ""}
// For directories, extension is always empty
QPair<QString, QString> splitFileName(const QFileInfo& info);

enum class ExecutableType {
    ELFBinary,
    ScriptWithShebang,
    TextExecutable,  // Executable text file without shebang
    Unknown
};

ExecutableType getExecutableType(const QString& filePath);
bool isTextFile(const QString& filePath);

