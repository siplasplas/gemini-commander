#ifndef FUNCTIONBAR_H
#define FUNCTIONBAR_H

#include <QWidget>
#include <QVector>

class QPushButton;

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

private:
    void setupUi();
    QPushButton* createButton(const QString& label);

    QVector<QPushButton*> m_buttons;
};

#endif // FUNCTIONBAR_H