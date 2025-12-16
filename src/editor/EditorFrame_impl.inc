bool EditorFrame::doCloseRequest(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    close();
    return true;
}