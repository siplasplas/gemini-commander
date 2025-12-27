#ifndef MRUTABWIDGET_H
#define MRUTABWIDGET_H

#include <QTabWidget>
#include <QTabBar>
#include <QList>
#include <QTimer>
#include <QPointer>
#include <QMap>
#include <QResizeEvent>
#include <QMenu>
#include <QMetaMethod>

class QDialog;
class QListWidget;
class QListWidgetItem;
class QAbstractButton;

/**
 * @class MruTabWidget
 * @brief Extended QTabWidget with MRU navigation, tab pinning and smart tab management
 *
* Provides enhanced tab management features for modern IDE-style applications:
 * - MRU (Most Recently Used) navigation using Ctrl+Tab/Ctrl+Shift+Tab sequences
 * - Automatic tab management with configurable unpinned tab limit and auto-saving
 * - Context-sensitive close button visibility (visible only for selected/hovered tabs)
 * - Persistent pinned tabs with independent lifetime management
 * - don't use QTabWidget::tabCloseRequested, use instead MruTabWidget signals
 * @see Documentation : docs/widgets/MruTabWidget.md
 */
class MruTabWidget : public QTabWidget
{
    Q_OBJECT
signals:
    void tabContextMenuRequested(int tabIndex, QMenu* menu);
    void tabAboutToClose(int index, bool askPin, bool &allow_close);
    void actionsBeforeTabClose(int index);
public:
    /**
     * @brief Constructs an MRU-enabled tab widget
     * @param parent Parent widget
     */
    explicit MruTabWidget(QWidget *parent = nullptr);
    bool requestCloseTab(int index, bool askPin = false);

    /**
     * @brief Destructor cleans up resources
     */
    ~MruTabWidget() override;

    /**
     * @brief Sets maximum number of allowed tabs
     * @param limit Maximum tab count (0 = unlimited)
     */
    void setTabLimit(int limit);

    /**
     * @brief Enforces currently set tab limit
     *
     * Closes least recently used tabs until under limit
     */
    int enforceTabLimit();

    /**
     * @brief Sets pin state for a tab
     * @param tabIndex Index of tab to modify
     * @param pinned Whether to pin the tab
     */
    void setTabPinned(int tabIndex, bool pinned);
    bool isTabPinned(int tabIndex) const;
    bool requestCloseAllTabs();
    void closeOtherTabs(int keepIndex);
    void closeTabsToLeft(int fromIndex);
    void closeTabsToRight(int fromIndex);
    void setPinIconUri(QString iconUri) { m_pinIconUri = iconUri; }

protected:
    // Override key event handlers
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    // Override tab management methods to keep MRU list consistent
    void tabRemoved(int index) override;
    void tabInserted(int index) override;

    // Override show/hide events to manage filter installation
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

    // Override resize event to handle geometry changes
    void resizeEvent(QResizeEvent *event) override;

    // The event filter method
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onCurrentChanged(int index);
    void handleCtrlTabTimeout();
    void onPopupListItemActivated(QListWidgetItem *item);

private:
    void updateMruOrder(int index);
    void showMruPopup();
    void hideMruPopup();
    void cycleMruPopup(bool forward);
    void activateSelectedMruTab();
    void performDirectSwitch();

    void onTabContextMenuRequested(const QPoint& pos);
    void updateTabButton(int index);

    void installTabBarEventFilter();
    void removeTabBarEventFilter();
    void updateCloseButtonVisibility();
    void mapCloseButtonsToTabs();

    // Add new private methods
    QVector<QWidget*> findLeastRecentlyUsedUnpinnedTabs(int atMost) const;
    int pinnedTabCount() const;


    // --- Member Variables ---
    QList<int> m_mruOrder;
    bool m_ctrlHeld = false;
    QTimer m_ctrlTabTimer;
    bool m_expectingPopup = false;
    bool m_shiftHeldOnTabPress = false;
    QString m_pinIconUri;

    QPointer<QDialog> m_mruPopup;
    QPointer<QListWidget> m_mruListWidget;

    // --- New members for close button visibility ---
    int m_hoveredTabIndex = -1; // Index of the tab currently hovered over (-1 if none)
    bool m_isTabBarFilterInstalled = false;
    // Map to store which button corresponds to which tab index
    // We use QPointer to handle buttons being deleted automatically
    QMap<int, QPointer<QAbstractButton>> m_tabIndexToCloseButtonMap;

    QVector<bool> m_pinnedTabs;
    int m_tabLimit = 0;
};

#endif // MRUTABWIDGET_H