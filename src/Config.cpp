#include "Config.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>
#include <cmath>

#include <toml++/toml.h>

QString Config::defaultConfigPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (base.isEmpty())
        base = QDir::homePath() + "/.config";

    QDir dir(base + "/gemini-commander");
    if (!dir.exists())
        dir.mkpath(".");

    return dir.filePath("config.toml");
}

Config& Config::instance()
{
    static Config cfg;
    return cfg;
}

bool Config::containsFavoriteDir(const QString& dir) const
{
    QString clean = QDir::cleanPath(dir);
    for (const auto& f : m_favorites)
        if (QDir::cleanPath(f.path) == clean)
            return true;
    return false;
}

void Config::addFavoriteDir(const QString& dir, const QString& label, const QString& group)
{
    QString clean = QDir::cleanPath(dir);
    if (clean.isEmpty())
        return;
    if (containsFavoriteDir(clean))
        return;

    FavoriteDir f;
    f.path  = clean;      // już wyczyszczone
    f.label = label;
    f.group = group;
    m_favorites.append(f);
}

void Config::setWindowGeometry(int x, int y, int width, int height)
{
    m_windowX = x;
    m_windowY = y;
    m_windowWidth = width;
    m_windowHeight = height;
}

void Config::setEditorGeometry(int x, int y, int width, int height)
{
    m_editorX = x;
    m_editorY = y;
    m_editorWidth = width;
    m_editorHeight = height;
}

void Config::setViewerGeometry(int x, int y, int width, int height)
{
    m_viewerX = x;
    m_viewerY = y;
    m_viewerWidth = width;
    m_viewerHeight = height;
}

bool Config::load(const QString& path)
{
    m_configPath = path;
    m_favorites.clear();
    m_externalToolPath.clear();
    m_confirmExit = true;
    m_windowWidth = 1024;
    m_windowHeight = 768;
    m_windowX = -1;
    m_windowY = -1;
    m_showFunctionBar = true;
    m_editorWidth = 800;
    m_editorHeight = 600;
    m_editorX = 0;
    m_editorY = 0;
    m_viewerWidth = 800;
    m_viewerHeight = 600;
    m_viewerX = 0;
    m_viewerY = 0;
    m_leftTabDirs.clear();
    m_leftTabIndex = 0;
    m_rightTabDirs.clear();
    m_rightTabIndex = 0;

    QFile f(path);
    if (!f.exists()) {
        qDebug() << "Config file does not exist, using empty hotlist.";
        return true;
    }

    try {
        auto tbl = toml::parse_file(path.toStdString());

        // [external_tool] section
        if (tbl.contains("external_tool")) {
            auto& tool = *tbl["external_tool"].as_table();
            if (auto path = tool["path"].value<std::string>())
                m_externalToolPath = QString::fromStdString(*path);
        }

        // [general] section
        if (tbl.contains("general")) {
            auto& general = *tbl["general"].as_table();
            if (auto confirm = general["confirm_exit"].value<bool>())
                m_confirmExit = *confirm;
        }

        // [window] section
        if (tbl.contains("window")) {
            auto& window = *tbl["window"].as_table();
            if (auto w = window["width"].value<int64_t>())
                m_windowWidth = static_cast<int>(*w);
            if (auto h = window["height"].value<int64_t>())
                m_windowHeight = static_cast<int>(*h);
            if (auto x = window["x"].value<int64_t>())
                m_windowX = static_cast<int>(*x);
            if (auto y = window["y"].value<int64_t>())
                m_windowY = static_cast<int>(*y);
        }

        // [ui] section
        if (tbl.contains("ui")) {
            auto& ui = *tbl["ui"].as_table();
            if (auto show = ui["showFunctionBar"].value<bool>())
                m_showFunctionBar = *show;
        }

        // [history] section
        if (tbl.contains("history")) {
            auto& history = *tbl["history"].as_table();
            if (auto size = history["max_size"].value<int64_t>())
                m_maxHistorySize = static_cast<int>(*size);
        }

        // [editor] section (position relative to main window)
        if (tbl.contains("editor")) {
            auto& editor = *tbl["editor"].as_table();
            if (auto w = editor["width"].value<int64_t>())
                m_editorWidth = static_cast<int>(*w);
            if (auto h = editor["height"].value<int64_t>())
                m_editorHeight = static_cast<int>(*h);
            if (auto x = editor["x"].value<int64_t>())
                m_editorX = static_cast<int>(*x);
            if (auto y = editor["y"].value<int64_t>())
                m_editorY = static_cast<int>(*y);
        }

        // [viewer] section (position relative to main window)
        if (tbl.contains("viewer")) {
            auto& viewer = *tbl["viewer"].as_table();
            if (auto w = viewer["width"].value<int64_t>())
                m_viewerWidth = static_cast<int>(*w);
            if (auto h = viewer["height"].value<int64_t>())
                m_viewerHeight = static_cast<int>(*h);
            if (auto x = viewer["x"].value<int64_t>())
                m_viewerX = static_cast<int>(*x);
            if (auto y = viewer["y"].value<int64_t>())
                m_viewerY = static_cast<int>(*y);
        }

        // [panels] section - sorting settings and columns
        if (tbl.contains("panels")) {
            auto& panels = *tbl["panels"].as_table();
            if (auto col = panels["left_sort_column"].value<std::string>())
                m_leftSortColumn = QString::fromStdString(*col);
            if (auto ord = panels["left_sort_order"].value<int64_t>())
                m_leftSortOrder = static_cast<int>(*ord);
            if (auto col = panels["right_sort_column"].value<std::string>())
                m_rightSortColumn = QString::fromStdString(*col);
            if (auto ord = panels["right_sort_order"].value<int64_t>())
                m_rightSortOrder = static_cast<int>(*ord);

            // Left panel columns
            if (panels.contains("left_columns") && panels["left_columns"].is_array()) {
                QStringList cols;
                for (const auto& node : *panels["left_columns"].as_array()) {
                    if (auto s = node.value<std::string>())
                        cols.append(QString::fromStdString(*s));
                }
                if (!cols.isEmpty())
                    m_leftColumns = cols;
            }
            if (panels.contains("left_proportions") && panels["left_proportions"].is_array()) {
                QVector<double> props;
                for (const auto& node : *panels["left_proportions"].as_array()) {
                    if (auto i = node.value<int64_t>())
                        props.append(*i / 100.0);
                }
                if (props.size() == m_leftColumns.size())
                    m_leftProportions = props;
            }

            // Right panel columns
            if (panels.contains("right_columns") && panels["right_columns"].is_array()) {
                QStringList cols;
                for (const auto& node : *panels["right_columns"].as_array()) {
                    if (auto s = node.value<std::string>())
                        cols.append(QString::fromStdString(*s));
                }
                if (!cols.isEmpty())
                    m_rightColumns = cols;
            }
            if (panels.contains("right_proportions") && panels["right_proportions"].is_array()) {
                QVector<double> props;
                for (const auto& node : *panels["right_proportions"].as_array()) {
                    if (auto i = node.value<int64_t>())
                        props.append(*i / 100.0);
                }
                if (props.size() == m_rightColumns.size())
                    m_rightProportions = props;
            }
        }

        // [tabs] section - tab directories
        if (tbl.contains("tabs")) {
            auto& tabs = *tbl["tabs"].as_table();

            // Left tabs
            if (tabs.contains("left_dirs") && tabs["left_dirs"].is_array()) {
                for (const auto& node : *tabs["left_dirs"].as_array()) {
                    if (auto s = node.value<std::string>())
                        m_leftTabDirs.append(QString::fromStdString(*s));
                }
            }
            if (auto idx = tabs["left_index"].value<int64_t>())
                m_leftTabIndex = static_cast<int>(*idx);

            // Right tabs
            if (tabs.contains("right_dirs") && tabs["right_dirs"].is_array()) {
                for (const auto& node : *tabs["right_dirs"].as_array()) {
                    if (auto s = node.value<std::string>())
                        m_rightTabDirs.append(QString::fromStdString(*s));
                }
            }
            if (auto idx = tabs["right_index"].value<int64_t>())
                m_rightTabIndex = static_cast<int>(*idx);
        }

        if (tbl.contains("favorites")) {
            auto favs = *tbl["favorites"].as_array();

            for (auto& node : favs) {
                if (!node.is_table()) continue;

                auto& t = *node.as_table();

                FavoriteDir fav;

                if (auto p = t["path"].value<std::string>())
                    fav.path = QString::fromStdString(*p);
                else
                    continue;

                if (auto l = t["label"].value<std::string>())
                    fav.label = QString::fromStdString(*l);

                if (auto g = t["group"].value<std::string>())
                    fav.group = QString::fromStdString(*g);
                else
                    fav.group = "";

                m_favorites.append(fav);
            }
        }
    }
    catch (const std::exception& e) {
        qWarning() << "Failed to parse config.toml:" << e.what();
        return false;
    }

    return true;
}

bool Config::validateToml(const QString& content, QString& errorMsg)
{
    try {
        auto pares_result = toml::parse(content.toStdString());
        return true;
    }
    catch (const toml::parse_error& e) {
        errorMsg = QString::fromStdString(std::string(e.description()));
        if (e.source().begin.line > 0) {
            errorMsg += QString(" (line %1)").arg(e.source().begin.line);
        }
        return false;
    }
    catch (const std::exception& e) {
        errorMsg = QString::fromUtf8(e.what());
        return false;
    }
}

bool Config::isConfigFile(const QString& path) const
{
    if (m_configPath.isEmpty() || path.isEmpty())
        return false;
    return QFileInfo(path).absoluteFilePath() == QFileInfo(m_configPath).absoluteFilePath();
}

bool Config::save() const
{
    if (m_configPath.isEmpty())
        return false;

    toml::table tbl;

    // [general] section
    toml::table generalTbl;
    generalTbl.insert("confirm_exit", m_confirmExit);
    tbl.insert("general", generalTbl);

    // [window] section
    toml::table windowTbl;
    windowTbl.insert("width", static_cast<int64_t>(m_windowWidth));
    windowTbl.insert("height", static_cast<int64_t>(m_windowHeight));
    windowTbl.insert("x", static_cast<int64_t>(m_windowX));
    windowTbl.insert("y", static_cast<int64_t>(m_windowY));
    tbl.insert("window", windowTbl);

    // [ui] section
    toml::table uiTbl;
    uiTbl.insert("showFunctionBar", m_showFunctionBar);
    tbl.insert("ui", uiTbl);

    // [history] section
    toml::table historyTbl;
    historyTbl.insert("max_size", static_cast<int64_t>(m_maxHistorySize));
    tbl.insert("history", historyTbl);

    // [editor] section (position relative to main window)
    toml::table editorTbl;
    editorTbl.insert("width", static_cast<int64_t>(m_editorWidth));
    editorTbl.insert("height", static_cast<int64_t>(m_editorHeight));
    editorTbl.insert("x", static_cast<int64_t>(m_editorX));
    editorTbl.insert("y", static_cast<int64_t>(m_editorY));
    tbl.insert("editor", editorTbl);

    // [viewer] section (position relative to main window)
    toml::table viewerTbl;
    viewerTbl.insert("width", static_cast<int64_t>(m_viewerWidth));
    viewerTbl.insert("height", static_cast<int64_t>(m_viewerHeight));
    viewerTbl.insert("x", static_cast<int64_t>(m_viewerX));
    viewerTbl.insert("y", static_cast<int64_t>(m_viewerY));
    tbl.insert("viewer", viewerTbl);

    // [panels] section - sorting settings and columns
    toml::table panelsTbl;
    panelsTbl.insert("left_sort_column", m_leftSortColumn.toStdString());
    panelsTbl.insert("left_sort_order", static_cast<int64_t>(m_leftSortOrder));
    panelsTbl.insert("right_sort_column", m_rightSortColumn.toStdString());
    panelsTbl.insert("right_sort_order", static_cast<int64_t>(m_rightSortOrder));

    // Left panel columns and proportions
    toml::array leftColsArr, leftPropsArr;
    for (const QString& col : m_leftColumns)
        leftColsArr.push_back(col.toStdString());
    for (double prop : m_leftProportions)
        leftPropsArr.push_back(static_cast<int64_t>(std::round(prop * 100.0)));
    panelsTbl.insert("left_columns", leftColsArr);
    panelsTbl.insert("left_proportions", leftPropsArr);

    // Right panel columns and proportions
    toml::array rightColsArr, rightPropsArr;
    for (const QString& col : m_rightColumns)
        rightColsArr.push_back(col.toStdString());
    for (double prop : m_rightProportions)
        rightPropsArr.push_back(static_cast<int64_t>(std::round(prop * 100.0)));
    panelsTbl.insert("right_columns", rightColsArr);
    panelsTbl.insert("right_proportions", rightPropsArr);

    tbl.insert("panels", panelsTbl);

    // [tabs] section - tab directories
    toml::table tabsTbl;
    toml::array leftDirsArr, rightDirsArr;
    for (const QString& dir : m_leftTabDirs)
        leftDirsArr.push_back(dir.toStdString());
    for (const QString& dir : m_rightTabDirs)
        rightDirsArr.push_back(dir.toStdString());
    tabsTbl.insert("left_dirs", leftDirsArr);
    tabsTbl.insert("left_index", static_cast<int64_t>(m_leftTabIndex));
    tabsTbl.insert("right_dirs", rightDirsArr);
    tabsTbl.insert("right_index", static_cast<int64_t>(m_rightTabIndex));
    tbl.insert("tabs", tabsTbl);

    // [external_tool] section
    if (!m_externalToolPath.isEmpty()) {
        toml::table toolTbl;
        toolTbl.insert("path", m_externalToolPath.toStdString());
        tbl.insert("external_tool", toolTbl);
    }

    // [[favorites]] array
    toml::array arr;
    for (const auto& f : m_favorites) {
        toml::table t;

        t.insert("path",  f.path.toStdString());
        t.insert("label", f.label.toStdString());
        t.insert("group", f.group.toStdString());

        arr.push_back(t);
    }

    tbl.insert("favorites", arr);

    try {
        std::ofstream out(m_configPath.toStdString());
        out << tbl;
    }
    catch (...) {
        qWarning() << "Failed to save TOML config";
        return false;
    }

    return true;
}
