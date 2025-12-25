#ifndef FILEPANEWIDGET_H
#define FILEPANEWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QStringList>

#include "FilePanel.h"

class SearchEdit;
class FilePaneWidget : public QWidget
{
  Q_OBJECT
public:
  FilePaneWidget(Side side, QWidget* parent = nullptr);

  FilePanel* filePanel() const { return m_filePanel; }

  void setCurrentPath(const QString& path);
  QString currentPath() const;

   Q_INVOKABLE bool doLocalSearch(QObject *obj, QKeyEvent *keyEvent);

  // Directory navigation history
  bool canGoBack() const;
  bool canGoForward() const;
  void goBack();
  void goForward();

public slots:
  void onDirectoryChanged(const QString& path);
  void onSelectionChanged();

private:
  Side m_side;
  QLineEdit* m_pathEdit = nullptr;
  FilePanel* m_filePanel = nullptr;
  QLabel*    m_statusLabel = nullptr;

  SearchEdit* m_searchEdit = nullptr;

  void updateStatusLabel();

  // Directory navigation history
  QStringList m_history;
  int m_historyPosition = -1;
  bool m_navigatingHistory = false;

  void addToHistory(const QString& path);
  void trimHistoryToLimit();
};

#endif // FILEPANEWIDGET_H
