#pragma once

#include "FileOperations.h"
#include "SearchIgnore.h"

#include <QDialog>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <memory>

class GitignoreOptionsWidget;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QThread;

/**
 * @class CopyDialog
 * @brief Asks for the destination of a copy (F5) or move (F6)
 *
 * For a copy of a directory or several entries it offers to skip what
 * .gitignore files ignore. "Count" sums the files and their size that would
 * be copied (honouring that checkbox) in a background thread; the copy then
 * uses that count instead of counting again. OK pressed while counting waits
 * for the count to finish.
 */
class CopyDialog : public QDialog {
    Q_OBJECT
public:
    /// offerGitignore: show the .gitignore checkbox. offerCount: show "Count".
    CopyDialog(const QString& title, const QString& label, const QString& destination,
               const QString& srcDir, const QStringList& names,
               bool offerGitignore, bool offerCount, QWidget* parent = nullptr);
    ~CopyDialog() override;

    QString destination() const;
    /// Checkbox state; false when it is not offered.
    bool useGitignore() const;
    void setUseGitignore(bool use);
    /// The .gitignore options shown (the configured ones when not offered).
    GitignoreOptions gitignoreOptions() const;
    /// Writes the shown .gitignore options to Config (without saving).
    void storeGitignoreOptions() const;
    /// The finished count (not stopped), for the current checkbox state.
    std::optional<FileOperations::CopyTotals> totals() const;

    void accept() override;

private:
    struct CountState {
        std::atomic<quint64> files{0};
        std::atomic<quint64> bytes{0};
        std::atomic<bool> cancel{false};
    };

    void startCounting();
    void stopCounting();
    void showCount(bool finished);

    QString m_srcDir;
    QStringList m_names;
    QLineEdit* m_destEdit = nullptr;
    QCheckBox* m_gitignoreCheck = nullptr;
    GitignoreOptionsWidget* m_gitignoreOptions = nullptr;
    QPushButton* m_countButton = nullptr;
    QPushButton* m_okButton = nullptr;
    QLabel* m_countLabel = nullptr;
    QThread* m_countThread = nullptr;
    std::shared_ptr<CountState> m_count;
    bool m_countComplete = false;       ///< m_count holds a finished count
    bool m_countedWithGitignore = false;
    GitignoreOptions m_countedOptions;
    bool m_acceptWhenCounted = false;   ///< OK pressed while counting
    QTimer m_countTimer;  ///< refreshes the label while counting
};
