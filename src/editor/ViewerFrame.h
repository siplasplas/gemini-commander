#ifndef VIEWERFRAME_H
#define VIEWERFRAME_H

#include <QDialog>
#include <QPointer>

class ViewerWidget;
class QVBoxLayout;
class QMenuBar;
class QAction;

// The viewer window (F3). An ordinary top-level window, not kept above the
// main window: `anchor` (the main window) only places it, its position being
// remembered relative to it.
class ViewerFrame : public QDialog
{
    Q_OBJECT

public:
    explicit ViewerFrame(const QString& filePath, QWidget *anchor = nullptr);
    ~ViewerFrame() override;

    void openFile(const QString& filePath);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onTextModeTriggered();
    void onHexModeTriggered();

private:
    void setupMenuBar();
    void updateMenuChecks();
    void saveGeometryToConfig();
    // The view is recreated for every file and mode: give it the focus, so
    // that Ctrl+F and F3 work without a click first.
    void focusView();

    QPointer<QWidget> m_anchor;
    ViewerWidget* m_viewerWidget = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QMenuBar* m_menuBar = nullptr;
    QAction* m_textAction = nullptr;
    QAction* m_hexAction = nullptr;
    bool m_geometryRestored = false;
};

#endif // VIEWERFRAME_H
