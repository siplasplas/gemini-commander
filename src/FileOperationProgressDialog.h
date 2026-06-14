#ifndef FILEOPERATIONPROGRESSDIALOG_H
#define FILEOPERATIONPROGRESSDIALOG_H

#include <QDialog>
#include <QElapsedTimer>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

// Progress dialog for file operations (copy/move).
//
// Shows two progress bars:
//   - top    : progress within the current file (proportional to its bytes;
//              updated chunk-by-chunk for large files copied in chunked mode)
//   - bottom : overall progress proportional to the total bytes of all files
//
// Always on top, blocks nested commands, allows cancel, processes events for
// repaint. The byte totals come from an up-front counting pass.
class FileOperationProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileOperationProgressDialog(const QString& title, QWidget* parent = nullptr);
    ~FileOperationProgressDialog() override;

    // Counting phase (before totals are known): show how much has been scanned.
    void updateCounting(quint64 files, quint64 bytes);

    // Set the totals discovered by the counting pass.
    void setTotals(quint64 totalFiles, quint64 totalBytes);

    // Per-file lifecycle:
    //  beginFile()  - start a new file (resets the top bar, updates the label)
    //  addFileBytes() - report bytes transferred so far within the current file
    //  endFile()    - the current file is done/skipped; commit its bytes to overall
    void beginFile(const QString& fileName, qint64 fileSize);
    void addFileBytes(qint64 bytesSoFar);
    void endFile();

    // Report bytes actually written to disk. Drives speed/ETA so that skipped
    // files (which advance the progress bar but transfer nothing) do not inflate
    // the measured speed.
    void addTransferred(qint64 deltaBytes) { m_transferredBytes += static_cast<quint64>(deltaBytes); }

    // Check if user canceled
    bool wasCanceled() const { return m_canceled; }

    // Pointer to the cancel flag, for cancellable counting helpers.
    bool* cancelPointer() { return &m_canceled; }

    // Pause/resume the transfer clock so time spent waiting on an overwrite
    // prompt is excluded from the speed and ETA computation.
    void pauseClock();
    void resumeClock();

    // Force event processing
    void processEvents();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupUi(const QString& title);
    void refreshBars(bool force = false);
    // Ask the user to confirm interrupting the operation; sets m_canceled on Yes.
    void requestCancel();
    qint64 transferElapsedMs() const;
    static void setBarFraction(QProgressBar* bar, double fraction);
    static QString formatBytes(qint64 bytes);
    static QString formatDuration(qint64 seconds);

    QLabel* m_fileLabel = nullptr;
    QLabel* m_overallLabel = nullptr;
    QLabel* m_statsLabel = nullptr;
    QProgressBar* m_fileBar = nullptr;
    QProgressBar* m_overallBar = nullptr;
    QPushButton* m_cancelButton = nullptr;

    quint64 m_totalFiles = 0;
    quint64 m_totalBytes = 0;
    quint64 m_fileIndex = 0;      // 1-based index of the current file
    qint64 m_curFileSize = 0;     // size of the current file
    qint64 m_curFileBytes = 0;    // bytes transferred within the current file
    quint64 m_bytesDone = 0;      // bytes of all fully-finished files (incl. skipped)
    quint64 m_transferredBytes = 0;  // bytes actually written (excludes skipped); for speed
    QString m_curFileName;

    QElapsedTimer m_repaintTimer;

    // Transfer clock (excludes time paused for overwrite prompts).
    QElapsedTimer m_clock;
    bool m_clockRunning = false;
    qint64 m_pausedMs = 0;
    qint64 m_pauseStartedAt = -1;  // m_clock.elapsed() when a pause began; -1 = not paused

    bool m_canceled = false;
    bool m_askingCancel = false;  // guard against re-entrant confirmation prompts
    bool m_operationWasInProgress = false;
};

#endif // FILEOPERATIONPROGRESSDIALOG_H
