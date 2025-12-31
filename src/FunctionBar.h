#pragma once

#include <QWidget>
#include <QVector>
#include <QPushButton>

class QBoxLayout;
class QToolBar;

/**
 * Custom button that can rotate its text when in vertical orientation.
 */
class FunctionButton : public QPushButton
{
    Q_OBJECT

public:
    explicit FunctionButton(const QString& text, QWidget* parent = nullptr);

    void setVertical(bool vertical);
    bool isVertical() const { return m_vertical; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_vertical = false;
};

/**
 * Function bar with F3-F8 buttons that can be docked horizontally or vertically.
 */
class FunctionBar : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionBar(QWidget *parent = nullptr);

signals:
    void viewClicked();
    void editClicked();
    void copyClicked();
    void moveClicked();
    void mkdirClicked();
    void deleteClicked();
    void terminalClicked();
    void exitClicked();

public slots:
    void setOrientation(Qt::Orientation orientation);

private:
    void setupUi();
    FunctionButton* createButton(const QString& label);
    void updateLayout();

    QVector<FunctionButton*> m_buttons;
    QBoxLayout* m_layout = nullptr;
    Qt::Orientation m_orientation = Qt::Horizontal;
};
