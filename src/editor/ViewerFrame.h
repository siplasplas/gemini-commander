#ifndef VIEWERFRAME_H
#define VIEWERFRAME_H

#include <QDialog>

class ViewerWidget;
class QVBoxLayout;
class QMenuBar;
class QAction;

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

private slots:
    void onTextModeTriggered();
    void onHexModeTriggered();

private:
    void setupMenuBar();
    void updateMenuChecks();
    void saveGeometryToConfig();

    ViewerWidget* m_viewerWidget = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QMenuBar* m_menuBar = nullptr;
    QAction* m_textAction = nullptr;
    QAction* m_hexAction = nullptr;
    bool m_geometryRestored = false;
};

#endif // VIEWERFRAME_H
