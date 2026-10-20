// SPDX-License-Identifier: MIT
#include "nextevent.h"

#include <QDateTime>
#include <QLocale>
#include <QTime>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace
{

// Enough to cover the coming days without turning the popup into a calendar.
constexpr qsizetype kUpcomingRows = 20;

QVariantList departureRows(const HomeFeed::FeedEvent &event, const QDateTime &now, const QLocale &locale)
{
    QVariantList rows;
    const QList<HomeFeed::Departure> departures = HomeFeed::pendingDepartures(event, now);
    for (const HomeFeed::Departure &departure : departures) {
        rows.append(QVariantMap{
            {u"text"_s, HomeFeed::departureLabel(departure, locale)},
            {u"detail"_s, departure.detail},
            {u"late"_s, departure.leave <= now},
        });
    }
    return rows;
}

}

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

const QVariantList &NextEvent::departures() const
{
    return m_departures;
}

const QVariantList &NextEvent::upcoming() const
{
    return m_upcoming;
}

void NextEvent::update()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDate today = now.date();
    const QLocale locale;
    const QList<HomeFeed::FeedEvent> ahead = HomeFeed::upcomingEvents(m_events, now, kUpcomingRows);

    QVariantList rows;
    QDate lastDay;
    for (const HomeFeed::FeedEvent &event : ahead) {
        const QDate day = HomeFeed::listDay(event.data, now);
        QStringList lines;
        const QString until = HomeFeed::untilLabel(event.data, now, locale);
        if (!until.isEmpty()) {
            lines.append(until);
        }
        if (!event.data.location().isEmpty()) {
            lines.append(event.data.location());
        }
        rows.append(QVariantMap{
            {u"heading"_s, day != lastDay ? HomeFeed::dayHeading(day, today, locale) : QString()},
            {u"time"_s, HomeFeed::rowTimeLabel(event.data, now, locale)},
            {u"title"_s, event.data.title()},
            {u"lines"_s, lines},
            {u"departures"_s, departureRows(event, now, locale)},
        });
        lastDay = day;
    }

    const bool newValid = !ahead.isEmpty();
    const QString newTitle = newValid ? ahead.first().data.title() : QString();
    const QString newWhen = newValid ? HomeFeed::whenLabel(ahead.first().data, now, locale) : QString();
    const QString newLocation = newValid ? ahead.first().data.location() : QString();
    const QVariantList newDepartures = newValid ? departureRows(ahead.first(), now, locale) : QVariantList();
    if (newValid == m_valid && newTitle == m_title && newWhen == m_when && newLocation == m_location && newDepartures == m_departures && rows == m_upcoming) {
        return;
    }
    m_valid = newValid;
    m_title = newTitle;
    m_when = newWhen;
    m_location = newLocation;
    m_departures = newDepartures;
    m_upcoming = rows;
    Q_EMIT changed();
}

void NextEvent::scheduleTick()
{
    // A little past the boundary, so the clock has already turned over.
    const int intoMinute = QTime::currentTime().msecsSinceStartOfDay() % 60'000;
    m_tick.start(60'000 - intoMinute + 200);
}

#include "moc_nextevent.cpp"
