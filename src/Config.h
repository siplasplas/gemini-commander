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

  // Editor window geometry (position relative to main window)
  int editorWidth() const { return m_editorWidth; }
  int editorHeight() const { return m_editorHeight; }
  int editorX() const { return m_editorX; }
  int editorY() const { return m_editorY; }
  void setEditorGeometry(int x, int y, int width, int height);

  // Viewer window geometry (position relative to main window)
  int viewerWidth() const { return m_viewerWidth; }
  int viewerHeight() const { return m_viewerHeight; }
  int viewerX() const { return m_viewerX; }
  int viewerY() const { return m_viewerY; }
  void setViewerGeometry(int x, int y, int width, int height);

  // Validate TOML content without loading it
  static bool validateToml(const QString& content, QString& errorMsg);

  // Check if given path is the config file
  bool isConfigFile(const QString& path) const;

  // UI visibility
  bool showFunctionBar() const { return m_showFunctionBar; }
  void setShowFunctionBar(bool show) { m_showFunctionBar = show; }

  // Directory navigation history
  int maxHistorySize() const { return m_maxHistorySize; }
  void setMaxHistorySize(int size) { m_maxHistorySize = size; }

private:
  Config() = default;

  QString m_configPath;
  QVector<FavoriteDir> m_favorites;
  QString m_externalToolPath;
  bool m_confirmExit = true;
  int m_windowWidth = 1024;
  int m_windowHeight = 768;
  int m_windowX = -1;  // -1 means not set (use system default)
  int m_windowY = -1;
  bool m_showFunctionBar = true;
  int m_maxHistorySize = 20;

  // Editor window geometry (relative to main window)
  int m_editorWidth = 800;
  int m_editorHeight = 600;
  int m_editorX = 0;
  int m_editorY = 0;

  // Viewer window geometry (relative to main window)
  int m_viewerWidth = 800;
  int m_viewerHeight = 600;
  int m_viewerX = 0;
  int m_viewerY = 0;
};

#endif
