#ifndef FILEPANEWIDGET_H
#define FILEPANEWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>

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
};

#endif // FILEPANEWIDGET_H
