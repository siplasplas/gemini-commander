#ifndef FILEPANEWIDGET_H
#define FILEPANEWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QStringList>
#include <QToolButton>

#include "FilePanel.h"

class SearchEdit;
class FilePaneWidget : public QWidget
{
  Q_OBJECT
public:
  FilePaneWidget(Side side, QWidget* parent = nullptr);

  FilePanel* filePanel() const { return m_filePanel; }
  QLineEdit* pathEdit() const { return m_pathEdit; }

  void setCurrentPath(const QString& path);
  QString currentPath() const;

   Q_INVOKABLE bool doLocalSearch(QObject *obj, QKeyEvent *keyEvent);

  // PathEdit handlers
  Q_INVOKABLE bool doRestoreAndReturnToPanel(QObject *obj, QKeyEvent *keyEvent);
  Q_INVOKABLE bool doNavigateOrRestore(QObject *obj, QKeyEvent *keyEvent);

  // Directory navigation history
  bool canGoBack() const;
  bool canGoForward() const;
  void goBack();
  void goForward();

signals:
  void favoritesRequested(const QPoint& pos);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
  void onDirectoryChanged(const QString& path);
  void onSelectionChanged();

private slots:
  void showHistoryMenu();

private:
  Side m_side;
  QToolButton* m_favoritesButton = nullptr;
  QLineEdit* m_pathEdit = nullptr;
  QToolButton* m_historyButton = nullptr;
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
  void navigateToHistoryIndex(int index);
  void restorePathEdit();
};

#endif // FILEPANEWIDGET_H
