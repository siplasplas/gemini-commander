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
    emit escapePressed();
    return true;
}

bool SearchEdit::doReturnToPanelAndTab(QObject *obj, QKeyEvent *keyEvent) {
    doReturnToPanel(obj, keyEvent);
    // Back in the panel, Tab does what it does there (keys.toml: the other panel).
    QTimer::singleShot(0, [] {
        if (QWidget* panel = QApplication::focusWidget()) {
            QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
            QCoreApplication::sendEvent(panel, &tab);
        }
    });
    return true;
}

bool SearchEdit::doAccept(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    clear();
    emit acceptPressed();
    return true;
}

