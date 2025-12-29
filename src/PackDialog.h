#ifndef PACKDIALOG_H
#define PACKDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>

class PackDialog : public QDialog {
    Q_OBJECT

public:
    explicit PackDialog(const QString& defaultName,
                        const QString& defaultDest,
                        int markedCount,
                        QWidget* parent = nullptr);

    QString archiveName() const;
    QString destination() const;
    QString packerType() const;  // "zip" or "7z"
    bool moveFiles() const;
    QString volumeSize() const;   // empty or "100k", "50m", etc.
    QString solidBlockSize() const; // empty or "1g", etc.

private slots:
    void onPackerChanged(int index);
    void onBrowseDestination();
    void updateArchiveExtension();

private:
    void setupUi();
    QString currentExtension() const;

    QString m_baseName;  // name without extension

    QLineEdit* m_archiveNameEdit = nullptr;
    QLineEdit* m_destinationEdit = nullptr;
    QComboBox* m_packerCombo = nullptr;
    QCheckBox* m_moveCheck = nullptr;

    // 7z-specific widgets
    QWidget* m_7zOptionsWidget = nullptr;
    QLineEdit* m_volumeSizeEdit = nullptr;
    QComboBox* m_volumeUnitCombo = nullptr;  // B, KB, MB, GB
    QLineEdit* m_solidBlockEdit = nullptr;
    QComboBox* m_solidBlockUnitCombo = nullptr;

    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};

#endif // PACKDIALOG_H
