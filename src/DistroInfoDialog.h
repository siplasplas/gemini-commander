#ifndef DISTROINFODIALOG_H
#define DISTROINFODIALOG_H

#include <QDialog>

class QTextEdit;
class QPushButton;

class DistroInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DistroInfoDialog(QWidget* parent = nullptr);

private slots:
    void copyAll();

private:
    QTextEdit* m_textEdit;
    QPushButton* m_copyButton;
    QPushButton* m_closeButton;
};

#endif // DISTROINFODIALOG_H