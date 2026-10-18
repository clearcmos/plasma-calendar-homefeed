// SPDX-License-Identifier: MIT
#include "homefeedevents.h"

#include <QDesktopServices>

using CalendarEvents::EventData;

HomeFeedEventsPlugin::HomeFeedEventsPlugin(QObject *parent)
    : CalendarEvents::CalendarEventsPlugin(parent)
{
    connect(&m_source, &HomeFeed::FeedSource::settingsChanged, this, [this] {
        forgetPublished();
        m_events.clear();
        m_links.clear();
        m_fetched = false;
    });
    connect(&m_source, &HomeFeed::FeedSource::eventsFetched, this, &HomeFeedEventsPlugin::applyEvents);
}

HomeFeedEventsPlugin::~HomeFeedEventsPlugin() = default;

void HomeFeedEventsPlugin::loadEventsForDateRange(const QDate &startDate, const QDate &endDate)
{
    m_rangeStart = startDate;
    m_rangeEnd = endDate;

    if (!m_fetched) {
        // The answer arrives from applyEvents() once the first fetch lands.
        if (!m_source.isFetching()) {
            m_source.refresh();
        }
        return;
    }

    m_published = HomeFeed::eventsByDay(m_events, m_rangeStart, m_rangeEnd);
    Q_EMIT dataReady(m_published);
}

bool HomeFeedEventsPlugin::showEvent(const QString &uid)
{
    const QUrl url = m_links.value(uid);
    if (!url.isValid() || url.isEmpty()) {
        return false;
    }
    return QDesktopServices::openUrl(url);
}

void HomeFeedEventsPlugin::applyEvents(const QList<HomeFeed::FeedEvent> &events)
{
    const QString defaultColor = m_source.eventColor();
    m_events = events;
    m_links.clear();
    for (HomeFeed::FeedEvent &event : m_events) {
        if (event.data.eventColor().isEmpty() && !defaultColor.isEmpty()) {
            event.data.setEventColor(defaultColor);
        }
        m_links.insert(event.data.uid(), event.url);
    }

    const bool firstFetch = !m_fetched;
    m_fetched = true;

    if (!m_rangeStart.isValid() || !m_rangeEnd.isValid()) {
        return; // Nobody has asked for a range yet.
    }

    const QMultiHash<QDate, EventData> current = HomeFeed::eventsByDay(m_events, m_rangeStart, m_rangeEnd);
    if (firstFetch) {
        m_published = current;
        Q_EMIT dataReady(m_published);
        return;
    }

    // Steady state: emit only what changed so the model never holds duplicates.
    const HomeFeed::PublishDiff diff = HomeFeed::diffPublished(m_published, current);
    for (const QString &uid : diff.removedUids) {
        Q_EMIT eventRemoved(uid);
    }
    m_published = current;
    if (!diff.added.isEmpty()) {
        Q_EMIT dataReady(diff.added);
    }
}

void HomeFeedEventsPlugin::forgetPublished()
{
    const HomeFeed::PublishDiff diff = HomeFeed::diffPublished(m_published, {});
    for (const QString &uid : diff.removedUids) {
        Q_EMIT eventRemoved(uid);
    }
    m_published.clear();
}

#include "moc_homefeedevents.cpp"
