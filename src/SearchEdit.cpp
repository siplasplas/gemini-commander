#include "SearchEdit.h"

SearchEdit::SearchEdit(QWidget *parent) : QLineEdit(parent) {
}

void SearchEdit::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
        case Qt::Key_Escape:
            emit escapePressed();
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
            emit acceptPressed();
            return;

        default:
            break;
    }

    QLineEdit::keyPressEvent(event);
}

#include "SearchEdit_impl.inc"