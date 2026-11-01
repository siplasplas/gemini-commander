#include "PackDialog.h"

#include "Plugins.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <qxfiledialog.h>
#include <QFileInfo>

namespace {

// Packers: the archive module writes them all; without it zip and 7z are
// packed by the zip and 7z programs.
struct Packer {
    const char* id;
    const char* extension;
    int maxLevel;      // 0: no compression level (plain tar)
    bool needsModule;
};

const Packer kPackers[] = {
    {"zip", ".zip", 9, false},
    {"7z", ".7z", 9, false},
    {"tar.gz", ".tar.gz", 9, true},
    {"tar.xz", ".tar.xz", 9, true},
    {"tar.zst", ".tar.zst", 22, true},
    {"tar.bz2", ".tar.bz2", 9, true},
    {"tar", ".tar", 0, true},
};

const Packer& packerOf(const QString& id)
{
    for (const Packer& packer : kPackers) {
        if (id == QLatin1String(packer.id))
            return packer;
    }
    return kPackers[0];
}

} // namespace

PackDialog::PackDialog(const QString& defaultName,
                       const QString& defaultDest,
                       int markedCount,
                       QWidget* parent)
    : QDialog(parent)
{
    Q_UNUSED(markedCount);

    setWindowTitle(tr("Pack Files"));
    setMinimumWidth(450);

    // Store base name without extension
    QFileInfo fi(defaultName);
    m_baseName = fi.completeBaseName();
    if (m_baseName.isEmpty()) {
        m_baseName = defaultName;
    }

    setupUi();

    // Set default values
    m_destinationEdit->setText(defaultDest);
    onPackerChanged(0);
}

void PackDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Archive name
    auto* formLayout = new QFormLayout();

    m_archiveNameEdit = new QLineEdit(this);
    formLayout->addRow(tr("Archive name:"), m_archiveNameEdit);

    // Destination with browse button
    auto* destLayout = new QHBoxLayout();
    m_destinationEdit = new QLineEdit(this);
    auto* browseButton = new QPushButton(tr("..."), this);
    browseButton->setFixedWidth(30);
    connect(browseButton, &QPushButton::clicked, this, &PackDialog::onBrowseDestination);
    destLayout->addWidget(m_destinationEdit);
    destLayout->addWidget(browseButton);
    formLayout->addRow(tr("Destination:"), destLayout);

    // Packer type combo
    const bool module = Plugins::instance().archive() != nullptr;
    m_packerCombo = new QComboBox(this);
    for (const Packer& packer : kPackers) {
        if (module || !packer.needsModule)
            m_packerCombo->addItem(QString::fromLatin1(packer.id));
    }
    connect(m_packerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PackDialog::onPackerChanged);
    formLayout->addRow(tr("Packer:"), m_packerCombo);

    // Compression level: -1 is the format's default
    m_levelSpin = new QSpinBox(this);
    m_levelSpin->setRange(-1, 9);
    m_levelSpin->setValue(-1);
    m_levelSpin->setSpecialValueText(tr("Default"));
    m_levelSpin->setToolTip(tr("0: fastest, 9: smallest (Zstandard: up to 22)"));
    formLayout->addRow(tr("Compression level:"), m_levelSpin);

    // Password: zip through the archive module (AES-256), 7z by the 7z
    // program (names encrypted too)
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("empty = none"));
    formLayout->addRow(tr("Password:"), m_passwordEdit);

    // Volume size
    auto* volumeLayout = new QHBoxLayout();
    m_volumeSizeEdit = new QLineEdit(this);
    m_volumeSizeEdit->setPlaceholderText(tr("empty = no split"));
    m_volumeSizeEdit->setFixedWidth(100);
    m_volumeUnitCombo = new QComboBox(this);
    m_volumeUnitCombo->addItem("B");
    m_volumeUnitCombo->addItem("KB");
    m_volumeUnitCombo->addItem("MB");
    m_volumeUnitCombo->addItem("GB");
    m_volumeUnitCombo->setCurrentIndex(2);  // MB default
    volumeLayout->addWidget(m_volumeSizeEdit);
    volumeLayout->addWidget(m_volumeUnitCombo);
    volumeLayout->addStretch();
    formLayout->addRow(tr("Volume size:"), volumeLayout);

    mainLayout->addLayout(formLayout);

    // Move checkbox
    m_moveCheck = new QCheckBox(tr("Move files (delete originals after packing)"), this);
    mainLayout->addWidget(m_moveCheck);

    // 7z options group
    m_7zOptionsWidget = new QGroupBox(tr("7z Options"), this);
    auto* optionsLayout = new QFormLayout(m_7zOptionsWidget);

    // Solid block
    auto* solidLayout = new QHBoxLayout();
    m_solidBlockEdit = new QLineEdit(this);
    // Empty: the archive module packs, as one solid block; a size needs 7z.
    m_solidBlockEdit->setPlaceholderText(tr("empty = one block"));
    m_solidBlockEdit->setToolTip(tr("Empty: one solid block for the whole archive.\n"
                                    "A size (e.g. 1 GB) packs with the 7z program."));
    m_solidBlockEdit->setFixedWidth(100);
    m_solidBlockUnitCombo = new QComboBox(this);
    m_solidBlockUnitCombo->addItem("B");
    m_solidBlockUnitCombo->addItem("KB");
    m_solidBlockUnitCombo->addItem("MB");
    m_solidBlockUnitCombo->addItem("GB");
    m_solidBlockUnitCombo->setCurrentIndex(2);  // MB default

    solidLayout->addWidget(m_solidBlockEdit);
    solidLayout->addWidget(m_solidBlockUnitCombo);
    solidLayout->addStretch();
    optionsLayout->addRow(tr("Solid block:"), solidLayout);

    m_7zOptionsWidget->setVisible(false);  // hidden by default (zip selected)
    mainLayout->addWidget(m_7zOptionsWidget);

    mainLayout->addStretch();

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_okButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void PackDialog::onPackerChanged(int index)
{
    Q_UNUSED(index);
    const QString type = packerType();
    const Packer& packer = packerOf(type);
    m_7zOptionsWidget->setVisible(type == "7z");
    m_levelSpin->setEnabled(packer.maxLevel > 0);
    m_levelSpin->setMaximum(qMax(packer.maxLevel, 0));
    // zip: AES-256 through the module (without it the zip program's weak
    // encryption is not offered); 7z: the 7z program.
    const bool encrypts = type == "7z" || (type == "zip" && Plugins::instance().archive());
    m_passwordEdit->setEnabled(encrypts);
    m_passwordEdit->setToolTip(type == "7z" ? tr("Packed by the 7z program; file names are encrypted too.")
                               : encrypts   ? tr("AES-256 encryption.")
                                            : tr("This format cannot be encrypted."));
    updateArchiveExtension();
}

void PackDialog::onBrowseDestination()
{
    QString dir = QxFileDialog::getExistingDirectory(
        this,
        tr("Select Destination Directory"),
        m_destinationEdit->text());

    if (!dir.isEmpty()) {
        m_destinationEdit->setText(dir);
    }
}

void PackDialog::updateArchiveExtension()
{
    m_archiveNameEdit->setText(m_baseName + currentExtension());
}

QString PackDialog::currentExtension() const
{
    return QString::fromLatin1(packerOf(packerType()).extension);
}

QString PackDialog::archiveName() const
{
    return m_archiveNameEdit->text();
}

QString PackDialog::destination() const
{
    return m_destinationEdit->text();
}

QString PackDialog::packerType() const
{
    return m_packerCombo->currentText();
}

bool PackDialog::moveFiles() const
{
    return m_moveCheck->isChecked();
}

int PackDialog::compressionLevel() const
{
    return m_levelSpin->isEnabled() ? m_levelSpin->value() : -1;
}

QString PackDialog::password() const
{
    return m_passwordEdit->isEnabled() ? m_passwordEdit->text() : QString();
}

QString PackDialog::sizeWithUnit(const QLineEdit* edit, const QComboBox* unitCombo)
{
    QString size = edit->text().trimmed();
    if (size.isEmpty())
        return {};

    QString unit = unitCombo->currentText().toLower();
    if (unit == "b") {
        unit = "";
    } else if (unit == "kb") {
        unit = "k";
    } else if (unit == "mb") {
        unit = "m";
    } else if (unit == "gb") {
        unit = "g";
    }

    return size + unit;
}

QString PackDialog::volumeSize() const
{
    return sizeWithUnit(m_volumeSizeEdit, m_volumeUnitCombo);
}

QString PackDialog::solidBlockSize() const
{
    return sizeWithUnit(m_solidBlockEdit, m_solidBlockUnitCombo);
}
