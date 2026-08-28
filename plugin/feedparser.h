// SPDX-License-Identifier: MIT
#pragma once

#include <CalendarEvents/CalendarEventsPlugin>

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QMultiHash>
#include <QString>
#include <QStringList>
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

struct PublishDiff {
    // Emit eventRemoved() for each; sorted, one entry per uid however many
    // days the event spanned.
    QStringList removedUids;
    // Emit dataReady() once with this if it is not empty.
    QMultiHash<QDate, CalendarEvents::EventData> added;
};

// What to tell the clock's model so that `previous` (what it holds from this
// plugin) becomes `current`. A changed event is removed by uid and re-added
// under every day it now touches; eventModified() is deliberately not used
// because it replaces in place and cannot move an event between days.
PublishDiff diffPublished(const QMultiHash<QDate, CalendarEvents::EventData> &previous, const QMultiHash<QDate, CalendarEvents::EventData> &current);

}
