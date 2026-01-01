#ifndef VIEWERWIDGET_H
#define VIEWERWIDGET_H

#include <QWidget>
#include <QFile>
#include <memory>

class QVBoxLayout;

namespace wid {
class TextViewer;
}

namespace KTextEditor {
class Document;
class View;
}

// Embeddable viewer widget (can be placed in QStackedWidget)
// Uses wid::TextViewer for small files, KTextEditor for large files
class ViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewerWidget(QWidget *parent = nullptr);
    ~ViewerWidget() override;

    void openFile(const QString& filePath);
    void clear();

    QString currentFile() const { return m_currentFile; }

    // Threshold for switching between viewers (bytes)
    static constexpr qint64 SmallFileThreshold = 70 * 1024;  // 70 KB

private:
    void createTextViewer(uchar* data, qint64 size);
    void createKTextEditorView(const QString& filePath);
    void clearViewer();

    std::unique_ptr<QFile> m_file;
    wid::TextViewer* m_textViewer = nullptr;
    KTextEditor::Document* m_kteDocument = nullptr;
    KTextEditor::View* m_kteView = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QString m_currentFile;
};

#endif // VIEWERWIDGET_H
