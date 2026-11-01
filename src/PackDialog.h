#ifndef PACKDIALOG_H
#define PACKDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>

class PackDialog : public QDialog {
    Q_OBJECT

public:
    explicit PackDialog(const QString& defaultName,
                        const QString& defaultDest,
                        int markedCount,
                        QWidget* parent = nullptr);

    QString archiveName() const;
    QString destination() const;
    // "zip", "7z", "tar", "tar.gz", "tar.xz", "tar.zst" or "tar.bz2" (the
    // tar formats with the archive module only)
    QString packerType() const;
    bool moveFiles() const;
    QString volumeSize() const;   // empty or "100k", "50m", etc.
    QString solidBlockSize() const; // 7z: empty or "1g", etc.
    int compressionLevel() const;   // -1: the format's default
    QString password() const;       // zip and 7z; empty: none

private slots:
    void onPackerChanged(int index);
    void onBrowseDestination();
    void updateArchiveExtension();

private:
    void setupUi();
    QString currentExtension() const;
    static QString sizeWithUnit(const QLineEdit* edit, const QComboBox* unit);

    QString m_baseName;  // name without extension

    QLineEdit* m_archiveNameEdit = nullptr;
    QLineEdit* m_destinationEdit = nullptr;
    QComboBox* m_packerCombo = nullptr;
    QCheckBox* m_moveCheck = nullptr;

    QSpinBox* m_levelSpin = nullptr;         // -1 shown as "Default"
    QLineEdit* m_passwordEdit = nullptr;
    QLineEdit* m_volumeSizeEdit = nullptr;
    QComboBox* m_volumeUnitCombo = nullptr;  // B, KB, MB, GB

    // 7z-specific widgets
    QWidget* m_7zOptionsWidget = nullptr;
    QLineEdit* m_solidBlockEdit = nullptr;
    QComboBox* m_solidBlockUnitCombo = nullptr;

    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};

#endif // PACKDIALOG_H
