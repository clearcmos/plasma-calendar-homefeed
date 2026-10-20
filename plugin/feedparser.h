// SPDX-License-Identifier: MIT
#pragma once

#include <CalendarEvents/CalendarEventsPlugin>

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QList>
#include <QLocale>
#include <QMultiHash>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>

namespace HomeFeed
{

// When to set off for an event by one means of travel, as the feed's
// producer computed it: "mode" names the means ("Bus"), "detail" says more
// ("40 min"), "fare" is a short price ("~$24").
struct Departure {
    QString mode;
    QDateTime leave;
    QString detail;
    QString fare;
};

struct FeedEvent {
    CalendarEvents::EventData data;
    QUrl url;
    QList<Departure> departures;
};

struct ParseResult {
    QList<FeedEvent> events;
    // Free-text notices the producer wants shown beside the events (for
    // example transit stop closures), in display order.
    QStringList notices;
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
// convention), which is folded into an inclusive last day here. A departure
// without a mode or a valid leave time is dropped.
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

// The event to show as "next" at `now`. A timed event counts until it ends, so
// a meeting that has started stays shown. An all-day event counts only until
// its first day begins, so on that day it does not hide the timed events.
// The earliest start wins; ties go to the earlier end, then the uid.
std::optional<FeedEvent> nextEvent(const QList<FeedEvent> &events, const QDateTime &now);

// The first `limit` events that still count at `now`, in nextEvent() order.
QList<FeedEvent> upcomingEvents(const QList<FeedEvent> &events, const QDateTime &now, qsizetype limit);

// When `event` happens relative to `now`, for a one-line label: "now, until
// 1:30 PM" while it runs, else a day ("Tomorrow", a weekday within the week,
// then the date; nothing for today) and the start time, or "all day".
QString whenLabel(const CalendarEvents::EventData &event, const QDateTime &now, const QLocale &locale);

// "until 1:30 PM" (with a day when it ends on another) for a timed event
// that is running at `now`; empty otherwise.
QString untilLabel(const CalendarEvents::EventData &event, const QDateTime &now, const QLocale &locale);

// A list row's time once its day is already shown: "now" while it runs, "all
// day", or the start time.
QString rowTimeLabel(const CalendarEvents::EventData &event, const QDateTime &now, const QLocale &locale);

// Heading for a day in a list: "Today", "Tomorrow", else "Thu Oct 22".
QString dayHeading(const QDate &day, const QDate &today, const QLocale &locale);

// The day a list files `event` under: its start day, or today for an event
// that began earlier and is still running.
QDate listDay(const CalendarEvents::EventData &event, const QDateTime &now);

// Departures still worth showing: all of them until the event starts, none
// after, since by then it is too late to set off.
QList<Departure> pendingDepartures(const FeedEvent &event, const QDateTime &now);

// "Bus 12:05 PM", for the panel line.
QString departureLabel(const Departure &departure, const QLocale &locale);

// The popup's line: the label, then the fare and the detail when present
// ("Car 12:30 PM ~$24 · 15 min drive").
QString departureSummary(const Departure &departure, const QLocale &locale);

// The program and arguments that hand one departure to the rc file's
// SendCommand: the command split the way a shell would, then the departure's
// mode and the event's location as two more arguments, run without a shell.
// Empty when the command or the location is blank.
QStringList sendCommandLine(const QString &command, const QString &mode, const QString &location);

}
