#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>

struct StringListEditorOptions {
    QString title           = "Edit List";
    bool    allowFileOpen   = false;
    QString fileFilter;
    QString fileDialogTitle = "Select File";
};

class StringListEditorDialog : public QDialog {
    Q_OBJECT
public:
    using Options = StringListEditorOptions;

    explicit StringListEditorDialog(const QStringList& items,
                                    int selectedIndex,
                                    const Options& options = Options{},
                                    QWidget* parent = nullptr);

    QStringList items() const;
    int selectedIndex() const;

private:
    void setupUi(const Options& options);
    void onSelectionChanged();
    void onEditChanged(const QString& text);
    void onAddNew();
    void onDelete();
    void onBrowse();
    void updateButtons();

    QListWidget* m_list;
    QLineEdit*   m_edit;
    QPushButton* m_addBtn;
    QPushButton* m_deleteBtn;
    QPushButton* m_browseBtn = nullptr;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;

    bool m_syncing = false;
    Options m_options;
};