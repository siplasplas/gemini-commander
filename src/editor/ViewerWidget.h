#ifndef VIEWERWIDGET_H
#define VIEWERWIDGET_H

#include <QWidget>
#include <QFile>
#include <memory>

class QVBoxLayout;
class HexViewWidget;

namespace wid {
class TextViewer;
}

namespace KTextEditor {
class Document;
class View;
}

// Embeddable viewer widget (can be placed in QStackedWidget)
// Supports Text mode (wid::TextViewer or KTextEditor) and Hex mode
class ViewerWidget : public QWidget
{
    Q_OBJECT

public:
    enum class ViewMode {
        Text,
        Hex
    };

    explicit ViewerWidget(QWidget *parent = nullptr);
    ~ViewerWidget() override;

    void openFile(const QString& filePath);
    void clear();

    [[nodiscard]] QString currentFile() const { return m_currentFile; }
    [[nodiscard]] ViewMode viewMode() const { return m_viewMode; }

    void setViewMode(ViewMode mode);

    // Threshold for switching between KTextEditor and wid::TextViewer (bytes)
    static constexpr qint64 SmallFileThreshold = 70 * 1024;  // 70 KB

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void showTextView();
    void showHexView();
    void createTextViewer(uchar* data, qint64 size);
    void createKTextEditorView(const QString& filePath);
    void createHexViewer(uchar* data, qint64 size);
    void clearViewer();

    std::unique_ptr<QFile> m_file;
    wid::TextViewer* m_textViewer = nullptr;
    KTextEditor::Document* m_kteDocument = nullptr;
    KTextEditor::View* m_kteView = nullptr;
    HexViewWidget* m_hexViewer = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QString m_currentFile;
    ViewMode m_viewMode = ViewMode::Text;
};

#endif // VIEWERWIDGET_H
