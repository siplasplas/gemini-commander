#pragma once

#include <QWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QGridLayout>

/**
 * @class MainHeader
 * @brief Adaptive header widget combining menu bar and toolbar
 *
 * Automatically switches between single-row and two-row layout based on available width
 */
class MainHeader : public QWidget
{
    Q_OBJECT

public:
    explicit MainHeader(QWidget *parent = nullptr);

    /// @brief Access menu bar component
    QMenuBar *menuBar() const { return m_menuBar; }

    /// @brief Access toolbar component
    QToolBar *toolBar() const { return m_toolBar; }

    /**
     * @brief Populates menu structure
     * @param openFile Action for opening files
     * @param closeFile Action for closing files
     * @param exitApp Action for exiting application
     * @param showSpecialChars Action for showing special characters
     * @param aboutApp Action for about dialog
     * @param findAction Action for Find (Ctrl+F)
     * @param findNextAction Action for Find Next (F3)
     * @param findPrevAction Action for Find Previous (Shift+F3)
     */
    void setupMenus(QAction* openFile, QAction* closeFile,
                    QAction* exitApp, QAction* showSpecialChars, QAction* wrapLines,
                    QAction* aboutApp, QAction* findAction = nullptr,
                    QAction* findNextAction = nullptr, QAction* findPrevAction = nullptr,
                    QAction* replaceAction = nullptr, QAction* gotoAction = nullptr);

    void setupToolsMenu(QAction* insertDate, QAction* insertTime, QAction* insertBoth);

    /**
     * @brief Configures toolbar buttons
     * @param buildProject Build action to add
     * @param runProject Run action to add
     */
    void setupToolBar(QAction* buildProject, QAction* runProject);

protected:
    /// @brief Handles dynamic layout changes on window resize
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    void switchToSingleRow();
    void switchToTwoRows();
    void recalculateThreshold();

    QGridLayout *m_layout = nullptr;
    QMenuBar *m_menuBar = nullptr;
    QToolBar *m_toolBar = nullptr;
    QWidget *m_spacer = nullptr;

    bool m_singleRow = true;
    int m_resizeThreshold = 1000;
    int m_resizeHysteresis = 50;
};