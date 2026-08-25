// SPDX-License-Identifier: MIT
#pragma once

#include <CalendarEvents/CalendarEventsPlugin>

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QMultiHash>
#include <QString>
#include <QUrl>

namespace HomeFeed
{

struct FeedEvent {
    CalendarEvents::EventData data;
    QUrl url;
};

struct ParseResult {
    QList<FeedEvent> events;
    QDate windowStart;
    QDate windowEnd;
    int skipped = 0;
    QString error;

    bool ok() const
    {
        return error.isEmpty();
    }
};

// Parses a feed document. Timed events carry RFC 3339 timestamps; all-day
// events carry bare dates with an exclusive end (Google and iCalendar
// convention), which is folded into an inclusive last day here.
ParseResult parseFeed(const QByteArray &document);

// Inclusive local-time days an event occupies. An end at exactly midnight
// belongs to the previous day and does not add a new one.
QList<QDate> daysSpanned(const CalendarEvents::EventData &event);

// Events keyed by every day they touch, restricted to [start, end] inclusive.
// This is the shape CalendarEventsPlugin::dataReady expects: the DaysModel
// looks events up per day and never expands multi-day spans itself.
QMultiHash<QDate, CalendarEvents::EventData> eventsByDay(const QList<FeedEvent> &events, const QDate &start, const QDate &end);

// Field-wise comparison used to detect changed events between refreshes.
bool sameEvent(const CalendarEvents::EventData &a, const CalendarEvents::EventData &b);

}
