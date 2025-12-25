#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QMimeDatabase>
#include <QLabel>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onSearchFiles();
    void onShowAllMimes();
    void onItemExpanded(QTreeWidgetItem* item);

private:
    void setupUi();
    void setupMenus();

    QTreeWidget* m_treeWidget = nullptr;
    QLabel* m_statusLabel = nullptr;
    QMimeDatabase m_mimeDb;
    QHash<QString, QString> m_defaultAppCache;  // mime -> default app name

    // For file search results
    void addFileToTree(const QString& filePath, const QString& mimeType, const QString& extension);
    QTreeWidgetItem* findOrCreateCategory(const QString& category);
    QTreeWidgetItem* findOrCreateSubType(QTreeWidgetItem* parent, const QString& subType);
    QTreeWidgetItem* findOrCreateExtension(QTreeWidgetItem* parent, const QString& extension);
    QString getDefaultAppForMime(const QString& mimeType);

    // For all mimes view
    void populateAllMimes();
};

#endif // MAINWINDOW_H
