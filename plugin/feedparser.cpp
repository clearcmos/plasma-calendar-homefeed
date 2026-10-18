// SPDX-License-Identifier: MIT
#include "feedparser.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

using namespace Qt::StringLiterals;
using CalendarEvents::EventData;

namespace
{

QHash<QString, EventData> byUid(const QMultiHash<QDate, EventData> &byDay)
{
    QHash<QString, EventData> result;
    for (auto it = byDay.cbegin(); it != byDay.cend(); ++it) {
        result.insert(it->uid(), *it);
    }
    return result;
}

QString stringOrEmpty(const QJsonObject &object, QLatin1StringView key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

bool parseAllDay(const QJsonObject &object, EventData &event)
{
    const QDate start = QDate::fromString(stringOrEmpty(object, "start"_L1), Qt::ISODate);
    if (!start.isValid()) {
        return false;
    }
    // "end" is the exclusive next day; a missing or nonsensical end means one day.
    QDate lastDay = QDate::fromString(stringOrEmpty(object, "end"_L1), Qt::ISODate).addDays(-1);
    if (!lastDay.isValid() || lastDay < start) {
        lastDay = start;
    }
    event.setStartDateTime(start.startOfDay());
    event.setEndDateTime(lastDay.endOfDay());
    event.setIsAllDay(true);
    return true;
}

// Whether `event` is still a candidate for "next" at `now`; see nextEvent().
bool stillAhead(const EventData &event, const QDateTime &now)
{
    return event.isAllDay() ? event.startDateTime() > now : event.endDateTime() > now;
}

bool comesFirst(const EventData &a, const EventData &b)
{
    if (a.startDateTime() != b.startDateTime()) {
        return a.startDateTime() < b.startDateTime();
    }
    if (a.endDateTime() != b.endDateTime()) {
        return a.endDateTime() < b.endDateTime();
    }
    return a.uid() < b.uid();
}

QString dayLabel(const QDate &day, const QDate &today, const QLocale &locale)
{
    const qint64 ahead = today.daysTo(day);
    if (ahead == 0) {
        return {};
    }
    if (ahead == 1) {
        return u"Tomorrow"_s;
    }
    if (ahead > 1 && ahead < 7) {
        return locale.dayName(day.dayOfWeek(), QLocale::ShortFormat);
    }
    return locale.toString(day, u"MMM d"_s);
}

QString dayAndTime(const QDateTime &moment, const QDate &today, const QLocale &locale)
{
    const QString time = locale.toString(moment.time(), QLocale::ShortFormat);
    const QString day = dayLabel(moment.date(), today, locale);
    return day.isEmpty() ? time : day + u' ' + time;
}

QList<HomeFeed::Departure> parseDepartures(const QJsonValue &value)
{
    QList<HomeFeed::Departure> departures;
    const QJsonArray entries = value.toArray();
    for (const QJsonValue &entry : entries) {
        const QJsonObject object = entry.toObject();
        HomeFeed::Departure departure{stringOrEmpty(object, "mode"_L1),
                                      QDateTime::fromString(stringOrEmpty(object, "leave"_L1), Qt::ISODateWithMs),
                                      stringOrEmpty(object, "detail"_L1)};
        if (!departure.mode.isEmpty() && departure.leave.isValid()) {
            departures.append(departure);
        }
    }
    return departures;
}

bool parseTimed(const QJsonObject &object, EventData &event)
{
    const QDateTime start = QDateTime::fromString(stringOrEmpty(object, "start"_L1), Qt::ISODateWithMs);
    if (!start.isValid()) {
        return false;
    }
    QDateTime end = QDateTime::fromString(stringOrEmpty(object, "end"_L1), Qt::ISODateWithMs);
    if (!end.isValid() || end < start) {
        end = start;
    }
    event.setStartDateTime(start);
    event.setEndDateTime(end);
    event.setIsAllDay(false);
    return true;
}

}

namespace HomeFeed
{

ParseResult parseFeed(const QByteArray &document)
{
    ParseResult result;

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(document, &parseError);
    if (json.isNull()) {
        result.error = u"invalid JSON: "_s + parseError.errorString();
        return result;
    }
    if (!json.isObject()) {
        result.error = u"top-level value is not an object"_s;
        return result;
    }

    const QJsonObject root = json.object();
    const QJsonValue eventsValue = root.value("events"_L1);
    if (!eventsValue.isArray()) {
        result.error = u"missing \"events\" array"_s;
        return result;
    }

    const QJsonObject window = root.value("window"_L1).toObject();
    result.windowStart = QDate::fromString(stringOrEmpty(window, "start"_L1), Qt::ISODate);
    result.windowEnd = QDate::fromString(stringOrEmpty(window, "end"_L1), Qt::ISODate);

    const QJsonArray events = eventsValue.toArray();
    result.events.reserve(events.size());
    for (const QJsonValue &value : events) {
        if (!value.isObject()) {
            ++result.skipped;
            continue;
        }
        const QJsonObject object = value.toObject();
        const QString uid = stringOrEmpty(object, "uid"_L1);
        if (uid.isEmpty()) {
            ++result.skipped;
            continue;
        }

        EventData data;
        const bool parsed = object.value("all_day"_L1).toBool() ? parseAllDay(object, data) : parseTimed(object, data);
        if (!parsed) {
            ++result.skipped;
            continue;
        }
        data.setUid(uid);
        data.setTitle(stringOrEmpty(object, "title"_L1));
        data.setDescription(stringOrEmpty(object, "description"_L1));
        data.setLocation(stringOrEmpty(object, "location"_L1));
        data.setEventColor(stringOrEmpty(object, "color"_L1));
        data.setEventType(EventData::Event);
        data.setIsMinor(false);

        result.events.append({data, QUrl(stringOrEmpty(object, "url"_L1)), parseDepartures(object.value("departures"_L1))});
    }
    return result;
}

QList<QDate> daysSpanned(const EventData &event)
{
    const QDateTime start = event.startDateTime().toLocalTime();
    const QDateTime end = event.endDateTime().toLocalTime();

    const QDate first = start.date();
    QDate last = first;
    if (end > start) {
        // Step back one millisecond so an end of exactly 00:00 stays on the
        // previous day instead of claiming the next one.
        last = end.addMSecs(-1).date();
        if (last < first) {
            last = first;
        }
    }

    QList<QDate> days;
    for (QDate day = first; day <= last; day = day.addDays(1)) {
        days.append(day);
    }
    return days;
}

QMultiHash<QDate, EventData> eventsByDay(const QList<FeedEvent> &events, const QDate &start, const QDate &end)
{
    QMultiHash<QDate, EventData> byDay;
    for (const FeedEvent &event : events) {
        const QList<QDate> days = daysSpanned(event.data);
        for (const QDate &day : days) {
            if (day >= start && day <= end) {
                byDay.insert(day, event.data);
            }
        }
    }
    return byDay;
}

PublishDiff diffPublished(const QMultiHash<QDate, EventData> &previous, const QMultiHash<QDate, EventData> &current)
{
    const QHash<QString, EventData> previousByUid = byUid(previous);
    const QHash<QString, EventData> currentByUid = byUid(current);

    PublishDiff diff;
    QSet<QString> readd;
    for (auto it = previousByUid.cbegin(); it != previousByUid.cend(); ++it) {
        const auto found = currentByUid.constFind(it.key());
        if (found == currentByUid.cend()) {
            diff.removedUids.append(it.key());
        } else if (!sameEvent(*it, *found)) {
            diff.removedUids.append(it.key());
            readd.insert(it.key());
        }
    }
    for (auto it = current.cbegin(); it != current.cend(); ++it) {
        if (!previousByUid.contains(it->uid()) || readd.contains(it->uid())) {
            diff.added.insert(it.key(), *it);
        }
    }
    diff.removedUids.sort();
    return diff;
}

std::optional<FeedEvent> nextEvent(const QList<FeedEvent> &events, const QDateTime &now)
{
    const QList<FeedEvent> next = upcomingEvents(events, now, 1);
    if (next.isEmpty()) {
        return std::nullopt;
    }
    return next.first();
}

QList<FeedEvent> upcomingEvents(const QList<FeedEvent> &events, const QDateTime &now, qsizetype limit)
{
    QList<FeedEvent> ahead;
    for (const FeedEvent &event : events) {
        if (stillAhead(event.data, now)) {
            ahead.append(event);
        }
    }
    std::sort(ahead.begin(), ahead.end(), [](const FeedEvent &a, const FeedEvent &b) {
        return comesFirst(a.data, b.data);
    });
    if (ahead.size() > limit) {
        ahead.resize(limit);
    }
    return ahead;
}

QString whenLabel(const EventData &event, const QDateTime &now, const QLocale &locale)
{
    const QDate today = now.toLocalTime().date();
    if (event.isAllDay()) {
        const QString day = dayLabel(event.startDateTime().date(), today, locale);
        return day.isEmpty() ? u"all day"_s : day + u", all day"_s;
    }
    const QDateTime start = event.startDateTime().toLocalTime();
    if (start <= now) {
        return u"now, "_s + untilLabel(event, now, locale);
    }
    return dayAndTime(start, today, locale);
}

QString untilLabel(const EventData &event, const QDateTime &now, const QLocale &locale)
{
    if (event.isAllDay() || event.startDateTime() > now) {
        return {};
    }
    return u"until "_s + dayAndTime(event.endDateTime().toLocalTime(), now.toLocalTime().date(), locale);
}

QString rowTimeLabel(const EventData &event, const QDateTime &now, const QLocale &locale)
{
    if (event.isAllDay()) {
        return u"all day"_s;
    }
    const QDateTime start = event.startDateTime().toLocalTime();
    if (start <= now) {
        return u"now"_s;
    }
    return locale.toString(start.time(), QLocale::ShortFormat);
}

QString dayHeading(const QDate &day, const QDate &today, const QLocale &locale)
{
    const qint64 ahead = today.daysTo(day);
    if (ahead == 0) {
        return u"Today"_s;
    }
    if (ahead == 1) {
        return u"Tomorrow"_s;
    }
    return locale.toString(day, u"ddd MMM d"_s);
}

QDate listDay(const EventData &event, const QDateTime &now)
{
    return qMax(event.startDateTime().toLocalTime().date(), now.toLocalTime().date());
}

QList<Departure> pendingDepartures(const FeedEvent &event, const QDateTime &now)
{
    if (event.data.isAllDay() || event.data.startDateTime() <= now) {
        return {};
    }
    return event.departures;
}

QString departureLabel(const Departure &departure, const QDateTime &now, const QLocale &locale)
{
    const QDateTime leave = departure.leave.toLocalTime();
    const QString when = leave <= now ? u"now"_s : locale.toString(leave.time(), QLocale::ShortFormat);
    return departure.mode + u" leave "_s + when;
}

bool sameEvent(const EventData &a, const EventData &b)
{
    return a.uid() == b.uid() && a.title() == b.title() && a.description() == b.description() && a.location() == b.location()
        && a.eventColor() == b.eventColor() && a.startDateTime() == b.startDateTime() && a.endDateTime() == b.endDateTime() && a.isAllDay() == b.isAllDay();
}

}
