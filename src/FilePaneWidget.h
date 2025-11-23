#ifndef FILEPANEWIDGET_H
#define FILEPANEWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>

#include "FilePanel.h"

class SearchLineEdit;
class FilePaneWidget : public QWidget
{
  Q_OBJECT
public:
  explicit FilePaneWidget(QWidget* parent = nullptr);

  FilePanel* filePanel() const { return m_filePanel; }

  // helpery do ustawiania katalogu / aktualizacji UI
  void setCurrentPath(const QString& path);
  QString currentPath() const;

public slots:
  void onDirectoryChanged(const QString& path);
void onSelectionChanged();

private:
  QLineEdit* m_pathEdit = nullptr;
  FilePanel* m_filePanel = nullptr;
  QLabel*    m_statusLabel = nullptr;

  SearchLineEdit* m_searchEdit = nullptr;

  void updateStatusLabel();
};

#endif // FILEPANEWIDGET_H
