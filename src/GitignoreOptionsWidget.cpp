#include "GitignoreOptionsWidget.h"

#include "Config.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>

QList<GitignoreOptionsWidget*> GitignoreOptionsWidget::s_instances;

GitignoreOptionsWidget::GitignoreOptionsWidget(QWidget* parent)
    : QWidget(parent)
{
    s_instances.append(this);
    const GitignoreOptions options = Config::instance().gitignoreOptions();
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);

    m_topCombo = new QComboBox(this);
    m_topCombo->addItem(tr("the nearest directory with .git (git repository)"),
                        static_cast<int>(GitignoreOptions::Top::NearestGit));
    m_topCombo->addItem(tr("the highest directory with .gitignore below home"),
                        static_cast<int>(GitignoreOptions::Top::HighestGitignore));
    m_topCombo->setCurrentIndex(m_topCombo->findData(static_cast<int>(options.top)));
    m_topCombo->setToolTip(tr("Above the directory, .gitignore files count from here down. "
                              "The highest .gitignore lets one .gitignore serve a group of "
                              "repositories, e.g. to copy them to a USB drive. The search stops "
                              "below the home directory. Shared by the file panels, Copy and "
                              "Find Files."));
    form->addRow(tr(".gitignore from:"), m_topCombo);

    m_skipGitCheck = new QCheckBox(tr("Skip .git directories too"), this);
    m_skipGitCheck->setToolTip(tr("Off: .git directories are shown, counted, copied and searched "
                                  "(unless a .gitignore ignores them). Shared by the file panels, "
                                  "Copy and Find Files."));
    m_skipGitCheck->setChecked(options.skipGitDir);
    form->addRow(m_skipGitCheck);

    connect(m_topCombo, &QComboBox::currentIndexChanged, this, &GitignoreOptionsWidget::changed);
    connect(m_skipGitCheck, &QCheckBox::toggled, this, &GitignoreOptionsWidget::changed);
}

GitignoreOptionsWidget::~GitignoreOptionsWidget()
{
    s_instances.removeOne(this);
}

GitignoreOptions GitignoreOptionsWidget::options() const
{
    GitignoreOptions options;
    options.top = static_cast<GitignoreOptions::Top>(m_topCombo->currentData().toInt());
    options.skipGitDir = m_skipGitCheck->isChecked();
    return options;
}

void GitignoreOptionsWidget::setOptions(const GitignoreOptions& options)
{
    m_topCombo->setCurrentIndex(m_topCombo->findData(static_cast<int>(options.top)));
    m_skipGitCheck->setChecked(options.skipGitDir);
}

void GitignoreOptionsWidget::storeInConfig() const
{
    const GitignoreOptions current = options();
    Config::instance().setGitignoreOptions(current);
    for (GitignoreOptionsWidget* other : s_instances)
        if (other != this)
            other->setOptions(current);
}
