#ifndef MULTIRENAMEDIALOG_H
#define MULTIRENAMEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTableView>
#include <QStandardItemModel>
#include <QPushButton>
#include <QStringList>

class MultiRenameDialog : public QDialog {
    Q_OBJECT

public:
    explicit MultiRenameDialog(const QStringList& fileNames,
                               const QStringList& existingNames,
                               QWidget* parent = nullptr);

    // Returns list of pairs: (oldName, newName) for files that will be renamed
    QList<QPair<QString, QString>> getRenameOperations() const;

private slots:
    void updatePreview();
    void onAccept();

private:
    void setupUi();
    QString applyRename(const QString& fileName) const;
    bool checkConflicts();

    QStringList m_originalNames;
    QStringList m_existingNames;  // All files in directory (for conflict check)

    QLineEdit* m_findEdit = nullptr;
    QLineEdit* m_replaceEdit = nullptr;
    QTableView* m_tableView = nullptr;
    QStandardItemModel* m_model = nullptr;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};

#endif // MULTIRENAMEDIALOG_H