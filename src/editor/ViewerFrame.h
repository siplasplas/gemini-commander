#ifndef VIEWERFRAME_H
#define VIEWERFRAME_H

#include <QDialog>

class QFile;

namespace wid {
class TextViewer;
}
class Viewer;
class QVBoxLayout;

class ViewerFrame : public QDialog
{
    Q_OBJECT

public:
    explicit ViewerFrame(const QString& filePath, QWidget *parent = nullptr);
    ~ViewerFrame() override;

    void openFile(const QString& filePath);

private:
    std::unique_ptr<QFile> file;
    wid::TextViewer *m_viewer = nullptr;
    QVBoxLayout *m_layout = nullptr;
};

#endif // VIEWERFRAME_H