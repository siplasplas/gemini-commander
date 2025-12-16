bool SearchEdit::doNextMatch(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    emit nextMatchRequested();
    return true;
}

bool SearchEdit::doPrevMatch(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    emit prevMatchRequested();
    return true;
}

bool SearchEdit::doReturnToPanel(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    clear();
    hide();
    return true;
}

bool SearchEdit::doAccept(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    clear();
    emit acceptPressed();
    return true;
}

