#pragma once

#include <QToolButton>
#include <QToolBar>
#include <QWidgetAction>

/**
 * A QToolButton that rotates its text 90° when the parent toolbar is vertical.
 * Similar to CLion's toolbar behavior.
 */
class VerticalToolButton : public QToolButton
{
    Q_OBJECT

public:
    explicit VerticalToolButton(QWidget* parent = nullptr);
    explicit VerticalToolButton(const QString& text, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Qt::Orientation orientation() const;
    void connectToToolbar();
};

/**
 * A QWidgetAction that creates VerticalToolButton widgets.
 * Use this to add actions to toolbars that will have rotated text when vertical.
 */
class VerticalToolButtonAction : public QWidgetAction
{
    Q_OBJECT

public:
    explicit VerticalToolButtonAction(QObject* parent = nullptr);
    explicit VerticalToolButtonAction(const QString& text, QObject* parent = nullptr);

protected:
    QWidget* createWidget(QWidget* parent) override;
};

/**
 * A QLabel that rotates its text 90° when the parent toolbar is vertical.
 * Use this for non-clickable text in toolbars (like storage info).
 */
class VerticalLabel : public QWidget
{
    Q_OBJECT

public:
    explicit VerticalLabel(QWidget* parent = nullptr);
    explicit VerticalLabel(const QString& text, QWidget* parent = nullptr);

    void setText(const QString& text);
    QString text() const { return m_text; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_text;
    Qt::Orientation orientation() const;
    void connectToToolbar();
};