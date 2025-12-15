#pragma once

#include <QKeyEvent>
#include <QLineEdit>
class SearchEdit : public QLineEdit
{
  Q_OBJECT
public:
  explicit SearchEdit(QWidget* parent = nullptr);
#include "SearchEdit_decl.inc"

signals:
  void escapePressed();
  void nextMatchRequested();
  void prevMatchRequested();
  void acceptPressed();
};
