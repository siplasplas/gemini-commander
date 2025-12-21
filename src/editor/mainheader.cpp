#include "mainheader.h"

MainHeader::MainHeader(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void MainHeader::setupUi()
{
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(5);

    m_menuBar = new QMenuBar(this);

    m_spacer = new QWidget(this);
    m_spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(24, 24)); // Startowo większe ikonki

    m_layout->addWidget(m_menuBar, 0, 0);
    m_layout->addWidget(m_spacer, 0, 1);
    m_layout->addWidget(m_toolBar, 0, 2);
}

void MainHeader::setupMenus(QAction* openFile, QAction* closeFile,
                             QAction* exitApp, QAction* showSpecialChars, QAction* aboutApp,
                             QAction* findAction, QAction* findNextAction, QAction* findPrevAction)
{
    QMenu* fileMenu = m_menuBar->addMenu(tr("&File"));
    fileMenu->addAction(openFile);
    fileMenu->addSeparator();
    fileMenu->addAction(closeFile);
    fileMenu->addSeparator();
    fileMenu->addAction(exitApp);

    QMenu* searchMenu = m_menuBar->addMenu(tr("&Search"));
    if (findAction)
        searchMenu->addAction(findAction);
    if (findNextAction)
        searchMenu->addAction(findNextAction);
    if (findPrevAction)
        searchMenu->addAction(findPrevAction);

    QMenu* viewMenu = m_menuBar->addMenu(tr("&View"));
    viewMenu->addAction(showSpecialChars);

    QMenu* helpMenu = m_menuBar->addMenu(tr("&Help"));
    helpMenu->addAction(aboutApp);
}

void MainHeader::setupToolBar(QAction* buildProject, QAction* runProject)
{
    m_toolBar->addAction(buildProject);
    m_toolBar->addAction(runProject);
}

void MainHeader::resizeEvent(QResizeEvent *event)
{
    int w = width();

    recalculateThreshold();

    if (m_singleRow && w < (m_resizeThreshold - m_resizeHysteresis)) {
        switchToTwoRows();
    } else if (!m_singleRow && w > (m_resizeThreshold + m_resizeHysteresis)) {
        switchToSingleRow();
    }

    QWidget::resizeEvent(event);
}

void MainHeader::switchToSingleRow()
{
    m_layout->removeWidget(m_menuBar);
    m_layout->removeWidget(m_spacer);
    m_layout->removeWidget(m_toolBar);

    m_layout->addWidget(m_menuBar, 0, 0);
    m_layout->addWidget(m_spacer, 0, 1);
    m_layout->addWidget(m_toolBar, 0, 2);

    m_toolBar->setIconSize(QSize(24, 24)); // Większe ikonki w szerokim układzie

    m_singleRow = true;
}

void MainHeader::switchToTwoRows()
{
    m_layout->removeWidget(m_menuBar);
    m_layout->removeWidget(m_spacer);
    m_layout->removeWidget(m_toolBar);

    m_layout->addWidget(m_menuBar, 0, 0, 1, 2);
    m_layout->addWidget(m_toolBar, 1, 1, Qt::AlignRight);

    m_toolBar->setIconSize(QSize(18, 18)); // Mniejsze ikonki w wąskim układzie

    m_singleRow = false;
}

void MainHeader::recalculateThreshold()
{
    int menuWidth = m_menuBar ? m_menuBar->sizeHint().width() : 0;
    int toolBarWidth = m_toolBar ? m_toolBar->sizeHint().width() : 0;
    int extraSpace = 100; // Zapasu na marginesy, odstępy itp.

    m_resizeThreshold = menuWidth + toolBarWidth + extraSpace;
}
