#include "ChmodDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QFileInfo>

#ifndef _WIN32
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#else
#include <windows.h>
#endif

ChmodDialog::ChmodDialog(const QStringList& paths, QWidget* parent)
    : QDialog(parent), m_paths(paths)
{
    setWindowTitle(tr("Change Attributes"));
    setMinimumWidth(360);

    auto* mainLayout = new QVBoxLayout(this);

    QString desc = paths.size() == 1
        ? tr("Changing attributes for: %1").arg(QFileInfo(paths[0]).fileName())
        : tr("Changing attributes for %n item(s)", "", paths.size());
    auto* descLabel = new QLabel(desc, this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    auto* page = new QWidget(this);
    mainLayout->addWidget(page);

#ifndef _WIN32
    setupUnixUi(page);
    loadUnixPermissions();
#else
    setupWindowsUi(page);
    loadWindowsAttributes();
#endif

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    btnRow->addWidget(m_okButton);
    btnRow->addWidget(m_cancelButton);
    mainLayout->addLayout(btnRow);

    connect(m_okButton,     &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

// ─────────────────────────────────────────────────────────────── Unix ────────

#ifndef _WIN32

void ChmodDialog::setupUnixUi(QWidget* page)
{
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* group = new QGroupBox(tr("Unix Permissions"), page);
    auto* grid  = new QGridLayout(group);
    grid->setSpacing(6);

    // Header row
    grid->addWidget(new QLabel(tr("Read"),    group), 0, 1, Qt::AlignCenter);
    grid->addWidget(new QLabel(tr("Write"),   group), 0, 2, Qt::AlignCenter);
    grid->addWidget(new QLabel(tr("Execute"), group), 0, 3, Qt::AlignCenter);

    const char* rowLabels[3] = {"Owner", "Group", "Other"};
    for (int row = 0; row < 3; ++row) {
        grid->addWidget(new QLabel(tr(rowLabels[row]), group), row + 1, 0);
        for (int col = 0; col < 3; ++col) {
            auto* cb = new QCheckBox(group);
            cb->setTristate(true);
            grid->addItem(new QSpacerItem(0, 0), row + 1, col + 1);
            grid->addWidget(cb, row + 1, col + 1, Qt::AlignCenter);
            m_cb[row][col] = cb;
            connect(cb, &QCheckBox::checkStateChanged, this, [this]() {
                if (!m_syncing) updateOctalFromCheckboxes();
            });
        }
    }

    layout->addWidget(group);

    auto* octalForm = new QFormLayout();
    m_octalEdit = new QLineEdit(page);
    m_octalEdit->setMaximumWidth(70);
    m_octalEdit->setPlaceholderText("664");
    octalForm->addRow(tr("Octal value:"), m_octalEdit);
    layout->addLayout(octalForm);

    connect(m_octalEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (!m_syncing) updateCheckboxesFromOctal(text);
    });
}

void ChmodDialog::loadUnixPermissions()
{
    // Compute AND (bits present in all files) and OR (bits present in any file).
    // Use 0777 mask; initialize AND to all-ones, OR to all-zeros.
    unsigned andBits = 0777u;
    unsigned orBits  = 0u;
    bool anyLoaded = false;

    for (const QString& path : m_paths) {
        struct stat st{};
        if (stat(path.toLocal8Bit().constData(), &st) == 0) {
            unsigned bits = static_cast<unsigned>(st.st_mode) & 0777u;
            andBits &= bits;
            orBits  |= bits;
            anyLoaded = true;
        }
    }

    if (!anyLoaded)
        return;

    // Bit mapping: [row][col] → bit position
    // row 0=owner, 1=group, 2=other; col 0=read, 1=write, 2=exec
    // owner: bits 8,7,6 → shifts 8,7,6
    // group: bits 5,4,3 → shifts 5,4,3
    // other: bits 2,1,0 → shifts 2,1,0
    const int shifts[3][3] = {
        {8, 7, 6},
        {5, 4, 3},
        {2, 1, 0}
    };

    m_syncing = true;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            unsigned bit = 1u << shifts[r][c];
            bool inAnd = (andBits & bit) != 0;
            bool inOr  = (orBits  & bit) != 0;
            if (inAnd)
                m_cb[r][c]->setCheckState(Qt::Checked);
            else if (inOr)
                m_cb[r][c]->setCheckState(Qt::PartiallyChecked);
            else
                m_cb[r][c]->setCheckState(Qt::Unchecked);
        }
    }
    m_syncing = false;

    // Show octal only when all files agree (no mixed bits)
    if (andBits == orBits)
        m_octalEdit->setText(QString::number(andBits, 8));
    else
        m_octalEdit->clear();
}

int ChmodDialog::octalFromCheckboxes() const
{
    const int shifts[3][3] = {
        {8, 7, 6},
        {5, 4, 3},
        {2, 1, 0}
    };
    int mode = 0;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (m_cb[r][c]->checkState() == Qt::Checked)
                mode |= (1 << shifts[r][c]);
    return mode;
}

void ChmodDialog::updateOctalFromCheckboxes()
{
    // Only show octal if no partial checkboxes remain
    bool anyPartial = false;
    for (int r = 0; r < 3 && !anyPartial; ++r)
        for (int c = 0; c < 3 && !anyPartial; ++c)
            if (m_cb[r][c]->checkState() == Qt::PartiallyChecked)
                anyPartial = true;

    m_syncing = true;
    if (anyPartial)
        m_octalEdit->clear();
    else
        m_octalEdit->setText(QString::number(octalFromCheckboxes(), 8));
    m_syncing = false;
}

void ChmodDialog::updateCheckboxesFromOctal(const QString& text)
{
    if (text.length() < 3 || text.length() > 4)
        return;
    bool ok = false;
    int mode = text.toInt(&ok, 8);
    if (!ok || mode < 0 || mode > 0777)
        return;

    const int shifts[3][3] = {
        {8, 7, 6},
        {5, 4, 3},
        {2, 1, 0}
    };
    m_syncing = true;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            m_cb[r][c]->setCheckState((mode >> shifts[r][c]) & 1 ? Qt::Checked : Qt::Unchecked);
    m_syncing = false;
}

void ChmodDialog::applyUnixChanges()
{
    const int shifts[3][3] = {
        {8, 7, 6},
        {5, 4, 3},
        {2, 1, 0}
    };

    // Build set/clear masks from non-partial checkboxes
    unsigned setMask   = 0;
    unsigned clearMask = 0;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            unsigned bit = 1u << shifts[r][c];
            Qt::CheckState state = m_cb[r][c]->checkState();
            if (state == Qt::Checked)
                setMask |= bit;
            else if (state == Qt::Unchecked)
                clearMask |= bit;
            // PartiallyChecked → leave that bit unchanged
        }
    }

    QStringList errors;
    for (const QString& path : m_paths) {
        struct stat st{};
        if (stat(path.toLocal8Bit().constData(), &st) != 0) {
            errors << path + ": " + QString::fromLocal8Bit(strerror(errno));
            continue;
        }
        unsigned newMode = ((static_cast<unsigned>(st.st_mode) & ~clearMask) | setMask) & 0777u;
        if (chmod(path.toLocal8Bit().constData(), static_cast<mode_t>(newMode)) != 0)
            errors << path + ": " + QString::fromLocal8Bit(strerror(errno));
    }

    if (!errors.isEmpty()) {
        QMessageBox::warning(parentWidget(), tr("Change Attributes"),
            tr("Could not change attributes for some items:\n%1").arg(errors.join('\n')));
    }
}

#else // ───────────────────────────────────────────────────────── Windows ────

void ChmodDialog::setupWindowsUi(QWidget* page)
{
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* group = new QGroupBox(tr("File Attributes"), page);
    auto* gLayout = new QVBoxLayout(group);

    m_readOnly = new QCheckBox(tr("Read-only"), group);
    m_hidden   = new QCheckBox(tr("Hidden"),    group);
    m_system   = new QCheckBox(tr("System"),    group);
    m_archive  = new QCheckBox(tr("Archive"),   group);

    m_readOnly->setTristate(true);
    m_hidden->setTristate(true);
    m_system->setTristate(true);
    m_archive->setTristate(true);

    gLayout->addWidget(m_readOnly);
    gLayout->addWidget(m_hidden);
    gLayout->addWidget(m_system);
    gLayout->addWidget(m_archive);

    layout->addWidget(group);
}

void ChmodDialog::loadWindowsAttributes()
{
    DWORD andBits = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
                    FILE_ATTRIBUTE_SYSTEM   | FILE_ATTRIBUTE_ARCHIVE;
    DWORD orBits  = 0;
    bool anyLoaded = false;

    for (const QString& path : m_paths) {
        DWORD attrs = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            andBits &= attrs;
            orBits  |= attrs;
            anyLoaded = true;
        }
    }

    if (!anyLoaded)
        return;

    auto setState = [&](QCheckBox* cb, DWORD flag) {
        bool inAnd = (andBits & flag) != 0;
        bool inOr  = (orBits  & flag) != 0;
        if (inAnd)        cb->setCheckState(Qt::Checked);
        else if (inOr)    cb->setCheckState(Qt::PartiallyChecked);
        else              cb->setCheckState(Qt::Unchecked);
    };

    setState(m_readOnly, FILE_ATTRIBUTE_READONLY);
    setState(m_hidden,   FILE_ATTRIBUTE_HIDDEN);
    setState(m_system,   FILE_ATTRIBUTE_SYSTEM);
    setState(m_archive,  FILE_ATTRIBUTE_ARCHIVE);
}

void ChmodDialog::applyWindowsChanges()
{
    auto toSet   = [](QCheckBox* cb, DWORD flag, DWORD& s, DWORD& c) {
        if (cb->checkState() == Qt::Checked)   s |= flag;
        if (cb->checkState() == Qt::Unchecked) c |= flag;
    };

    DWORD setMask = 0, clearMask = 0;
    toSet(m_readOnly, FILE_ATTRIBUTE_READONLY, setMask, clearMask);
    toSet(m_hidden,   FILE_ATTRIBUTE_HIDDEN,   setMask, clearMask);
    toSet(m_system,   FILE_ATTRIBUTE_SYSTEM,   setMask, clearMask);
    toSet(m_archive,  FILE_ATTRIBUTE_ARCHIVE,  setMask, clearMask);

    QStringList errors;
    for (const QString& path : m_paths) {
        DWORD cur = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
        if (cur == INVALID_FILE_ATTRIBUTES) {
            errors << path;
            continue;
        }
        DWORD newAttrs = (cur & ~clearMask) | setMask;
        if (!SetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()), newAttrs))
            errors << path;
    }

    if (!errors.isEmpty()) {
        QMessageBox::warning(parentWidget(), tr("Change Attributes"),
            tr("Could not change attributes for some items:\n%1").arg(errors.join('\n')));
    }
}

#endif

void ChmodDialog::applyChanges()
{
#ifndef _WIN32
    applyUnixChanges();
#else
    applyWindowsChanges();
#endif
}