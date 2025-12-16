bool Viewer::doClose(QObject *obj, QKeyEvent *keyEvent) {
    Q_UNUSED(obj);
    Q_UNUSED(keyEvent);
    if (auto* dialog = qobject_cast<QDialog*>(window())) {
        dialog->close();
        return true;
    }
    return false;
}

