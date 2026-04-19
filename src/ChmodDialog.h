#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>

class ChmodDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChmodDialog(const QStringList& paths, QWidget* parent = nullptr);

    void applyChanges();

private:
    QStringList m_paths;
    bool m_syncing = false;

#ifndef _WIN32
    // Unix permission checkboxes [owner/group/other][read/write/exec]
    QCheckBox* m_cb[3][3];
    QLineEdit* m_octalEdit;

    void setupUnixUi(QWidget* page);
    void updateOctalFromCheckboxes();
    void updateCheckboxesFromOctal(const QString& text);
    int octalFromCheckboxes() const;
    void loadUnixPermissions();
    void applyUnixChanges();
#else
    QCheckBox* m_readOnly;
    QCheckBox* m_hidden;
    QCheckBox* m_system;
    QCheckBox* m_archive;

    void setupWindowsUi(QWidget* page);
    void loadWindowsAttributes();
    void applyWindowsChanges();
#endif

    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
};