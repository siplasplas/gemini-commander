#include "mrutabwidget.h"
#include <QTabWidget>
#include <QTabBar>
#include <QEvent>
#include <QHoverEvent>
#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QPointer>
#include <QTimer>
#include <QSet>
#include <QMetaMethod>
#include <cmath>
#include <QToolButton>
#include <QDebug>
#include <QMenu>

#include "utils/Ev.h"

constexpr int CTRL_TAB_TIMEOUT_MS = 200;

/**
 * @brief Constructs MruTabWidget with parent
 * @param parent Parent widget
 */
MruTabWidget::MruTabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    connect(this, &QTabWidget::currentChanged, this, &MruTabWidget::onCurrentChanged);
    m_ctrlTabTimer.setSingleShot(true);
    m_ctrlTabTimer.setInterval(CTRL_TAB_TIMEOUT_MS);
    connect(&m_ctrlTabTimer, &QTimer::timeout, this, &MruTabWidget::handleCtrlTabTimeout);

    assert(tabBar());
    tabBar()->setMouseTracking(true);
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QTabBar::customContextMenuRequested, this, &MruTabWidget::onTabContextMenuRequested);

    connect(this, &QTabWidget::tabCloseRequested, [this](int index) {
        requestCloseTab(index);
    });
}


bool MruTabWidget::requestCloseTab(int index, bool askPin)
{
    assert(index >= 0 && index < count());

    QWidget* tab = widget(index);
    assert(tab);

    bool allowClose = true;
    emit tabAboutToClose(index, askPin, allowClose);
    if (!allowClose) return false;
    emit actionsBeforeTabClose(index);
    tab->deleteLater();
    removeTab(index);
    return true;
}


/**
 * @brief Destructor cleans up resources
 */
MruTabWidget::~MruTabWidget()
{
    hideMruPopup();
    removeTabBarEventFilter();
}

void MruTabWidget::setTabLimit(int limit)
{
    m_tabLimit = limit;
    enforceTabLimit();
}


/**
 * @brief Handles tab bar event filter installation
 */
void MruTabWidget::installTabBarEventFilter() {
    if (tabBar() && !m_isTabBarFilterInstalled) {
        tabBar()->installEventFilter(this);
        m_isTabBarFilterInstalled = true;
        // Initial mapping and visibility update
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
    } else if (!tabBar()) {
         qWarning() << "[installTabBarEventFilter] Attempted to install filter, but tabBar is NULL!"; // Debug Hover
    } else {
    }
}

/**
 * @brief Handles tab bar event filter removal
 */
void MruTabWidget::removeTabBarEventFilter() {
     if (tabBar() && m_isTabBarFilterInstalled) {
        tabBar()->removeEventFilter(this);
        m_isTabBarFilterInstalled = false;
    }
}

/**
 * @brief Handles widget show event
 * @param event Show event
 */
void MruTabWidget::showEvent(QShowEvent *event) {
    QTabWidget::showEvent(event);
    installTabBarEventFilter(); // Install filter when widget becomes visible
}

/**
 * @brief Handles widget hide event
 * @param event Hide event
 */
void MruTabWidget::hideEvent(QHideEvent *event) {
    removeTabBarEventFilter(); // Remove filter when widget is hidden
    QTabWidget::hideEvent(event);
}

// --- Event Handlers ---

// Override resizeEvent to update button mapping on resize
void MruTabWidget::resizeEvent(QResizeEvent *event) {
    QTabWidget::resizeEvent(event); // Call base implementation first

    // Remap buttons and update their visibility after the widget has been resized
    // Using QTimer::singleShot ensures the layout is stable
    QTimer::singleShot(0, this, [this]() {
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
    });
}

/**
 * @brief Handles key press events
 * @param event Key event
 *
 * Implements:
 * - Ctrl+Tab/Shift+Ctrl+Tab navigation
 * - MRU popup triggering
 */
void MruTabWidget::keyPressEvent(QKeyEvent *event)
{
    // Handle Ctrl+Tab / Ctrl+Shift+Tab
    if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab)
        {
            if (count() < 2) { // Ignore if less than 2 tabs
                QTabWidget::keyPressEvent(event);
                return;
            }

            // If Ctrl was *not* pressed before -> start timer
            if (!m_ctrlHeld) {
                m_ctrlHeld = true;
                m_expectingPopup = true; // Expecting a potential popup
                m_shiftHeldOnTabPress = (event->modifiers() & Qt::ShiftModifier); // Remember Shift state
                m_ctrlTabTimer.start(); // Start timer
            }
             // If Ctrl *was* already pressed OR popup is visible
            else {
                 // If timer was active (quick second press) -> show popup
                 if (m_ctrlTabTimer.isActive()) {
                     m_ctrlTabTimer.stop(); // Stop timer
                     m_expectingPopup = false;
                     if (!m_mruPopup) { // Show popup if not already there
                          showMruPopup();
                     }
                 }
                 // If popup *is* visible -> do nothing here, eventFilter will handle it
                 // If popup is not visible and we are not expecting it -> show as fallback?
                 else if (!m_expectingPopup && !m_mruPopup) {
                     showMruPopup();
                 }
            }

            event->accept(); // Event handled
            return;
        }
    }
    // Pass other key press events to the base class
    QTabWidget::keyPressEvent(event);
}

/**
 * @brief Handles key release events
 * @param event Key event
 *
 * Implements:
 * - Ctrl key release handling
 * - MRU popup confirmation
 */
void MruTabWidget::keyReleaseEvent(QKeyEvent *event)
{
    // Handle Ctrl key release
    if (event->key() == Qt::Key_Control) {
        bool wasCtrlHeld = m_ctrlHeld; // Store state before reset
        m_ctrlHeld = false;            // Reset state immediately
        m_expectingPopup = false;     // Reset this too

        // If timer was active (short press/release) -> perform direct switch
        if (m_ctrlTabTimer.isActive()) {
            m_ctrlTabTimer.stop();
            performDirectSwitch(); // Switch to previous tab
            event->accept();
        }
        // If popup was visible -> activate selected tab and hide popup
        else if (wasCtrlHeld && m_mruPopup) {
            activateSelectedMruTab();
            hideMruPopup();
            event->accept();
        }
        // Otherwise allow default processing
    }
    else {
        // Pass other key release events to the base class
        QTabWidget::keyReleaseEvent(event);
    }
}

/**
 * @brief Updates MRU order when current tab changes
 * @param index New current tab index
 */
void MruTabWidget::onCurrentChanged(int index)
{
    // Update the MRU order when the selected tab changes
    if (index >= 0) {
        updateMruOrder(index);
    }
    // Hide the popup if it's open and Ctrl is not pressed (e.g., changed by clicking)
    if (m_mruPopup && !m_ctrlHeld) {
         hideMruPopup();
    }
    // Update the visibility of close buttons when the selected tab changes
    updateCloseButtonVisibility();
}

/**
 * @brief Handles Ctrl+Tab timeout for popup display
 */
void MruTabWidget::handleCtrlTabTimeout()
{
    // Timer timed out -> show the popup if Ctrl is still held
    m_expectingPopup = false; // We are no longer expecting (either we show or we don't)
    if (m_ctrlHeld && !m_mruPopup && count() >= 2) {
        showMruPopup(); // Show the popup (and install the event filter for the list)
    }
}

/**
 * @brief Handles MRU popup list activation
 * @param item Activated list item
 */
void MruTabWidget::onPopupListItemActivated(QListWidgetItem *item)
{
    // Handle list item activation (e.g., Enter, double-click)
    if(item && m_ctrlHeld) { // Check if Ctrl is still held
         activateSelectedMruTab();
         hideMruPopup();
    }
}

/**
 * @brief Updates MRU list after tab removal
 * @param index Removed tab index
 */
void MruTabWidget::tabRemoved(int index)
{
    // Update the MRU list *before* the tab is actually removed
    m_mruOrder.removeAll(index); // Remove all occurrences of the index
    // Adjust indices in the MRU list that were greater than the removed one
    for (int i = 0; i < m_mruOrder.size(); ++i) {
        if (m_mruOrder[i] > index) {
            m_mruOrder[i]--;
        }
    }

    // Remove entry from pinned tabs vector
    if (index < m_pinnedTabs.size()) {
        m_pinnedTabs.remove(index);
    }

    // The base class handles the actual removal and emits signals
    // We update the mapping *after* the tab is visually gone
    // Using QTimer::singleShot ensures mapping happens after potential layout changes
    QTimer::singleShot(0, this, [this]() {
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
    });
    // No need to call QTabWidget::tabRemoved(index) - the signal handles it
}

void MruTabWidget::tabInserted(int index) {
    // Adjust MRU order for indices after the insertion point
    for (int i = 0; i < m_mruOrder.size(); ++i) {
        if (m_mruOrder[i] >= index) {
            m_mruOrder[i]++;
        }
    }
    m_mruOrder.prepend(index);

    // Insert entry in pinned tabs vector
    if (index >= m_pinnedTabs.size()) {
        m_pinnedTabs.resize(count());
    } else {
        m_pinnedTabs.insert(index, false);
    }

    // The base class handles insertion and emits signals
    // Update mapping *after* the tab is visually added
    QTimer::singleShot(0, this, [this]() {
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
    });
    // No need to call QTabWidget::tabInserted(index)
}

/**
 * @brief Main event filter for tab bar and MRU popup
 * @param watched Event target object
 * @param event Event object
 * @return true if event was handled
 *
 * Handles:
 * - Tab bar hover events
 * - MRU popup keyboard navigation
 */
bool MruTabWidget::eventFilter(QObject *watched, QEvent *event)
{
    // Debug: Log all events coming to the filter for watched objects
    // Only log for tabBar or listWidget to avoid excessive output
    if (watched == tabBar() || watched == m_mruListWidget) {
    }

    // --- Filtering events for QTabBar ---
    if (watched == tabBar()) {
        // Check mouse tracking status on any event for the tab bar
        if (tabBar() && !tabBar()->hasMouseTracking()) {
             qWarning() << "[eventFilter] Mouse tracking is DISABLED on tabBar!";
        }

        switch (event->type()) {
            // --- Handle MouseMove INSTEAD of HoverMove ---
            case QEvent::MouseMove: {
                 if (!tabBar()) {
                     qWarning() << "[MouseMove] tabBar is NULL!";
                     return false;
                 }
                 QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                 int tabIndex = tabBar()->tabAt(mouseEvent->pos());
                 if (tabIndex != m_hoveredTabIndex) {
                    m_hoveredTabIndex = tabIndex;
                    updateCloseButtonVisibility();
                 }
                 return false; // Allow normal processing
            }
            // --- Keep HoverMove as a fallback (though likely not triggered) ---
            case QEvent::HoverMove: {
                if (!tabBar()) { // Paranoid check
                     qWarning() << "[HoverMove] tabBar is NULL!";
                     return false;
                }
                QHoverEvent *hoverEvent = static_cast<QHoverEvent*>(event);
                // Use QTabBar::tabAt to find the tab index under the cursor
                int tabIndex = tabBar()->tabAt(Ev::local(hoverEvent));
                // If the hover index changed
                if (tabIndex != m_hoveredTabIndex) {
                    m_hoveredTabIndex = tabIndex; // Save the new index
                    updateCloseButtonVisibility(); // Update button visibility
                }
                // Return false to allow normal hover processing (e.g., tooltips)
                return false;
            }
            // Mouse leave event from the tab bar area
            case QEvent::Leave: // Handle Leave event (often paired with MouseMove)
            case QEvent::HoverLeave: {
                 if (!tabBar()) { // Paranoid check
                     qWarning() << "[Leave/HoverLeave] tabBar is NULL!";
                     return false;
                }
                // Mouse left the tab bar area
                if (m_hoveredTabIndex != -1) { // If it was previously over a tab
                    m_hoveredTabIndex = -1; // Reset the hover index
                    updateCloseButtonVisibility(); // Update visibility
                }
                return false; // Allow normal processing
            }
            // Mouse clicks - update visibility just in case
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
                 // Update visibility with a slight delay to ensure the state is current
                 QTimer::singleShot(0, this, &MruTabWidget::updateCloseButtonVisibility);
                 return false; // Allow normal click processing

            default:
                break; // Ignore other events for the tab bar
        }
    }
    // --- Filtering events for the MRU popup list (as before) ---
    else if (watched == m_mruListWidget && m_mruPopup) {
        // (MRU popup event handling code remains the same)
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            // Handle Ctrl+Tab/Ctrl+Shift+Tab in the popup
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
                    bool isForward = (keyEvent->key() == Qt::Key_Tab);
                    bool isShiftPressed = (keyEvent->modifiers() & Qt::ShiftModifier);
                    cycleMruPopup(isForward && !isShiftPressed); // Cycle selection in the popup
                    return true; // Event handled
                }
            }
            // Handle Escape in the popup
            else if (keyEvent->key() == Qt::Key_Escape) {
                hideMruPopup(); // Hide popup
                m_ctrlHeld = false; // Reset Ctrl state
                return true; // Event handled
            }
        }
        // Handle Ctrl release while the popup is active
        else if (event->type() == QEvent::KeyRelease) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Control) {
                // If the popup was visible -> activate the selected tab and hide
                if (m_mruPopup && m_mruPopup->isVisible()) {
                    activateSelectedMruTab();
                    hideMruPopup(); // This will also remove the event filter from the list
                }
                m_ctrlHeld = false; // Reset Ctrl state
                return true; // Event handled
            }
        }
    }

    // Pass unhandled events to the base class filter
    return QTabWidget::eventFilter(watched, event);
}

// --- Close Button Visibility Logic ---

// Maps found close buttons to tab indices
void MruTabWidget::mapCloseButtonsToTabs() {
    if (!tabBar()) return; // Check if the tab bar exists

    m_tabIndexToCloseButtonMap.clear(); // Clear the old map
    // Find all QAbstractButton widgets that are children of the QTabBar
    QList<QAbstractButton*> buttons = tabBar()->findChildren<QAbstractButton*>();

    for (QAbstractButton *button : buttons) {
        // Ensure the button is valid and has geometry
        if (!button || button->geometry().isEmpty()) {
             continue; // Skip invalid or geometry-less buttons
        }

        // --- NEW HEURISTIC (Class Focus) ---
        const QMetaObject *metaObj = button->metaObject();
        QString className = metaObj ? metaObj->className() : "";
        QString objName = button->objectName(); // Get object name for scroll button check

        // 1. Ignore known scroll buttons
        if (objName == "ScrollLeftButton" || objName == "ScrollRightButton") {
            continue;
        }

        // 2. Check if the class is "CloseButton"
        bool looksLikeCloseButton = (className == "CloseButton");

        if (looksLikeCloseButton) {
            // 3. Find the tab index for this button
            QPoint buttonCenter = button->geometry().center();
            int tabIndex = tabBar()->tabAt(buttonCenter);

            if (tabIndex != -1) {
                 m_tabIndexToCloseButtonMap[tabIndex] = button; // Add to map
            } else {
            }
        } else {
        }
        // --- END OF NEW HEURISTIC ---
    }
}

// Updates the visibility of close buttons based on state
void MruTabWidget::updateCloseButtonVisibility() {
    if (!tabBar()) return; // Check if the tab bar exists

    int currentSelectedTabIndex = currentIndex(); // Current selected index

    // Set of buttons that *should* be visible according to logic
    QSet<QAbstractButton*> buttonsToShow;

    // Iterate through the mapped buttons to determine which should be shown
    // We now use the map which should only contain the actual CloseButtons
    for (auto it = m_tabIndexToCloseButtonMap.constBegin(); it != m_tabIndexToCloseButtonMap.constEnd(); ++it) {
        int tabIndex = it.key();
        QPointer<QAbstractButton> button = it.value();

        if (!button) continue; // Skip if the button pointer is invalid

        bool isSelected = (tabIndex == currentSelectedTabIndex); // Is the tab selected?
        bool isHovered = (tabIndex == m_hoveredTabIndex);      // Is the tab hovered?

        // If the tab is selected OR hovered -> the button should be visible
        if (isSelected || isHovered) {
            buttonsToShow.insert(button.data()); // Add the raw pointer to the set
        }
    }

    // Now iterate through *all* buttons we mapped (i.e., only CloseButtons)
    // and set their visibility
    for (auto it = m_tabIndexToCloseButtonMap.constBegin(); it != m_tabIndexToCloseButtonMap.constEnd(); ++it) {
         QPointer<QAbstractButton> button = it.value();
         if (!button) continue;

         bool shouldBeVisible = buttonsToShow.contains(button.data());

         if (button->isVisible() != shouldBeVisible) {
             button->setVisible(shouldBeVisible);
         }
    }

    // We no longer need to iterate through `allButtons` because we only manage
    // the ones identified as CloseButton in the map.
    // The visibility of scroll arrows is managed by QTabBar itself.
}

bool MruTabWidget::isTabPinned(int tabIndex) const
{
    if (tabIndex < 0 || tabIndex >= m_pinnedTabs.size()) {
        return false;
    }
    return m_pinnedTabs[tabIndex];
}

QVector<QWidget*> MruTabWidget::findLeastRecentlyUsedUnpinnedTabs(int atMost) const
{
    QVector<QWidget*> result;
    if (atMost <= 0) return result;

    // First, try to find unpinned tabs from MRU list (least recent to most recent)
    for (int i = m_mruOrder.size() - 1; i >= 0 && result.size() < atMost; --i) {
        int tabIndex = m_mruOrder[i];
        if (tabIndex >= 0 && tabIndex < count() && !isTabPinned(tabIndex)) {
            QWidget* w = widget(tabIndex);
            if (w && !result.contains(w)) {
                result.append(w);
            }
        }
    }

    // If we still need more, search through all tabs from right to left
    if (result.size() < atMost) {
        for (int i = count() - 1; i >= 0 && result.size() < atMost; --i) {
            if (!isTabPinned(i)) {
                QWidget* w = widget(i);
                if (w && !result.contains(w)) {
                    result.append(w);
                }
            }
        }
    }

    return result;
}

int MruTabWidget::pinnedTabCount() const
{
    int count = 0;
    for (bool pinned : m_pinnedTabs) {
        if (pinned) count++;
    }
    return count;
}

// Modify updateMruOrder to skip pinned tabs
void MruTabWidget::updateMruOrder(int index) {
    if (count() < 2)
        return;
    if (m_mruOrder.count() > count())
        return;
    // Remove the index if it already exists
    m_mruOrder.removeAll(index);
    // Insert the index at the beginning (most recently used)
    m_mruOrder.prepend(index);
}

void MruTabWidget::showMruPopup()
{
    // Check preconditions
    if (m_mruPopup || count() < 2) {
        return;
    }

    // Build the list of tabs to show in the popup (MRU + rest)
    QList<int> tabsToShowOrder;
    QSet<int> addedIndices;
    for (int index : m_mruOrder) {
        if (index >= 0 && index < count() && !addedIndices.contains(index)) {
            tabsToShowOrder.append(index);
            addedIndices.insert(index);
        }
    }
    for (int i = 0; i < count(); ++i) {
        if (!addedIndices.contains(i)) {
            tabsToShowOrder.append(i);
            addedIndices.insert(i);
        }
    }

    if (tabsToShowOrder.size() < 2) {
         return; // Don't show if less than 2 tabs
    }

    // Create and configure the popup
    m_mruPopup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    m_mruPopup->setObjectName("MruPopupDialog");

    m_mruListWidget = new QListWidget(m_mruPopup);
    m_mruListWidget->setObjectName("MruPopupList");
    m_mruListWidget->setAlternatingRowColors(true);
    connect(m_mruListWidget, &QListWidget::itemActivated, this, &MruTabWidget::onPopupListItemActivated);
    if (m_mruListWidget) {
        m_mruListWidget->installEventFilter(this); // Event filter for keys on the list
    }

    // Populate the list widget based on the new order
    for (int tabIndex : tabsToShowOrder) {
        if (tabIndex >= 0 && tabIndex < count()) {
            QListWidgetItem *item = new QListWidgetItem(tabText(tabIndex), m_mruListWidget);
            item->setData(Qt::UserRole, tabIndex); // Store the tab index
            item->setIcon(tabIcon(tabIndex));
            m_mruListWidget->addItem(item);
        }
    }

    // Popup layout
    QVBoxLayout *layout = new QVBoxLayout(m_mruPopup);
    layout->addWidget(m_mruListWidget);
    layout->setContentsMargins(2, 2, 2, 2);
    m_mruPopup->setLayout(layout);

    // Set initial selection (second tab in the list - the next one to switch to)
    if (m_mruListWidget->count() > 1) {
        m_mruListWidget->setCurrentRow(1);
    } else {
         m_mruListWidget->setCurrentRow(0); // Fallback
    }

    // Position and show the popup
    QPoint center = mapToGlobal(rect().center());
    QSize popupSizeHint = m_mruPopup->sizeHint();
    popupSizeHint.setWidth(qMax(popupSizeHint.width(), width() / 3)); // Minimum width
    popupSizeHint.setHeight(qMin(popupSizeHint.height(), qMin(height() * 2 / 3, 400))); // Limit height
    m_mruPopup->resize(popupSizeHint);

    int x = center.x() - m_mruPopup->width() / 2;
    int y = center.y() - m_mruPopup->height() / 2;

    // Ensure the popup fits on the screen
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        x = qBound(screenGeometry.left(), x, screenGeometry.right() - m_mruPopup->width());
        y = qBound(screenGeometry.top(), y, screenGeometry.bottom() - m_mruPopup->height());
    }

    m_mruPopup->move(x, y);
    m_mruPopup->setFocus(); // Set focus to the popup
    m_mruListWidget->setFocus(); // Set focus to the list
    m_mruPopup->show();
}

void MruTabWidget::hideMruPopup()
{
    if (m_mruPopup) {
        if (m_mruListWidget) {
            m_mruListWidget->removeEventFilter(this); // Remove event filter from the list
        }
        m_mruPopup->hide();
        m_mruPopup->deleteLater(); // Schedule object deletion
        // QPointer will automatically set itself to nullptr
    }
}

void MruTabWidget::cycleMruPopup(bool forward)
{
    // Cycle selection in the popup list
    if (!m_mruPopup || !m_mruListWidget || m_mruListWidget->count() == 0) {
        return;
    }
    int count = m_mruListWidget->count();
    int currentRow = m_mruListWidget->currentRow();
    int nextRow = forward ? (currentRow + 1) % count : (currentRow - 1 + count) % count;
    m_mruListWidget->setCurrentRow(nextRow);
}

void MruTabWidget::activateSelectedMruTab()
{
    // Activate the tab selected in the popup
    if (!m_mruPopup || !m_mruListWidget) return;
    QListWidgetItem *selectedItem = m_mruListWidget->currentItem();
    if (selectedItem) {
        bool ok;
        int indexToActivate = selectedItem->data(Qt::UserRole).toInt(&ok); // Get index from data
        if (ok && indexToActivate >= 0 && indexToActivate < count()) {
            setCurrentIndex(indexToActivate); // Set as current tab
        }
    }
}

void MruTabWidget::performDirectSwitch()
{
    // Perform the quick switch (without popup)
    if (m_mruOrder.size() >= 2) {
        // MRU logic: Switch to the second element (index 1)
        int indexToActivate = m_mruOrder[1];
        if (indexToActivate >= 0 && indexToActivate < count()) {
            setCurrentIndex(indexToActivate);
        }
    } else {
        // Cyclic logic when MRU < 2
        int current = currentIndex();
        int numTabs = count();
        if (numTabs > 1) {
            int nextIndex = m_shiftHeldOnTabPress ? (current - 1 + numTabs) % numTabs // Backwards if Shift
                                                  : (current + 1) % numTabs; // Forwards if no Shift
            setCurrentIndex(nextIndex);
        }
    }
}

bool MruTabWidget::requestCloseAllTabs()
{
    int leaved = 0;
    for (int index = count() - 1; index >= 0; index--)
    {
        if (!requestCloseTab(index))
            leaved++;
    }
    return leaved == 0;
}

void MruTabWidget::closeOtherTabs(int keepIndex)
{
    for (int i = count() - 1; i >= 0; --i)
    {
        if (i != keepIndex)
            requestCloseTab(i);
    }
}

void MruTabWidget::closeTabsToLeft(int fromIndex)
{
    for (int i = fromIndex - 1; i >= 0; --i)
    {
        requestCloseTab(i);
    }
}

void MruTabWidget::closeTabsToRight(int fromIndex)
{
    for (int i = count() - 1; i > fromIndex; --i)
    {
        requestCloseTab(i);
    }
}

void MruTabWidget::onTabContextMenuRequested(const QPoint& pos)
{
    int tabIndex = tabBar()->tabAt(pos);
    if (tabIndex < 0) return; // A blank space was clicked

    QMenu menu(this);

    QAction* closeAction = menu.addAction(tr("Close"));
    closeAction->setShortcut(QKeySequence::Close); // Ctrl+F4
    connect(closeAction, &QAction::triggered, this, [this, tabIndex]() {
        requestCloseTab(tabIndex);
    });

    QAction* closeOthersAction = menu.addAction(tr("Close Other Tabs"));
    connect(closeOthersAction, &QAction::triggered, this, [this, tabIndex]() {
        closeOtherTabs(tabIndex);
    });

    QAction* closeAllAction = menu.addAction(tr("Close All Tabs"));
    connect(closeAllAction, &QAction::triggered, this, [this]() {
        requestCloseAllTabs();
    });

    menu.addSeparator();

    QAction* closeLeftAction = menu.addAction(tr("Close Tabs to the Left"));
    connect(closeLeftAction, &QAction::triggered, this, [this, tabIndex]() {
        closeTabsToLeft(tabIndex);
    });

    QAction* closeRightAction = menu.addAction(tr("Close Tabs to the Right"));
    connect(closeRightAction, &QAction::triggered, this, [this, tabIndex]() {
        closeTabsToRight(tabIndex);
    });

    menu.addSeparator();

    if (isTabPinned(tabIndex))
    {
        QAction* unpinAction = menu.addAction(tr("Unpin Tab"));
        connect(unpinAction, &QAction::triggered, this, [this, tabIndex]() {
            setTabPinned(tabIndex, false);
            updateTabButton(tabIndex);
        });
    }
    else
    {
        QAction* pinAction = menu.addAction(tr("Pin Tab"));
        connect(pinAction, &QAction::triggered, this, [this, tabIndex]() {
            setTabPinned(tabIndex, true);
            updateTabButton(tabIndex);
        });
    }

    emit tabContextMenuRequested(tabIndex, &menu);
    menu.exec(tabBar()->mapToGlobal(pos));
}


void MruTabWidget::updateTabButton(int index)
{
    QTabBar* bar = tabBar();
    if (!bar || index < 0 || index >= bar->count())
        return;

    // Usuwamy stary przycisk
    QWidget* existingButton = bar->tabButton(index, QTabBar::RightSide);
    if (existingButton) {
        existingButton->deleteLater();
    }

    if (isTabPinned(index))
    {
        // Pinned: wstawiamy pinezkę
        QToolButton* pinButton = new QToolButton(bar);
        pinButton->setIcon(QIcon(m_pinIconUri));
        pinButton->setIconSize(QSize(16, 16));
        pinButton->setCursor(Qt::PointingHandCursor);
        pinButton->setToolTip(tr("Unpin Tab"));
        pinButton->setStyleSheet("QToolButton { border: none; padding: 0px; }");

        connect(pinButton, &QToolButton::clicked, this, [this, index]() {
            setTabPinned(index, false);
            updateTabButton(index);
        });

        bar->setTabButton(index, QTabBar::RightSide, pinButton);
    }
    else
    {
        // Nie pinned: wstawiamy przycisk zamykania
        QToolButton* closeButton = new QToolButton(bar);
        closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        closeButton->setIconSize(QSize(16, 16));
        closeButton->setCursor(Qt::ArrowCursor);
        closeButton->setToolTip(tr("Close Tab"));
        closeButton->setStyleSheet("QToolButton { border: none; padding: 0px; }");

        connect(closeButton, &QToolButton::clicked, this, [this, index]() {
            emit tabCloseRequested(index);
        });

        bar->setTabButton(index, QTabBar::RightSide, closeButton);
    }
}

int MruTabWidget::enforceTabLimit()
{
    if (m_tabLimit <= 0) return 0;

    int unpinnedCount = count() - pinnedTabCount();
    if (unpinnedCount <= m_tabLimit) return 0;

    int tabsToRemove = unpinnedCount - m_tabLimit;
    QVector<QWidget*> tabsToClose = findLeastRecentlyUsedUnpinnedTabs(tabsToRemove);

    int removedCount = 0;
    for (QWidget* w : tabsToClose) {
        int index = indexOf(w);
        if (index == -1) continue; // Widget not found, skip

        // Try to close the tab with askPin = true
        if (requestCloseTab(index, true)) {
            // Tab was closed successfully
            removedCount++;
        } else {
            // User cancelled - pin the tab
            setTabPinned(index, true);
        }
    }

    return removedCount;
}


void MruTabWidget::setTabPinned(int tabIndex, bool pinned)
{
    if (tabIndex < 0 || tabIndex >= count()) return;

    // Ensure vector is large enough
    if (tabIndex >= m_pinnedTabs.size()) {
        m_pinnedTabs.resize(count());
    }

    bool wasPinned = m_pinnedTabs[tabIndex];
    m_pinnedTabs[tabIndex] = pinned;

    // Update visual appearance
    updateTabButton(tabIndex);

    // Update MRU list - remove pinned tabs, add unpinned ones
    if (pinned) {
        m_mruOrder.removeAll(tabIndex);
    } else if (!wasPinned && !m_mruOrder.contains(tabIndex)) {
        m_mruOrder.prepend(tabIndex);
    }

    // If unpinning, we might need to enforce the tab limit
    if (wasPinned && !pinned) {
        enforceTabLimit();
    }
}
