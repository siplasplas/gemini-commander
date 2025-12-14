#ifndef VIEWERFRAME_H
#define VIEWERFRAME_H

#include <QDialog>

class Viewer;
class QVBoxLayout;

class ViewerFrame : public QDialog
{
    Q_OBJECT

public:
    explicit ViewerFrame(const QString& filePath, QWidget *parent = nullptr);
    ~ViewerFrame() override;

private:
    Viewer *m_viewer = nullptr;
    QVBoxLayout *m_layout = nullptr;
};

#endif // VIEWERFRAME_H