#pragma once

#include "FileOperations.h"

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QTimer>
#include <QFile>
#include <atomic>
#include <memory>

// Widget that displays live size calculation progress for a directory
// Runs calculation in background thread with live updates every 100ms
class SizeCalculationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SizeCalculationWidget(QWidget* parent = nullptr);
    ~SizeCalculationWidget() override;

    void startCalculation(const QString& path);
    void cancel();
    bool isCalculating() const;

signals:
    void calculationFinished(const FileOperations::CopyStats& stats);
    void cancelled();

private slots:
    void updateDisplay();
    void onCalculationDone();

private:
    QLabel* m_pathLabel = nullptr;
    QLabel* m_statsLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QTimer* m_updateTimer = nullptr;

    QString m_path;
    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_running{false};

    // Thread-safe stats updated by worker thread
    std::atomic<quint64> m_totalFiles{0};
    std::atomic<quint64> m_totalDirs{0};
    std::atomic<quint64> m_totalBytes{0};
    std::atomic<quint64> m_bytesOnDisk{0};
    std::atomic<quint64> m_symlinks{0};

    FileOperations::CopyStats m_finalStats;
};
