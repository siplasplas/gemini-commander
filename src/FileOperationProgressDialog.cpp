#include "FileOperationProgressDialog.h"
#include "keys/KeyRouter.h"

#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QVBoxLayout>

FileOperationProgressDialog::FileOperationProgressDialog(const QString& title, int totalFiles, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::WindowTitleHint)
    , m_totalFiles(totalFiles)
{
    setupUi(title);

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

    m_progressLabel = new QLabel(this);
    m_progressLabel->setMinimumWidth(400);
    layout->addWidget(m_progressLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, m_totalFiles);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        m_canceled = true;
    });
    layout->addWidget(m_cancelButton, 0, Qt::AlignCenter);

    show();
    processEvents();
}

void FileOperationProgressDialog::updateProgress(int currentFile, const QString& fileName, qint64 fileSize)
{
    if (m_canceled)
        return;

    m_progressBar->setValue(currentFile);

    QString sizeStr;
    if (fileSize >= 1024 * 1024) {
        sizeStr = QString::number(fileSize / (1024 * 1024)) + " MB";
    } else if (fileSize >= 1024) {
        sizeStr = QString::number(fileSize / 1024) + " KB";
    } else {
        sizeStr = QString::number(fileSize) + " B";
    }

    m_progressLabel->setText(QString("%1/%2 %3, %4")
        .arg(currentFile)
        .arg(m_totalFiles)
        .arg(fileName)
        .arg(sizeStr));

    processEvents();
}

void FileOperationProgressDialog::updateMoveProgress(int currentFile, int showEveryN)
{
    if (m_canceled)
        return;

    m_progressBar->setValue(currentFile);

    // Only update label every Nth file
    if (currentFile % showEveryN == 0 || currentFile == m_totalFiles) {
        m_progressLabel->setText(QString("%1/%2")
            .arg(currentFile)
            .arg(m_totalFiles));
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