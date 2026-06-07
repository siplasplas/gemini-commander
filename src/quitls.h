#pragma once

#include <QPair>

#include "SizeFormat.h"

class QString;
class QFileInfo;

QString qMakeTempPartPath(const QString& path, bool pathIsDir);
QString qFormatSize(std::size_t value, SizeFormat::SizeKind format);
QString formatWithSeparators(std::size_t value);

bool isDarkTheme();

QString qEscapePathForShell(const QString& path);

// Return the last `count` components of a path joined with '/'.
// Used for tab popup text, e.g. "/home/a/dir3/dir4" with count=2 -> "dir3/dir4".
// Root "/" returns "/"; paths with fewer components return what's available.
QString qLastPathComponents(const QString& path, int count = 2);

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

// Finalize copied file: preserve modification time and sync to disk
void finalizeCopiedFile(const QString& srcPath, const QString& dstPath);

// Check if two paths are on the same filesystem/partition
// Used to determine if rename() will work or if copy+delete is needed
bool areOnSameFilesystem(const QString& path1, const QString& path2);

