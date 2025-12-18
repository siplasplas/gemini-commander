#pragma once

class QString;
enum class ExecutableType {
    ELFBinary,
    ScriptWithShebang,
    TextExecutable,  // Executable text file without shebang
    Unknown
};

ExecutableType getExecutableType(const QString& filePath);
bool isTextFile(const QString& filePath);

