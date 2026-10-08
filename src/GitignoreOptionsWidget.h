#pragma once

#include "SearchIgnore.h"

#include <QWidget>

class QCheckBox;
class QComboBox;

/**
 * @class GitignoreOptionsWidget
 * @brief The .gitignore options (where they start counting, hide .git)
 *
 * The same widget in Show > Custom, the copy dialog and Find Files: all
 * three show and set the one Config::gitignoreOptions(). Starts with the
 * configured options; storeInConfig() writes them back (without saving)
 * and shows them in the other open instances too (e.g. a Find Files dialog
 * left open).
 */
class GitignoreOptionsWidget : public QWidget {
    Q_OBJECT
public:
    explicit GitignoreOptionsWidget(QWidget* parent = nullptr);
    ~GitignoreOptionsWidget() override;

    GitignoreOptions options() const;
    void setOptions(const GitignoreOptions& options);
    void storeInConfig() const;

signals:
    void changed();

private:
    static QList<GitignoreOptionsWidget*> s_instances;

    QComboBox* m_topCombo = nullptr;
    QCheckBox* m_skipGitCheck = nullptr;
};
