#ifndef EDITOR_H
#define EDITOR_H

#include <QWidget>
#include <QString>
#include <KTextEditor/Editor>       // The Editor singleton
#include <KTextEditor/Document>     // Document representation
#include <KTextEditor/View>         // Base view class (used for log)
#include <KTextEditor/MainWindow>   // Often needed for full integration/context
#include <KTextEditor/Application>

/**
 * @class Editor
 * @brief Widget wrapper for KTextEditor components
 *
 * Manages text editing sessions with document/view architecture
 */
class Editor : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs editor with KTextEditor document
     * @param doc KTextEditor document to manage
     * @param parent Parent widget
     */
    explicit Editor(KTextEditor::Document *doc, QWidget *parent = nullptr);

    /**
     * @brief Destroys editor instance
     * @note Automatically cleans up view through Qt's parent-child system
     */
    ~Editor();

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
     * @brief Gets full file path
     * @return Document path as QString
     */
    QString filePath() const;

    /**
     * @brief Gets filename without path
     * @return Base filename or "Untitled" for new documents
     */
    QString baseFileName() const;

    /**
     * @brief Checks document modification state
     * @return true if document has unsaved changes
     */
    bool isModified() const;

    /**
     * @brief Saves document to disk
     * @return true on successful save
     * @note Shows error message on failure
     */
    bool saveFile();

private:
    QString m_filePath;                 ///< Full document filesystem path
    KTextEditor::Document *m_document; ///< Managed text document
    KTextEditor::View *m_view;         ///< Document visualization component
};

#endif // EDITOR_H