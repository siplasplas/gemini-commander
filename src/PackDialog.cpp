#include "PackDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>

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
    updateArchiveExtension();
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
    m_packerCombo = new QComboBox(this);
    m_packerCombo->addItem("zip");
    m_packerCombo->addItem("7z");
    connect(m_packerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PackDialog::onPackerChanged);
    formLayout->addRow(tr("Packer:"), m_packerCombo);

    mainLayout->addLayout(formLayout);

    // Move checkbox
    m_moveCheck = new QCheckBox(tr("Move files (delete originals after packing)"), this);
    mainLayout->addWidget(m_moveCheck);

    // 7z options group
    m_7zOptionsWidget = new QGroupBox(tr("7z Options"), this);
    auto* optionsLayout = new QFormLayout(m_7zOptionsWidget);

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
    optionsLayout->addRow(tr("Volume size:"), volumeLayout);

    // Solid block
    auto* solidLayout = new QHBoxLayout();
    m_solidBlockEdit = new QLineEdit(this);
    m_solidBlockEdit->setPlaceholderText(tr("empty = no solid"));
    m_solidBlockEdit->setText("1");
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
    m_7zOptionsWidget->setVisible(packerType() == "7z");
    updateArchiveExtension();
}

void PackDialog::onBrowseDestination()
{
    QString dir = QFileDialog::getExistingDirectory(
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
    return (m_packerCombo->currentIndex() == 0) ? ".zip" : ".7z";
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

QString PackDialog::volumeSize() const
{
    QString size = m_volumeSizeEdit->text().trimmed();
    if (size.isEmpty()) {
        return QString();
    }

    QString unit = m_volumeUnitCombo->currentText().toLower();
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

QString PackDialog::solidBlockSize() const
{
    QString size = m_solidBlockEdit->text().trimmed();
    if (size.isEmpty()) {
        // Solid mode enabled but no size specified - use default solid
        return {};
    }

    QString unit = m_solidBlockUnitCombo->currentText().toLower();
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
