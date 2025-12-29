#include "DistroInfoDialog.h"
#include "DistroInfo.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QClipboard>
#include <QGuiApplication>

DistroInfoDialog::DistroInfoDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Distribution Info"));
    resize(500, 400);

    auto* layout = new QVBoxLayout(this);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setFont(QFont("monospace"));
    m_textEdit->setPlainText(DistroInfo::fullReport());
    layout->addWidget(m_textEdit);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_copyButton = new QPushButton(tr("Copy All"), this);
    connect(m_copyButton, &QPushButton::clicked, this, &DistroInfoDialog::copyAll);
    buttonLayout->addWidget(m_copyButton);

    m_closeButton = new QPushButton(tr("Close"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeButton);

    layout->addLayout(buttonLayout);
}

void DistroInfoDialog::copyAll()
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(m_textEdit->toPlainText());
}