#include "SizeCalculationWidget.h"
#include "SizeFormat.h"

#include <QFileInfo>
#include <QtConcurrent>

SizeCalculationWidget::SizeCalculationWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    // Add stretch to center content vertically
    layout->addStretch();

    m_pathLabel = new QLabel(this);
    m_pathLabel->setAlignment(Qt::AlignCenter);
    m_pathLabel->setWordWrap(true);
    layout->addWidget(m_pathLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // Indeterminate
    m_progressBar->setTextVisible(false);
    layout->addWidget(m_progressBar);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_statsLabel->font();
    font.setPointSize(font.pointSize() + 2);
    m_statsLabel->setFont(font);
    layout->addWidget(m_statsLabel);

    m_hintLabel = new QLabel(tr("Press ESC or Ctrl+Q to cancel"), this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet("color: gray;");
    layout->addWidget(m_hintLabel);

    layout->addStretch();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(100);
    connect(m_updateTimer, &QTimer::timeout, this, &SizeCalculationWidget::updateDisplay);
}

SizeCalculationWidget::~SizeCalculationWidget()
{
    cancel();
}

void SizeCalculationWidget::startCalculation(const QString& path)
{
    // Cancel any previous calculation
    cancel();

    m_path = path;
    m_cancelled.store(false);
    m_running.store(true);

    // Reset stats
    m_totalFiles.store(0);
    m_totalDirs.store(0);
    m_totalBytes.store(0);
    m_bytesOnDisk.store(0);
    m_symlinks.store(0);

    QFileInfo info(path);
    m_pathLabel->setText(tr("Calculating: %1").arg(info.fileName()));
    m_statsLabel->setText(tr("Scanning..."));

    // Yellow progress bar during calculation
    m_progressBar->setStyleSheet("QProgressBar::chunk { background-color: #f0ad4e; }");
    m_progressBar->setRange(0, 0); // Indeterminate

    m_updateTimer->start();

    // Run calculation in background thread
    auto future = QtConcurrent::run([this, path]() {
        quint64 clusterSize = FileOperations::getClusterSize(path);

        FileOperations::AtomicStats stats;
        stats.totalFiles = &m_totalFiles;
        stats.totalDirs = &m_totalDirs;
        stats.totalBytes = &m_totalBytes;
        stats.bytesOnDisk = &m_bytesOnDisk;
        stats.symlinks = &m_symlinks;

        FileOperations::calculateEntrySizeAtomic(path, stats, clusterSize, &m_cancelled);

        m_running.store(false);

        // Signal completion on main thread
        QMetaObject::invokeMethod(this, &SizeCalculationWidget::onCalculationDone, Qt::QueuedConnection);
    });
}

void SizeCalculationWidget::cancel()
{
    m_cancelled.store(true);
    m_updateTimer->stop();
    m_running.store(false);
}

bool SizeCalculationWidget::isCalculating() const
{
    return m_running.load();
}

void SizeCalculationWidget::updateDisplay()
{
    quint64 files = m_totalFiles.load();
    quint64 dirs = m_totalDirs.load();
    quint64 bytes = m_totalBytes.load();
    quint64 onDisk = m_bytesOnDisk.load();
    quint64 symlinks = m_symlinks.load();

    QString sizeStr = QString::fromStdString(
        SizeFormat::formatSize(static_cast<size_t>(bytes), SizeFormat::Binary));
    QString diskStr = QString::fromStdString(
        SizeFormat::formatSize(static_cast<size_t>(onDisk), SizeFormat::Binary));

    QString text = tr("Files: %1  |  Dirs: %2").arg(files).arg(dirs);
    if (symlinks > 0) {
        text += tr("  |  Symlinks: %1").arg(symlinks);
    }
    text += "\n";
    text += tr("Size: %1  |  On disk: %2").arg(sizeStr, diskStr);

    m_statsLabel->setText(text);
}

void SizeCalculationWidget::onCalculationDone()
{
    m_updateTimer->stop();

    if (m_cancelled.load()) {
        emit cancelled();
        return;
    }

    // Final update
    updateDisplay();

    // Blue progress bar when completed
    m_progressBar->setStyleSheet("QProgressBar::chunk { background-color: #5bc0de; }");
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);

    // Emit final stats
    m_finalStats.totalFiles = m_totalFiles.load();
    m_finalStats.totalDirs = m_totalDirs.load();
    m_finalStats.totalBytes = m_totalBytes.load();
    m_finalStats.bytesOnDisk = m_bytesOnDisk.load();
    m_finalStats.symlinks = m_symlinks.load();

    m_pathLabel->setText(tr("Completed: %1").arg(QFileInfo(m_path).fileName()));

    emit calculationFinished(m_finalStats);
}
