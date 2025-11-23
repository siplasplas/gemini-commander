// Created by andrzej on 11/23/25.
//

#ifndef GEMINI_COMMANDER_SEARCHLINEEDIT_H
#define GEMINI_COMMANDER_SEARCHLINEEDIT_H
#include <QKeyEvent>
#include <QLineEdit>
class SearchLineEdit : public QLineEdit
{
  Q_OBJECT
public:
  explicit SearchLineEdit(QWidget* parent = nullptr)
      : QLineEdit(parent)
  {}

signals:
  void escapePressed();
  void nextMatchRequested();
  void prevMatchRequested();
  void acceptPressed();

protected:
  void keyPressEvent(QKeyEvent* event) override
  {
    switch (event->key()) {
    case Qt::Key_Escape:
      emit escapePressed();
      // NIE wołamy QLineEdit::keyPressEvent, żeby ESC nie szedł dalej
      return;
    case Qt::Key_Down:
    case Qt::Key_PageDown:
      emit nextMatchRequested();
      return;
    case Qt::Key_Up:
    case Qt::Key_PageUp:
      emit prevMatchRequested();
      return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      emit acceptPressed();   // Enter zatwierdza wybór
      return;

    default:
      break;
    }

    QLineEdit::keyPressEvent(event);
  }
};


#endif // GEMINI_COMMANDER_SEARCHLINEEDIT_H
