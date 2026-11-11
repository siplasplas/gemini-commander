#ifndef VIEWERFINDTARGET_H
#define VIEWERFINDTARGET_H

#include "siplasplas/plugin/FindPlugin.h"

#include <QObject>

namespace wid {
class TextViewer;
}

// The large-file viewer as the target of the find bar plugin: huge (it seeks
// itself, in the background, and marks the matches it shows) and read-only.
class ViewerFindTarget : public QObject, public spp::FindTarget
{
public:
    explicit ViewerFindTarget(wid::TextViewer* viewer);

    QWidget* widget() override;
    bool canReplace() const override { return false; }
    bool isHuge() const override { return true; }
    QString selectedText() const override;

    void setQuery(const spp::FindQuery& query) override;
    void find(const spp::FindQuery& query, bool forward) override;
    void cancelFind() override;
    void setFindListener(spp::FindTargetListener* listener) override { m_listener = listener; }

private:
    wid::TextViewer* m_viewer;
    spp::FindTargetListener* m_listener = nullptr;
};

#endif // VIEWERFINDTARGET_H
