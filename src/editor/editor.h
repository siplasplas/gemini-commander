#ifndef EDITOR_H
#define EDITOR_H

#include <QWidget>
#include <QString>
#include <KTextEditor/Editor>       // The Editor singleton
#include <KTextEditor/Document>     // Document representation
#include <KTextEditor/View>         // Base view class (used for log)
#include <KTextEditor/MainWindow>   // Often needed for full integration/context
#include <KTextEditor/Application>

#include "BaseViewer.h"

/**
 * @class Editor
 * @brief Widget wrapper for KTextEditor components
 *
 * Manages text editing sessions with document/view architecture
 */
class Editor : public BaseViewer {
    Q_OBJECT

signals:
    void configFileSaved();

public:
    /**
     * @brief Constructs editor with KTextEditor document
     * @param doc KTextEditor document to manage
     * @param parent Parent widget
     */
    explicit Editor(KTextEditor::Document *doc, QWidget *parent = nullptr);

    /**
     * @brief Gets managed document
     * @return Pointer to KTextEditor document
     */
    KTextEditor::Document* document() const;

    /**
     * @brief Gets editor view component
     * @return Pointer to KTextEditor view widget
     */
    KTextEditor::View* view() const;

    /**
     * @brief Gets filename without path
     * @return Base filename or "Untitled" for new documents
     */
    [[nodiscard]] QString baseFileName() const override;

    /**
     * @brief Checks document modification state
     * @return true if document has unsaved changes
     */
    [[nodiscard]] bool isModified() const override;

    /**
     * @brief Saves document to disk
     * @return true on successful save
     * @note Shows error message on failure
     */
    bool saveFile();

private:
    KTextEditor::Document *m_document; ///< Managed text document
    KTextEditor::View *m_view;         ///< Document visualization component
};

#endif // EDITOR_H