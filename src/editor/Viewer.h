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
#include "Viewer_decl.inc"
};

#endif //VIEWER_H
