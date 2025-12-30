#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

class ConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigDialog(QWidget* parent = nullptr);

signals:
    void settingsApplied();

private slots:
    void onCategoryChanged(int index);
    void onApply();
    void onOk();

private:
    void setupUi();
    void createWindowPage();
    void createPanelsPage();
    void createHistoryPage();
    void createGeneralPage();

    void loadSettings();
    void saveSettings();
    void showRestartWarningIfNeeded();

    bool isWayland() const;

    // Initial values to detect changes
    int m_initialWidth = 0;
    int m_initialHeight = 0;

    // UI elements
    QListWidget* m_categoryList;
    QStackedWidget* m_pagesStack;

    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_applyButton;

    // Window page
    QSpinBox* m_windowWidth;
    QSpinBox* m_windowHeight;
    QSpinBox* m_windowX;
    QSpinBox* m_windowY;
    QLabel* m_waylandWarning;
    QCheckBox* m_positionEnabled;

    // Editor/Viewer relative position
    QSpinBox* m_editorWidth;
    QSpinBox* m_editorHeight;
    QSpinBox* m_editorX;
    QSpinBox* m_editorY;
    QSpinBox* m_viewerWidth;
    QSpinBox* m_viewerHeight;
    QSpinBox* m_viewerX;
    QSpinBox* m_viewerY;

    // Panels page
    QLineEdit* m_leftPanelStartDir;
    QLineEdit* m_rightPanelStartDir;
    QComboBox* m_leftSortColumn;
    QComboBox* m_leftSortOrder;
    QComboBox* m_rightSortColumn;
    QComboBox* m_rightSortOrder;

    // History page
    QSpinBox* m_maxHistorySize;
    QLabel* m_historyDescription;

    // General page
    QCheckBox* m_confirmExit;
    QCheckBox* m_showFunctionBar;
    QLineEdit* m_externalToolPath;
};