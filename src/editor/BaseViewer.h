#ifndef BASEVIEWER_H
#define BASEVIEWER_H
#include <QWidget>

class BaseViewer : public QWidget {
public:
    BaseViewer(QWidget *parent = nullptr);
    ~BaseViewer() override;
    /**
     * @brief Gets full file path
     * @return Document path as QString
     */
    QString filePath() const;

    /**
     * @brief Gets filename without path
     * @return Base filename or "Untitled" for new documents
     */
    [[nodiscard]] virtual QString baseFileName() const;

    /**
     * @brief Checks document modification state
     * @return true if document has unsaved changes
     */
    [[nodiscard]] virtual bool isModified() const;

protected:
    QString m_filePath;                 ///< Full document filesystem path
};

#endif //BASEVIEWER_H
