#include "FileOperationProgressDialog.h"
#include "keys/KeyRouter.h"

#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QVBoxLayout>

FileOperationProgressDialog::FileOperationProgressDialog(const QString& title, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::WindowTitleHint)
{
    setupUi(title);
    m_repaintTimer.start();

    // Block nested commands
    m_operationWasInProgress = KeyRouter::instance().isOperationInProgress();
    KeyRouter::instance().setOperationInProgress(true);
}

FileOperationProgressDialog::~FileOperationProgressDialog()
{
    // Restore operation state
    KeyRouter::instance().setOperationInProgress(m_operationWasInProgress);
}

void FileOperationProgressDialog::setupUi(const QString& title)
{
    setWindowTitle(title);
    setMinimumWidth(450);
    setModal(true);

    auto* layout = new QVBoxLayout(this);

    m_fileLabel = new QLabel(tr("Counting..."), this);
    m_fileLabel->setMinimumWidth(400);
    layout->addWidget(m_fileLabel);

    m_fileBar = new QProgressBar(this);
    m_fileBar->setRange(0, 1000);
    m_fileBar->setValue(0);
    layout->addWidget(m_fileBar);

    m_overallLabel = new QLabel(this);
    layout->addWidget(m_overallLabel);

    m_overallBar = new QProgressBar(this);
    m_overallBar->setRange(0, 1000);
    m_overallBar->setValue(0);
    layout->addWidget(m_overallBar);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        m_canceled = true;
    });
    layout->addWidget(m_cancelButton, 0, Qt::AlignCenter);

    show();
    processEvents();
}

void FileOperationProgressDialog::setBarFraction(QProgressBar* bar, double fraction)
{
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    bar->setValue(static_cast<int>(fraction * 1000.0 + 0.5));
}

QString FileOperationProgressDialog::formatBytes(qint64 bytes)
{
    if (bytes >= 1024LL * 1024 * 1024)
        return QString::number(static_cast<double>(bytes) / (1024.0 * 1024 * 1024), 'f', 2) + " GB";
    if (bytes >= 1024 * 1024)
        return QString::number(static_cast<double>(bytes) / (1024.0 * 1024), 'f', 1) + " MB";
    if (bytes >= 1024)
        return QString::number(bytes / 1024) + " KB";
    return QString::number(bytes) + " B";
}

void FileOperationProgressDialog::updateCounting(quint64 files, quint64 bytes)
{
    if (m_canceled)
        return;
    m_fileLabel->setText(tr("Counting... %1 files, %2")
                             .arg(files)
                             .arg(formatBytes(static_cast<qint64>(bytes))));
    refreshBars(true);
}

void FileOperationProgressDialog::setTotals(quint64 totalFiles, quint64 totalBytes)
{
    m_totalFiles = totalFiles;
    m_totalBytes = totalBytes;
    m_bytesDone = 0;
    m_fileIndex = 0;
    m_curFileBytes = 0;
    m_curFileSize = 0;
    refreshBars(true);
}

void FileOperationProgressDialog::beginFile(const QString& fileName, qint64 fileSize)
{
    m_curFileName = fileName;
    m_curFileSize = fileSize;
    m_curFileBytes = 0;
    ++m_fileIndex;
    refreshBars(false);
}

void FileOperationProgressDialog::addFileBytes(qint64 bytesSoFar)
{
    m_curFileBytes = bytesSoFar;
    refreshBars(false);
}

void FileOperationProgressDialog::endFile()
{
    m_bytesDone += static_cast<quint64>(m_curFileSize);
    m_curFileBytes = 0;
    m_curFileSize = 0;
    refreshBars(false);
}

void FileOperationProgressDialog::refreshBars(bool force)
{
    // Top bar: progress within the current file.
    double fileFraction = (m_curFileSize > 0)
                              ? static_cast<double>(m_curFileBytes) / static_cast<double>(m_curFileSize)
                              : 1.0;
    setBarFraction(m_fileBar, fileFraction);

    // Bottom bar: overall progress across all bytes.
    double overallFraction = 0.0;
    quint64 doneNow = m_bytesDone + static_cast<quint64>(m_curFileBytes);
    if (m_totalBytes > 0)
        overallFraction = static_cast<double>(doneNow) / static_cast<double>(m_totalBytes);
    else if (m_totalFiles > 0)
        overallFraction = static_cast<double>(m_fileIndex) / static_cast<double>(m_totalFiles);
    setBarFraction(m_overallBar, overallFraction);

    if (!m_curFileName.isEmpty()) {
        m_fileLabel->setText(QString("%1/%2  %3  (%4)")
                                 .arg(m_fileIndex)
                                 .arg(m_totalFiles)
                                 .arg(m_curFileName)
                                 .arg(formatBytes(m_curFileSize)));
    }
    m_overallLabel->setText(tr("Total: %1 / %2")
                                .arg(formatBytes(static_cast<qint64>(doneNow)))
                                .arg(formatBytes(static_cast<qint64>(m_totalBytes))));

    // Throttle event processing to keep the UI responsive without slowing the copy.
    if (force || m_repaintTimer.elapsed() >= 30) {
        m_repaintTimer.restart();
        processEvents();
    }
}

void FileOperationProgressDialog::processEvents()
{
    QApplication::processEvents(QEventLoop::AllEvents, 50);
}

void FileOperationProgressDialog::closeEvent(QCloseEvent* event)
{
    // Treat close as cancel
    m_canceled = true;
    event->accept();
}

void FileOperationProgressDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        m_canceled = true;
        event->accept();
    } else {
        QDialog::keyPressEvent(event);
    }
}
