#include "BaseViewer.h"

#include <QFileInfo>

BaseViewer::BaseViewer(QWidget *parent) : QWidget(parent) {
}

/**
 * @brief Destroys editor instance
 * @note Logs destruction and relies on Qt's parent-child cleanup
 */
BaseViewer::~BaseViewer() {
    // The m_view widget will be deleted by Qt's parent/child mechanism
    // as it's parented to 'this' (the Editor QWidget).
    // The m_document is managed by KTextEditor::Editor singleton, do not delete here.
}

/**
 * @brief Returns document filesystem path
 * @return Full file path as QString
 */
QString BaseViewer::filePath() const {
    return m_filePath;
}

/**
 * @brief Extracts filename from path
 * @return Short filename or "Untitled"
 */
QString BaseViewer::baseFileName() const {
    return QFileInfo(m_filePath).fileName();
}


/**
 * @brief Checks document modification state
 * @return true if document contains unsaved changes
 */
bool BaseViewer::isModified() const {
    return false;
}
