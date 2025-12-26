#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QMimeDatabase>
#include <QLabel>
#include <QLineEdit>
#include "Archives.h"
#include "FileIconResolver.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSearchFiles();
    void onShowAllMimes();
    void onItemExpanded(QTreeWidgetItem* item);
    void onFilterChanged(const QString& text);

private:
    void setupUi();
    void setupMenus();

    QLineEdit* m_filterEdit = nullptr;
    QTreeWidget* m_treeWidget = nullptr;
    QLabel* m_statusLabel = nullptr;
    QMimeDatabase m_mimeDb;
    QHash<QString, QString> m_defaultAppCache;  // mime -> default app name
    QString m_lastSearchPath;  // Last used search path
    bool m_searchCancelled = false;  // Flag for cancelling search

    // For file search results
    void addFileToTree(const QString& filePath, const QString& mimeType, const QString& extension,
                       const QString& components, const QString& archiveType);
    QTreeWidgetItem* findOrCreateCategory(const QString& category);
    QTreeWidgetItem* findOrCreateSubType(QTreeWidgetItem* parent, const QString& subType);
    QTreeWidgetItem* findOrCreateExtension(QTreeWidgetItem* parent, const QString& extension);
    QString getDefaultAppForMime(const QString& mimeType);

    // For all mimes view
    void populateAllMimes();
};

#endif // MAINWINDOW_H
