#include "SearchDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QDir>
#include <QLabel>

SearchDialog::SearchDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Search Files"));
    setMinimumWidth(500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Path selection
    QHBoxLayout* pathLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setText(QDir::homePath());
    m_pathEdit->setPlaceholderText(tr("Enter directory path..."));

    m_browseButton = new QPushButton(tr("..."), this);
    m_browseButton->setFixedWidth(40);
    connect(m_browseButton, &QPushButton::clicked, this, &SearchDialog::onBrowse);

    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(m_browseButton);

    // Match mode selection
    m_matchModeCombo = new QComboBox(this);
    m_matchModeCombo->addItem(tr("MatchDefault"), static_cast<int>(QMimeDatabase::MatchDefault));
    m_matchModeCombo->addItem(tr("MatchExtension"), static_cast<int>(QMimeDatabase::MatchExtension));
    m_matchModeCombo->addItem(tr("MatchContent"), static_cast<int>(QMimeDatabase::MatchContent));
    m_matchModeCombo->setCurrentIndex(0);

    // Form layout for labels
    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow(tr("Search in:"), pathLayout);
    formLayout->addRow(tr("Match mode:"), m_matchModeCombo);

    // Description labels
    QLabel* descLabel = new QLabel(this);
    descLabel->setText(
        tr("<b>MatchDefault:</b> Use both extension and content analysis<br>"
           "<b>MatchExtension:</b> Use file extension only (fast)<br>"
           "<b>MatchContent:</b> Analyze file content (slower, more accurate)")
    );
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; margin-top: 10px; }");

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_startButton = new QPushButton(tr("Start"), this);
    m_startButton->setDefault(true);
    connect(m_startButton, &QPushButton::clicked, this, &SearchDialog::onStart);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_cancelButton);

    // Assemble layout
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(descLabel);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void SearchDialog::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Directory"),
        m_pathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        m_pathEdit->setText(dir);
    }
}

void SearchDialog::onStart()
{
    // Validate path
    QDir dir(m_pathEdit->text());
    if (!dir.exists()) {
        m_pathEdit->setFocus();
        m_pathEdit->selectAll();
        return;
    }

    accept();
}

QString SearchDialog::searchPath() const
{
    return m_pathEdit->text();
}

QMimeDatabase::MatchMode SearchDialog::matchMode() const
{
    int mode = m_matchModeCombo->currentData().toInt();
    return static_cast<QMimeDatabase::MatchMode>(mode);
}
