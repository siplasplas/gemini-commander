#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QStringList>
#include <QVector>
#include "SizeFormat.h"

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

  // Panel sorting (column: "Name", "Ext", "Size", "Date"; order: 0=Asc, 1=Desc)
  QString leftSortColumn() const { return m_leftSortColumn; }
  int leftSortOrder() const { return m_leftSortOrder; }
  void setLeftSort(const QString& column, int order) { m_leftSortColumn = column; m_leftSortOrder = order; }

  QString rightSortColumn() const { return m_rightSortColumn; }
  int rightSortOrder() const { return m_rightSortOrder; }
  void setRightSort(const QString& column, int order) { m_rightSortColumn = column; m_rightSortOrder = order; }

  // Panel columns configuration
  QStringList leftPanelColumns() const { return m_leftColumns; }
  QVector<double> leftPanelProportions() const { return m_leftProportions; }
  void setLeftPanelColumns(const QStringList& cols, const QVector<double>& props) {
      m_leftColumns = cols; m_leftProportions = props;
  }

  QStringList rightPanelColumns() const { return m_rightColumns; }
  QVector<double> rightPanelProportions() const { return m_rightProportions; }
  void setRightPanelColumns(const QStringList& cols, const QVector<double>& props) {
      m_rightColumns = cols; m_rightProportions = props;
  }

  // Available column names (static)
  static QStringList availableColumns() { return {"Name", "Ext", "Size", "Date", "Attr"}; }
  static QStringList defaultColumns() { return {"Name", "Ext", "Size", "Date"}; }
  static QVector<double> defaultProportions() { return {0.40, 0.14, 0.20, 0.26}; }

  // Tab directories configuration
  QStringList leftTabDirs() const { return m_leftTabDirs; }
  int leftTabIndex() const { return m_leftTabIndex; }
  void setLeftTabs(const QStringList& dirs, int index) { m_leftTabDirs = dirs; m_leftTabIndex = index; }

  QStringList rightTabDirs() const { return m_rightTabDirs; }
  int rightTabIndex() const { return m_rightTabIndex; }
  void setRightTabs(const QStringList& dirs, int index) { m_rightTabDirs = dirs; m_rightTabIndex = index; }

  // MRU tabs configuration
  int maxUnpinnedTabs() const { return m_maxUnpinnedTabs; }
  void setMaxUnpinnedTabs(int max) { m_maxUnpinnedTabs = max; }

  // Size display format
  SizeFormat::SizeKind sizeFormat() const { return m_sizeFormat; }
  void setSizeFormat(SizeFormat::SizeKind format) { m_sizeFormat = format; }

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

  // Panel sorting (defaults: Date, Descending)
  QString m_leftSortColumn = "Date";
  int m_leftSortOrder = 1;    // Qt::DescendingOrder
  QString m_rightSortColumn = "Date";
  int m_rightSortOrder = 1;

  // Panel columns (defaults: Name, Ext, Size, Date)
  QStringList m_leftColumns = {"Name", "Ext", "Size", "Date"};
  QVector<double> m_leftProportions = {0.40, 0.14, 0.20, 0.26};
  QStringList m_rightColumns = {"Name", "Ext", "Size", "Date"};
  QVector<double> m_rightProportions = {0.40, 0.14, 0.20, 0.26};

  // Tab directories (empty means use default/cwd)
  QStringList m_leftTabDirs;
  int m_leftTabIndex = 0;
  QStringList m_rightTabDirs;
  int m_rightTabIndex = 0;

  // MRU tabs
  int m_maxUnpinnedTabs = 10;

  // Size display format
  SizeFormat::SizeKind m_sizeFormat = SizeFormat::Decimal;
};

#endif
