// SPDX-License-Identifier: MIT
#include "nextevent.h"

#include <QDateTime>
#include <QLocale>
#include <QTime>

NextEvent::NextEvent(QObject *parent)
    : QObject(parent)
{
    m_tick.setSingleShot(true);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        update();
        scheduleTick();
    });

    connect(&m_source, &HomeFeed::FeedSource::settingsChanged, this, [this] {
        m_events.clear();
        update();
    });
    connect(&m_source, &HomeFeed::FeedSource::eventsFetched, this, [this](const QList<HomeFeed::FeedEvent> &events) {
        m_events = events;
        update();
    });

    scheduleTick();
}

bool NextEvent::isValid() const
{
    return m_valid;
}

const QString &NextEvent::title() const
{
    return m_title;
}

const QString &NextEvent::when() const
{
    return m_when;
}

const QString &NextEvent::location() const
{
    return m_location;
}

void NextEvent::update()
{
    const QDateTime now = QDateTime::currentDateTime();
    const std::optional<HomeFeed::FeedEvent> next = HomeFeed::nextEvent(m_events, now);

    const bool newValid = next.has_value();
    const QString newTitle = newValid ? next->data.title() : QString();
    const QString newWhen = newValid ? HomeFeed::whenLabel(next->data, now, QLocale()) : QString();
    const QString newLocation = newValid ? next->data.location() : QString();
    if (newValid == m_valid && newTitle == m_title && newWhen == m_when && newLocation == m_location) {
        return;
    }
    m_valid = newValid;
    m_title = newTitle;
    m_when = newWhen;
    m_location = newLocation;
    Q_EMIT changed();
}

void NextEvent::scheduleTick()
{
    // A little past the boundary, so the clock has already turned over.
    const int intoMinute = QTime::currentTime().msecsSinceStartOfDay() % 60'000;
    m_tick.start(60'000 - intoMinute + 200);
}

#include "moc_nextevent.cpp"
