#ifndef HEXVIEWWIDGET_H
#define HEXVIEWWIDGET_H

#include <QAbstractScrollArea>

// Hex viewer that works with memory-mapped data (read-only)
class HexViewWidget : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit HexViewWidget(QWidget *parent = nullptr);
    ~HexViewWidget() override;

    // Set memory-mapped data (does not copy)
    void setData(const char* data, qint64 size);
    void clear();

    void setBytesPerLine(int bytes);
    int bytesPerLine() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateScrollbars();
    qint64 positionAtPoint(const QPoint &point);

    const char* m_data = nullptr;
    qint64 m_size = 0;
    int m_bytesPerLine = 16;
    qint64 m_cursorPosition = 0;
    int m_charWidth = 0;
    int m_charHeight = 0;

    enum FocusArea {
        HexArea,
        AsciiArea
    };
    FocusArea m_focusArea = HexArea;
};

#endif // HEXVIEWWIDGET_H
