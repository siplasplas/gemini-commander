#include "CopyDialog.h"

#include "FileOperations.h"
#include "SearchIgnore.h"
#include "SizeFormat.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

#include <optional>

CopyDialog::CopyDialog(const QString& title, const QString& label, const QString& destination,
                       const QString& srcDir, const QStringList& names,
                       bool offerGitignore, bool offerCount, QWidget* parent)
    : QDialog(parent)
    , m_srcDir(srcDir)
    , m_names(names)
{
    setWindowTitle(title);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(label, this));
    m_destEdit = new QLineEdit(destination, this);
    m_destEdit->selectAll();
    layout->addWidget(m_destEdit);

    if (offerGitignore) {
        m_gitignoreCheck = new QCheckBox(tr("Skip what .gitignore files ignore"), this);
        m_gitignoreCheck->setToolTip(tr("Reads .gitignore from the source directory down; deeper "
                                        "files add rules, ! includes again. The .git directory is "
                                        "skipped too."));
        layout->addWidget(m_gitignoreCheck);
        // A shown count no longer matches: count again.
        connect(m_gitignoreCheck, &QCheckBox::toggled, this, [this] {
            if (m_countButton && !m_countLabel->text().isEmpty())
                startCounting();
        });
    }

    if (offerCount) {
        auto* countRow = new QHBoxLayout;
        m_countButton = new QPushButton(tr("Count"), this);
        m_countButton->setAutoDefault(false);  // Enter still means OK
        m_countButton->setToolTip(tr("Count the files to copy and their total size"));
        m_countLabel = new QLabel(this);
        m_countLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        countRow->addWidget(m_countButton);
        countRow->addWidget(m_countLabel, 1);
        layout->addLayout(countRow);
        connect(m_countButton, &QPushButton::clicked, this, [this] {
            if (!m_countThread) {
                startCounting();
                return;
            }
            stopCounting();
            // Stopped while OK waits for it: copy now; the copy counts by itself.
            if (m_acceptWhenCounted)
                QDialog::accept();
        });
        m_countTimer.setInterval(100);
        connect(&m_countTimer, &QTimer::timeout, this, [this] { showCount(false); });
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &CopyDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(500, sizeHint().height());
}

CopyDialog::~CopyDialog()
{
    stopCounting();
}

QString CopyDialog::destination() const
{
    return m_destEdit->text();
}

bool CopyDialog::useGitignore() const
{
    return m_gitignoreCheck && m_gitignoreCheck->isChecked();
}

void CopyDialog::setUseGitignore(bool use)
{
    if (m_gitignoreCheck)
        m_gitignoreCheck->setChecked(use);
}

std::optional<FileOperations::CopyTotals> CopyDialog::totals() const
{
    if (!m_countComplete || m_countedWithGitignore != useGitignore())
        return std::nullopt;
    return FileOperations::CopyTotals{m_count->files.load(), m_count->bytes.load()};
}

void CopyDialog::accept()
{
    if (m_countThread) {
        // Copy with the count being made: close when it is done.
        m_acceptWhenCounted = true;
        m_okButton->setEnabled(false);
        m_countLabel->setToolTip(tr("Copying starts when counting is done"));
        return;
    }
    QDialog::accept();
}

void CopyDialog::startCounting()
{
    stopCounting();
    auto state = std::make_shared<CountState>();
    m_count = state;
    m_countComplete = false;
    const bool gitignore = useGitignore();
    m_countedWithGitignore = gitignore;
    m_countThread = QThread::create([state, srcDir = m_srcDir, names = m_names, gitignore] {
        // The same skipping as the copy itself (executeCopyOrMove).
        std::optional<SearchIgnore> ignore;
        if (gitignore)
            ignore.emplace(srcDir, QStringList(), QStringList(), true);
        quint64 files = 0, bytes = 0;
        FileOperations::countCopyWork(srcDir, names, files, bytes,
            [&state](quint64 f, quint64 b) {
                state->files = f;
                state->bytes = b;
                return !state->cancel;
            }, ignore ? &*ignore : nullptr);
    });
    QThread* thread = m_countThread;
    connect(thread, &QThread::finished, this, [this, thread] {
        if (thread != m_countThread)
            return;  // stopped (and waited for) already
        m_countThread->deleteLater();
        m_countThread = nullptr;
        m_countComplete = true;
        m_countTimer.stop();
        m_countButton->setText(tr("Count"));
        showCount(true);
        if (m_acceptWhenCounted)
            QDialog::accept();
    });
    m_countButton->setText(tr("Stop"));
    showCount(false);
    m_countTimer.start();
    thread->start();
}

void CopyDialog::stopCounting()
{
    if (!m_countThread)
        return;
    QThread* thread = m_countThread;
    m_countThread = nullptr;
    const bool interrupted = !thread->isFinished();
    m_count->cancel = true;
    thread->wait();
    // Not deleted at once: its queued finished() may still be pending.
    thread->deleteLater();
    m_countComplete = !interrupted;
    m_countTimer.stop();
    m_countButton->setText(tr("Count"));
    showCount(true);
    if (interrupted)
        m_countLabel->setText(m_countLabel->text() + tr(" (stopped)"));
}

void CopyDialog::showCount(bool finished)
{
    if (!m_count)
        return;
    const auto files = static_cast<size_t>(m_count->files.load());
    const auto bytes = static_cast<size_t>(m_count->bytes.load());
    QString text = tr("Files: %1, size: %2 B (%3)")
        .arg(QString::fromStdString(SizeFormat::formatWithSeparators(files)),
             QString::fromStdString(SizeFormat::formatWithSeparators(bytes)),
             QString::fromStdString(SizeFormat::formatSize(bytes, SizeFormat::Binary)));
    if (!finished)
        text = tr("Counting... %1").arg(text);
    m_countLabel->setText(text);
}
