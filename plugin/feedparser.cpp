// SPDX-License-Identifier: MIT
#include "feedparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

using namespace Qt::StringLiterals;
using CalendarEvents::EventData;

namespace
{

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

        result.events.append({data, QUrl(stringOrEmpty(object, "url"_L1))});
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

bool sameEvent(const EventData &a, const EventData &b)
{
    return a.uid() == b.uid() && a.title() == b.title() && a.description() == b.description() && a.location() == b.location()
        && a.eventColor() == b.eventColor() && a.startDateTime() == b.startDateTime() && a.endDateTime() == b.endDateTime() && a.isAllDay() == b.isAllDay();
}

}
