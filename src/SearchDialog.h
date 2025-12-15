#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QLabel>
#include <QThread>

class SearchWorker;

class SearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit SearchDialog(const QString& startPath, QWidget* parent = nullptr);
    ~SearchDialog();

private slots:
    void onStartSearch();
    void onStopSearch();
    void onResultFound(const QString& path, qint64 size, const QDateTime& modified);
    void onSearchFinished();
    void onProgressUpdate(int searchedFiles, int foundFiles);
    void onResultDoubleClicked(int row, int column);

private:
    void setupUi();
    void createStandardTab();
    void createAdvancedTab();
    void createResultsTab();

    // UI elements
    QTabWidget* m_tabWidget;

    // Standard tab
    QWidget* m_standardTab;
    QLineEdit* m_searchInEdit;
    QPushButton* m_browseButton;
    QLineEdit* m_fileNameEdit;
    QCheckBox* m_fileNameCaseSensitiveCheck;
    QCheckBox* m_partOfNameCheck;
    QLineEdit* m_containingTextEdit;
    QCheckBox* m_textCaseSensitiveCheck;
    QCheckBox* m_wholeWordsCheck;

    // Advanced tab
    QWidget* m_advancedTab;
    QLineEdit* m_minSizeEdit;
    QLineEdit* m_maxSizeEdit;

    // Results tab
    QWidget* m_resultsTab;
    QTableWidget* m_resultsTable;
    QLabel* m_statusLabel;

    // Control buttons
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_closeButton;

    // Search worker
    QThread* m_searchThread;
    SearchWorker* m_searchWorker;

    QString m_startPath;
    int m_foundCount;
};