#pragma once

#include "SearchIgnore.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

/**
 * @struct PanelFilter
 * @brief What a file panel shows (Show menu: All Files / Programs / Custom)
 *
 * Directories are always shown, except those .gitignore files ignore.
 */
struct PanelFilter {
    enum class Mode { AllFiles, Programs, Custom };
    Mode mode = Mode::AllFiles;
    QStringList masks;          ///< Custom: file name masks, e.g. *.cpp;*.h (none = all files)
    bool useGitignore = false;  ///< Custom: hide what .gitignore files ignore

    /// Shows everything (no matcher needed).
    bool showsAll() const;
    /// Short text shown after the panel path, e.g. "*.cpp;*.h;!gitignore"; empty for All Files.
    QString description() const;
};

/**
 * @class PanelFilterMatcher
 * @brief Applies a PanelFilter to the entries under one directory
 *
 * Used for the panel listing, the branch view and directory sizes (Space,
 * Alt+Shift+Enter), so all of them show and count the same files. Hidden
 * directories (.gitignore) are not entered. Not thread-safe.
 */
class PanelFilterMatcher {
public:
    /// root: the panel directory; .gitignore files are read as SearchIgnore does.
    PanelFilterMatcher(const PanelFilter& filter, const QString& root);

    bool accepts(const QFileInfo& entry);

    /// Programs: executable by the user on Linux, an .exe/.com/.bat/.cmd/.ps1 on Windows.
    static bool isProgram(const QFileInfo& file);

private:
    PanelFilter::Mode m_mode;
    QVector<QRegularExpression> m_masks;
    std::optional<SearchIgnore> m_ignore;
};
