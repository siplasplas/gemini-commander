#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QStringList>
#include <QVector>

struct FavoriteDir {
  QString path;
  QString label;
  QString group;
};

class Config
{
public:
  static Config& instance();

  QString defaultConfigPath() const;

  bool load(const QString& path);
  bool save() const;

  const QVector<FavoriteDir>& favoriteDirs() const { return m_favorites; }

  bool containsFavoriteDir(const QString& dir) const;
  void addFavoriteDir(const QString& dir,
                      const QString& label = QString(),
                      const QString& group = QString());

  void setConfigPath(const QString& p) { m_configPath = p; }
  QString configPath() const { return m_configPath; }

private:
  Config() = default;

  QString m_configPath;
  QVector<FavoriteDir> m_favorites;
};

#endif
