#ifndef VIEWERWIDGET_H
#define VIEWERWIDGET_H

#include <QWidget>
#include <QFile>
#include <memory>

class QVBoxLayout;

namespace wid {
class TextViewer;
}

// Embeddable viewer widget (can be placed in QStackedWidget)
// In future: will use KTextEditor for small files, TextViewer for large
class ViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewerWidget(QWidget *parent = nullptr);
    ~ViewerWidget() override;

    void openFile(const QString& filePath);
    void clear();

    QString currentFile() const { return m_currentFile; }

private:
    void createTextViewer(uchar* data, qint64 size);
    void clearViewer();

    std::unique_ptr<QFile> m_file;
    wid::TextViewer* m_viewer = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QString m_currentFile;
};

#endif // VIEWERWIDGET_H
