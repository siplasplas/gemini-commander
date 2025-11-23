#ifndef VIEWER_H
#define VIEWER_H
#include <QFile>

#include "BaseViewer.h"
#include "../../external/textviewers/wid/TextViewer.h"

class Viewer : public BaseViewer {
    Q_OBJECT
    wid::TextViewer *widget = nullptr;
    std::unique_ptr<QFile> file;
public:
    explicit Viewer(const QString& fileName, QWidget *parent = nullptr);
signals:
    // Emitted when user presses ESC and viewer requests closing its tab
    void closeRequested();
protected:
    void keyPressEvent(QKeyEvent* event) override;
};

#endif //VIEWER_H
