#include "ViewerFindTarget.h"

#include "wid/TextViewer.h"

namespace {

vl::SearchOptions optionsOf(const spp::FindQuery& query)
{
    vl::SearchOptions options;
    options.caseInsensitive = !query.matchCase;
    options.wholeWord = query.wholeWords;
    options.regex = query.regex;
    return options;
}

} // namespace

ViewerFindTarget::ViewerFindTarget(wid::TextViewer* viewer) : m_viewer(viewer)
{
    connect(m_viewer, &wid::TextViewer::findProgress, this, [this](qint64 done, qint64 total) {
        if (m_listener) m_listener->findProgress(done, total);
    });
    connect(m_viewer, &wid::TextViewer::findFinished, this, [this](wid::TextViewer::FindOutcome outcome) {
        if (!m_listener) return;
        switch (outcome) {
        case wid::TextViewer::FindOutcome::Found: m_listener->findFinished(spp::FindResult::Found); break;
        case wid::TextViewer::FindOutcome::Wrapped: m_listener->findFinished(spp::FindResult::Wrapped); break;
        case wid::TextViewer::FindOutcome::NotFound: m_listener->findFinished(spp::FindResult::NotFound); break;
        case wid::TextViewer::FindOutcome::Cancelled: m_listener->findFinished(spp::FindResult::Cancelled); break;
        }
    });
}

QWidget* ViewerFindTarget::widget()
{
    return m_viewer;
}

QString ViewerFindTarget::selectedText() const
{
    // The selected text is sought if it is one short line.
    const QByteArray selected = m_viewer->selectedBytes(1024);
    if (selected.isEmpty() || selected.contains('\n') || selected.contains('\r'))
        return {};
    return QString::fromUtf8(selected);
}

void ViewerFindTarget::setQuery(const spp::FindQuery& query)
{
    if (query.text.isEmpty())
        m_viewer->clearSearchMarks();
    else
        m_viewer->setSearchMarks(query.text.toUtf8().toStdString(), optionsOf(query));
}

void ViewerFindTarget::find(const spp::FindQuery& query, bool forward)
{
    m_viewer->find(query.text.toUtf8().toStdString(), optionsOf(query), !forward, query.wrap);
}

void ViewerFindTarget::cancelFind()
{
    m_viewer->cancelFind();
}
