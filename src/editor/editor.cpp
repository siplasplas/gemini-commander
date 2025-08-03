#include "editor.h"
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <QFont>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>

/**
 * @brief Constructs editor with document
 * @param doc KTextEditor document to display
 * @param parent Parent widget
 *
 * @note Creates view and sets up editor layout automatically
 */
Editor::Editor(KTextEditor::Document *doc, QWidget *parent) :
    BaseViewer(parent),
    m_document(doc),
    m_view(nullptr)
{
    if (!m_document) {
        qWarning() << "Editor created with null document!";
        // Handle error? Maybe create a placeholder? For now, just proceed carefully.
        m_filePath = ""; // Ensure path is empty
        return; // Cannot create view without document
    }

    // Store the file path from the document
    m_filePath = m_document->url().toLocalFile();

    // Create the KTextEditor View, parented to this Editor widget
    m_view = m_document->createView(this);
    if (!m_view) {
        qWarning() << "Failed to create KTextEditor::View for document:" << m_filePath;
        // Handle error?
        return; // Cannot proceed without a view
    }

    // --- Configure the view (Basic settings) ---
    // Attempt basic configuration directly on the view
    QFont editorFont("Monospace");
    editorFont.setStyleHint(QFont::TypeWriter);
    m_view->setFont(editorFont); // Set font directly on the view

    // TODO: Explore KConfig for more persistent/advanced KTextEditor view settings in KF6

    // --- Set up layout ---
    // Create a layout for the Editor widget
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); // No margins
    // Add the KTextEditor::View to the layout, making it fill the Editor widget
    layout->addWidget(m_view);
    this->setLayout(layout);

    // Set object name for debugging/identification
    setObjectName(m_filePath);
}

/**
 * @brief Provides access to managed document
 * @return KTextEditor document pointer
 */
KTextEditor::Document* Editor::document() const {
    return m_document;
}

/**
 * @brief Provides access to editor view
 * @return KTextEditor view widget pointer
 */
KTextEditor::View* Editor::view() const {
    return m_view;
}


/**
 * @brief Extracts filename from path
 * @return Short filename or "Untitled"
 */
QString Editor::baseFileName() const {
    if (m_filePath.isEmpty()) {
        // Try getting from document URL if path wasn't set/valid
        if(m_document && !m_document->url().isEmpty()){
             return QFileInfo(m_document->url().path()).fileName();
        }
        return tr("Untitled");
    }
    return QFileInfo(m_filePath).fileName();
}

/**
 * @brief Checks document modification state
 * @return true if document contains unsaved changes
 */
bool Editor::isModified() const {
    if (m_document) {
        return m_document->isModified();
    }
    return false;
}

/**
 * @brief Saves document to original path
 * @return true on successful save
 *
 * @details Uses KTextEditor's native save functionality
 * @warning Shows QMessageBox on failure
 */
bool Editor::saveFile() {
    if (!m_document) {
        qWarning() << "saveFile called on Editor with null document.";
        return false;
    }
    // Use the document's save method
    if (!m_document->save()) {
        qWarning() << "Failed to save document:" << m_filePath;
        QMessageBox::warning(this->parentWidget(), // Show error relative to parent window
                             tr("Save Error"),
                             tr("Failed to save file:\n%1").arg(m_filePath));
        return false;
    }
    qDebug() << "Document saved successfully:" << m_filePath;
    // isModified should be automatically set to false by KTextEditor::Document on successful save
    return true;
}