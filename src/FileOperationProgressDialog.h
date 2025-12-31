#ifndef FILEOPERATIONPROGRESSDIALOG_H
#define FILEOPERATIONPROGRESSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

// Progress dialog for file operations (copy/move)
// - Shows "1/1000 filename, len=2400" style progress
// - Always on top, blocks nested commands
// - Allows cancel, processes events for repaint
class FileOperationProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileOperationProgressDialog(const QString& title, int totalFiles, QWidget* parent = nullptr);
    ~FileOperationProgressDialog() override;

    // Update progress for copy operations (shows filename and size)
    void updateProgress(int currentFile, const QString& fileName, qint64 fileSize);

    // Update progress for fast move operations (shows only every Nth file, no filename)
    void updateMoveProgress(int currentFile, int showEveryN = 100);

    // Check if user canceled
    bool wasCanceled() const { return m_canceled; }

    // Force event processing
    void processEvents();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupUi(const QString& title);

    QLabel* m_progressLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_cancelButton = nullptr;

    int m_totalFiles = 0;
    bool m_canceled = false;
    bool m_operationWasInProgress = false;
};

#endif // FILEOPERATIONPROGRESSDIALOG_H