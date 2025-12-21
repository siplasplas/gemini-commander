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

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void saveGeometryToConfig();

    std::unique_ptr<QFile> file;
    wid::TextViewer *m_viewer = nullptr;
    QVBoxLayout *m_layout = nullptr;
    bool m_geometryRestored = false;
};

#endif // VIEWERFRAME_H