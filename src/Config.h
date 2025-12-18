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

enum class IconMode {
  Extension,  // current behavior: icon by file extension via QMimeDatabase
  FileType,   // icon by file type category (executable, text, image, etc.)
  AppIcon     // icon from associated application (future: + thumbnails)
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

  IconMode iconMode() const { return m_iconMode; }
  void setIconMode(IconMode mode) { m_iconMode = mode; }

private:
  Config() = default;

  QString m_configPath;
  QVector<FavoriteDir> m_favorites;
  IconMode m_iconMode = IconMode::Extension;
};

#endif
