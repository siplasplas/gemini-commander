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

  QString externalToolPath() const { return m_externalToolPath; }
  void setExternalToolPath(const QString& path) { m_externalToolPath = path; }

  bool confirmExit() const { return m_confirmExit; }
  void setConfirmExit(bool confirm) { m_confirmExit = confirm; }

  // Window geometry
  int windowWidth() const { return m_windowWidth; }
  int windowHeight() const { return m_windowHeight; }
  int windowX() const { return m_windowX; }
  int windowY() const { return m_windowY; }
  void setWindowGeometry(int x, int y, int width, int height);

  // Validate TOML content without loading it
  static bool validateToml(const QString& content, QString& errorMsg);

  // Check if given path is the config file
  bool isConfigFile(const QString& path) const;

  // UI visibility
  bool showFunctionBar() const { return m_showFunctionBar; }
  void setShowFunctionBar(bool show) { m_showFunctionBar = show; }

private:
  Config() = default;

  QString m_configPath;
  QVector<FavoriteDir> m_favorites;
  IconMode m_iconMode = IconMode::Extension;
  QString m_externalToolPath;
  bool m_confirmExit = true;
  int m_windowWidth = 1024;
  int m_windowHeight = 768;
  int m_windowX = -1;  // -1 means not set (use system default)
  int m_windowY = -1;
  bool m_showFunctionBar = true;
};

#endif
