#include "Config.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>

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

bool Config::load(const QString& path)
{
    m_configPath = path;
    m_favorites.clear();

    QFile f(path);
    if (!f.exists()) {
        qDebug() << "Config file does not exist, using empty hotlist.";
        return true;
    }

    try {
        auto tbl = toml::parse_file(path.toStdString());

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

bool Config::save() const
{
    if (m_configPath.isEmpty())
        return false;

    toml::table tbl;
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
