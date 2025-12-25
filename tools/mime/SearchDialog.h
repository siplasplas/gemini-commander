#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QMimeDatabase>

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchDialog(QWidget *parent = nullptr);

    QString searchPath() const;
    QMimeDatabase::MatchMode matchMode() const;

private slots:
    void onBrowse();
    void onStart();

private:
    QLineEdit* m_pathEdit = nullptr;
    QComboBox* m_matchModeCombo = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};

#endif // SEARCHDIALOG_H
