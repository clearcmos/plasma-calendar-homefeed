// SPDX-License-Identifier: MIT
#include "feedparser.h"

#include <QFile>
#include <QLocale>
#include <QSet>
#include <QTest>
#include <QTimeZone>

#include <algorithm>

using namespace Qt::StringLiterals;
using CalendarEvents::EventData;
using HomeFeed::FeedEvent;
using HomeFeed::ParseResult;

namespace
{

QByteArray fixture(const QString &name)
{
    QFile file(QStringLiteral(FIXTURE_DIR) + u'/' + name);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("cannot open fixture %s", qPrintable(file.fileName()));
    }
    return file.readAll();
}

const FeedEvent *find(const ParseResult &result, const QString &uid)
{
    const auto it = std::find_if(result.events.cbegin(), result.events.cend(), [&uid](const FeedEvent &event) {
        return event.data.uid() == uid;
    });
    return it == result.events.cend() ? nullptr : &*it;
}

FeedEvent timed(const QString &uid, const QDateTime &start, const QDateTime &end)
{
    EventData data;
    data.setUid(uid);
    data.setTitle(uid);
    data.setStartDateTime(start);
    data.setEndDateTime(end);
    data.setIsAllDay(false);
    return {data, QUrl(), {}};
}

FeedEvent allDay(const QString &uid, const QDate &first, const QDate &last)
{
    EventData data;
    data.setUid(uid);
    data.setTitle(uid);
    data.setStartDateTime(first.startOfDay());
    data.setEndDateTime(last.endOfDay());
    data.setIsAllDay(true);
    return {data, QUrl(), {}};
}

QDateTime at(int day, int hour, int minute = 0)
{
    return QDateTime(QDate(2026, 10, day), QTime(hour, minute), QTimeZone::UTC);
}

QString nextUid(const QList<FeedEvent> &events, const QDateTime &now)
{
    const std::optional<FeedEvent> next = HomeFeed::nextEvent(events, now);
    return next ? next->data.uid() : u"none"_s;
}

QList<QDate> days(const ParseResult &result, const QString &uid)
{
    const FeedEvent *event = find(result, uid);
    if (!event) {
        return {};
    }
    return HomeFeed::daysSpanned(event->data);
}

}

Q_DECLARE_METATYPE(HomeFeed::FeedEvent)

class FeedParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // The CTest property pins TZ=UTC; refuse to run against anything else
        // so a stray local run cannot pass or fail for the wrong reason.
        QCOMPARE(QTimeZone::systemTimeZoneId(), QByteArrayLiteral("UTC"));
        m_result = HomeFeed::parseFeed(fixture(u"feed.json"_s));
        QVERIFY2(m_result.ok(), qPrintable(m_result.error));
    }

    void parsesValidEntriesAndSkipsMalformedOnes()
    {
        QCOMPARE(m_result.events.size(), 8);
        QCOMPARE(m_result.skipped, 3);
        QCOMPARE(m_result.windowStart, QDate(2026, 5, 27));
        QCOMPARE(m_result.windowEnd, QDate(2027, 8, 25));
        QVERIFY(!find(m_result, u"bad-start"_s));
    }

    void timedFieldsPassThrough()
    {
        const FeedEvent *event = find(m_result, u"timed-single"_s);
        QVERIFY(event);
        QCOMPARE(event->data.title(), u"Dentist"_s);
        QCOMPARE(event->data.location(), u"Downtown"_s);
        QCOMPARE(event->data.description(), u"Bring the insurance card"_s);
        QCOMPARE(event->data.eventColor(), u"#7986cb"_s);
        QCOMPARE(event->data.type(), EventData::Event);
        QVERIFY(!event->data.isAllDay());
        QVERIFY(!event->data.isMinor());
        QCOMPARE(event->data.startDateTime().toUTC(), QDateTime(QDate(2026, 8, 25), QTime(14, 0), QTimeZone::UTC));
        QCOMPARE(event->data.endDateTime().toUTC(), QDateTime(QDate(2026, 8, 25), QTime(15, 0), QTimeZone::UTC));
        QCOMPARE(event->url, QUrl(u"https://calendar.example/event/timed-single"_s));
    }

    void nullFieldsBecomeEmptyStrings()
    {
        const FeedEvent *event = find(m_result, u"allday-single"_s);
        QVERIFY(event);
        QCOMPARE(event->data.location(), QString());
        QCOMPARE(event->data.description(), QString());
        QCOMPARE(event->data.eventColor(), QString());
    }

    void allDaySingleDayIgnoresExclusiveEnd()
    {
        // Pins the off-by-one where Google's exclusive end date leaked a
        // one-day event onto the following day as well.
        const FeedEvent *event = find(m_result, u"allday-single"_s);
        QVERIFY(event);
        QVERIFY(event->data.isAllDay());
        QCOMPARE(days(m_result, u"allday-single"_s), QList<QDate>{QDate(2026, 8, 28)});
        QCOMPARE(event->data.startDateTime(), QDate(2026, 8, 28).startOfDay());
        QCOMPARE(event->data.endDateTime(), QDate(2026, 8, 28).endOfDay());
    }

    void allDayMultiDayCoversEveryDay()
    {
        QCOMPARE(days(m_result, u"allday-three-days"_s), (QList<QDate>{QDate(2026, 9, 4), QDate(2026, 9, 5), QDate(2026, 9, 6)}));
    }

    void allDayWithoutEndIsOneDay()
    {
        QCOMPARE(days(m_result, u"allday-no-end"_s), QList<QDate>{QDate(2026, 9, 8)});
    }

    void timedOvernightTouchesBothDays()
    {
        QCOMPARE(days(m_result, u"timed-overnight"_s), (QList<QDate>{QDate(2026, 8, 30), QDate(2026, 8, 31)}));
    }

    void timedEndingAtMidnightStaysOnItsDay()
    {
        QCOMPARE(days(m_result, u"timed-ends-midnight"_s), QList<QDate>{QDate(2026, 9, 1)});
    }

    void timedWithoutEndCollapsesToStart()
    {
        const FeedEvent *event = find(m_result, u"timed-no-end"_s);
        QVERIFY(event);
        QCOMPARE(event->data.endDateTime(), event->data.startDateTime());
        QCOMPARE(days(m_result, u"timed-no-end"_s), QList<QDate>{QDate(2026, 9, 10)});
    }

    void timedOffsetIsConvertedToLocalDay()
    {
        // 23:30 at UTC-4 is 03:30 UTC the next day, and the test runs in UTC.
        QCOMPARE(days(m_result, u"timed-offset"_s), QList<QDate>{QDate(2026, 9, 16)});
    }

    void eventsByDayRestrictsToRangeAndExpandsSpans()
    {
        const QMultiHash<QDate, EventData> byDay = HomeFeed::eventsByDay(m_result.events, QDate(2026, 8, 30), QDate(2026, 9, 5));

        QCOMPARE(byDay.values(QDate(2026, 8, 30)).size(), 1); // night shift starts
        QCOMPARE(byDay.values(QDate(2026, 8, 31)).size(), 1); // night shift ends
        QCOMPARE(byDay.values(QDate(2026, 9, 1)).size(), 1); // party
        QCOMPARE(byDay.values(QDate(2026, 9, 2)).size(), 0); // party ended at midnight
        QCOMPARE(byDay.values(QDate(2026, 9, 4)).size(), 1); // trip day 1
        QCOMPARE(byDay.values(QDate(2026, 9, 5)).size(), 1); // trip day 2
        QCOMPARE(byDay.values(QDate(2026, 9, 6)).size(), 0); // trip day 3 is outside the range
        QCOMPARE(byDay.values(QDate(2026, 8, 25)).size(), 0); // dentist is before the range
        QCOMPARE(byDay.size(), 5);
        QCOMPARE(byDay.values(QDate(2026, 9, 4)).first().uid(), u"allday-three-days"_s);
    }

    void rejectsInvalidDocuments_data()
    {
        QTest::addColumn<QByteArray>("document");
        QTest::addColumn<QString>("errorPrefix");
        QTest::newRow("not json") << QByteArrayLiteral("not json") << u"invalid JSON"_s;
        QTest::newRow("array") << QByteArrayLiteral("[]") << u"top-level value is not an object"_s;
        QTest::newRow("no events") << QByteArrayLiteral("{\"window\":{}}") << u"missing \"events\" array"_s;
        QTest::newRow("events not array") << QByteArrayLiteral("{\"events\":{}}") << u"missing \"events\" array"_s;
    }

    void rejectsInvalidDocuments()
    {
        QFETCH(QByteArray, document);
        QFETCH(QString, errorPrefix);
        const ParseResult result = HomeFeed::parseFeed(document);
        QVERIFY(!result.ok());
        QVERIFY2(result.error.startsWith(errorPrefix), qPrintable(result.error));
        QVERIFY(result.events.isEmpty());
    }

    void emptyEventsArrayIsValid()
    {
        const ParseResult result = HomeFeed::parseFeed(QByteArrayLiteral("{\"events\":[]}"));
        QVERIFY(result.ok());
        QVERIFY(result.events.isEmpty());
        QVERIFY(!result.windowStart.isValid());
    }

    void diffOfIdenticalStatesIsEmpty()
    {
        const QMultiHash<QDate, EventData> byDay = HomeFeed::eventsByDay(m_result.events, kFrom, kTo);
        const HomeFeed::PublishDiff diff = HomeFeed::diffPublished(byDay, byDay);
        QVERIFY(diff.removedUids.isEmpty());
        QVERIFY(diff.added.isEmpty());
    }

    void diffReportsVanishedChangedAndNewEvents()
    {
        const QMultiHash<QDate, EventData> previous = HomeFeed::eventsByDay(m_result.events, kFrom, kTo);

        QList<FeedEvent> events = m_result.events;
        events.erase(std::remove_if(events.begin(),
                                    events.end(),
                                    [](const FeedEvent &event) {
                                        return event.data.uid() == u"timed-single"_s;
                                    }),
                     events.end());
        for (FeedEvent &event : events) {
            if (event.data.uid() == u"allday-three-days"_s) {
                event.data.setTitle(u"Road trip"_s);
            }
        }
        EventData fresh;
        fresh.setUid(u"brand-new"_s);
        fresh.setTitle(u"New"_s);
        fresh.setStartDateTime(QDate(2026, 9, 20).startOfDay());
        fresh.setEndDateTime(QDate(2026, 9, 20).endOfDay());
        fresh.setIsAllDay(true);
        events.append({fresh, QUrl(), {}});

        const HomeFeed::PublishDiff diff = HomeFeed::diffPublished(previous, HomeFeed::eventsByDay(events, kFrom, kTo));

        // The three-day trip is removed once, not once per day.
        QCOMPARE(diff.removedUids, (QStringList{u"allday-three-days"_s, u"timed-single"_s}));
        QCOMPARE(diff.added.size(), 4); // retitled trip under its three days, plus the new event
        QCOMPARE(diff.added.values(QDate(2026, 9, 4)).first().title(), u"Road trip"_s);
        QCOMPARE(diff.added.values(QDate(2026, 9, 5)).size(), 1);
        QCOMPARE(diff.added.values(QDate(2026, 9, 6)).size(), 1);
        QCOMPARE(diff.added.values(QDate(2026, 9, 20)).first().uid(), u"brand-new"_s);
        QVERIFY(diff.added.values(QDate(2026, 8, 28)).isEmpty()); // untouched payday is not re-sent
    }

    void diffMovedEventLandsOnlyOnItsNewDay()
    {
        // Pins why eventModified() is not used: it replaces in place and would
        // leave the event on its old day.
        const QMultiHash<QDate, EventData> previous = HomeFeed::eventsByDay(m_result.events, kFrom, kTo);
        QList<FeedEvent> events = m_result.events;
        for (FeedEvent &event : events) {
            if (event.data.uid() == u"allday-single"_s) {
                event.data.setStartDateTime(QDate(2026, 8, 29).startOfDay());
                event.data.setEndDateTime(QDate(2026, 8, 29).endOfDay());
            }
        }
        const HomeFeed::PublishDiff diff = HomeFeed::diffPublished(previous, HomeFeed::eventsByDay(events, kFrom, kTo));
        QCOMPARE(diff.removedUids, QStringList{u"allday-single"_s});
        QCOMPARE(diff.added.keys(), QList<QDate>{QDate(2026, 8, 29)});
    }

    void diffAgainstEmptyRemovesEveryUidOnce()
    {
        const QMultiHash<QDate, EventData> previous = HomeFeed::eventsByDay(m_result.events, kFrom, kTo);
        const HomeFeed::PublishDiff diff = HomeFeed::diffPublished(previous, {});
        QCOMPARE(diff.removedUids.size(), 8);
        QCOMPARE(QSet<QString>(diff.removedUids.cbegin(), diff.removedUids.cend()).size(), 8);
        QVERIFY(diff.added.isEmpty());
    }

    void sameEventDetectsFieldChanges()
    {
        const FeedEvent *event = find(m_result, u"timed-single"_s);
        QVERIFY(event);
        EventData copy = event->data;
        QVERIFY(HomeFeed::sameEvent(event->data, copy));
        copy.setTitle(u"Orthodontist"_s);
        QVERIFY(!HomeFeed::sameEvent(event->data, copy));
        copy = event->data;
        copy.setStartDateTime(copy.startDateTime().addSecs(60));
        QVERIFY(!HomeFeed::sameEvent(event->data, copy));
        copy = event->data;
        copy.setEventColor(u"#000000"_s);
        QVERIFY(!HomeFeed::sameEvent(event->data, copy));
    }

    void nextEventKeepsAMeetingUntilItEnds()
    {
        const QList<FeedEvent> events{timed(u"later"_s, at(19, 15), at(19, 16)), timed(u"meeting"_s, at(19, 13), at(19, 13, 30))};
        QCOMPARE(nextUid(events, at(19, 9)), u"meeting"_s);
        QCOMPARE(nextUid(events, at(19, 13, 10)), u"meeting"_s);
        QCOMPARE(nextUid(events, at(19, 13, 30)), u"later"_s);
        QCOMPARE(nextUid(events, at(19, 16)), u"none"_s);
    }

    void nextEventShowsAllDayOnlyBeforeItsDay()
    {
        const QList<FeedEvent> events{timed(u"call"_s, at(22, 10), at(22, 10, 30)), allDay(u"clinic"_s, QDate(2026, 10, 22), QDate(2026, 10, 22))};
        QCOMPARE(nextUid(events, at(21, 20)), u"clinic"_s);
        // On its own day the all-day event steps aside for the timed ones.
        QCOMPARE(nextUid(events, at(22, 0)), u"call"_s);
        QCOMPARE(nextUid(events, at(22, 8)), u"call"_s);
    }

    void nextEventOfNothingIsNothing()
    {
        QCOMPARE(nextUid({}, at(19, 9)), u"none"_s);
    }

    void nextEventBreaksTiesOnEndThenUid()
    {
        const QList<FeedEvent> events{timed(u"long"_s, at(19, 13), at(19, 15)),
                                      timed(u"b-short"_s, at(19, 13), at(19, 14)),
                                      timed(u"a-short"_s, at(19, 13), at(19, 14))};
        QCOMPARE(nextUid(events, at(19, 9)), u"a-short"_s);
    }

    void whenLabelNamesTheDayAndTime_data()
    {
        QTest::addColumn<FeedEvent>("event");
        QTest::addColumn<QDateTime>("now");
        QTest::addColumn<QString>("dayPart");
        QTest::addColumn<QTime>("time");

        const QDateTime monday = at(19, 9);
        QTest::newRow("today") << timed(u"e"_s, at(19, 13), at(19, 14)) << monday << QString() << QTime(13, 0);
        QTest::newRow("tomorrow") << timed(u"e"_s, at(20, 15), at(20, 16)) << monday << u"Tomorrow "_s << QTime(15, 0);
        QTest::newRow("this week") << timed(u"e"_s, at(22, 10), at(22, 11)) << monday << u"Thu "_s << QTime(10, 0);
        QTest::newRow("six days") << timed(u"e"_s, at(25, 10), at(25, 11)) << monday << u"Sun "_s << QTime(10, 0);
        QTest::newRow("a week out") << timed(u"e"_s, at(26, 13), at(26, 14)) << monday << u"Oct 26 "_s << QTime(13, 0);
        QTest::newRow("running") << timed(u"e"_s, at(19, 13), at(19, 13, 30)) << at(19, 13, 10) << u"now, until "_s << QTime(13, 30);
        QTest::newRow("running past midnight") << timed(u"e"_s, at(19, 23), at(20, 1)) << at(19, 23, 30) << u"now, until Tomorrow "_s << QTime(1, 0);
        // 13:00 at UTC-4 is 17:00 in the test's UTC zone.
        const QDateTime offsetStart = QDateTime::fromString(u"2026-10-19T13:00:00-04:00"_s, Qt::ISODate);
        QTest::newRow("converted to local") << timed(u"e"_s, offsetStart, offsetStart.addSecs(1800)) << monday << QString() << QTime(17, 0);
    }

    void whenLabelNamesTheDayAndTime()
    {
        QFETCH(FeedEvent, event);
        QFETCH(QDateTime, now);
        QFETCH(QString, dayPart);
        QFETCH(QTime, time);
        // The time half comes from the locale itself, which decides the AM/PM
        // spacing; the rules under test are the day words.
        const QLocale locale(QLocale::English, QLocale::UnitedStates);
        QCOMPARE(HomeFeed::whenLabel(event.data, now, locale), dayPart + locale.toString(time, QLocale::ShortFormat));
    }

    void whenLabelForAllDayEvents_data()
    {
        QTest::addColumn<QDate>("day");
        QTest::addColumn<QString>("expected");
        QTest::newRow("today") << QDate(2026, 10, 19) << u"all day"_s;
        QTest::newRow("tomorrow") << QDate(2026, 10, 20) << u"Tomorrow, all day"_s;
        QTest::newRow("this week") << QDate(2026, 10, 22) << u"Thu, all day"_s;
        QTest::newRow("later") << QDate(2026, 11, 2) << u"Nov 2, all day"_s;
    }

    void whenLabelForAllDayEvents()
    {
        QFETCH(QDate, day);
        QFETCH(QString, expected);
        const QLocale locale(QLocale::English, QLocale::UnitedStates);
        QCOMPARE(HomeFeed::whenLabel(allDay(u"e"_s, day, day).data, at(19, 9), locale), expected);
    }

    void departuresParseAndDropMalformedEntries()
    {
        const FeedEvent *event = find(m_result, u"timed-single"_s);
        QVERIFY(event);
        QCOMPARE(event->departures.size(), 2);
        QCOMPARE(event->departures.at(0).mode, u"Bus"_s);
        QCOMPARE(event->departures.at(0).leave.toUTC(), QDateTime(QDate(2026, 8, 25), QTime(13, 10), QTimeZone::UTC));
        QCOMPARE(event->departures.at(0).detail, u"40 min"_s);
        QCOMPARE(event->departures.at(1).detail, QString());
        QCOMPARE(event->departures.at(0).fare, QString());
        QCOMPARE(event->departures.at(1).fare, u"~$24"_s);
        QVERIFY(find(m_result, u"allday-single"_s)->departures.isEmpty());
    }

    void departuresShowOnlyBeforeTheEventStarts()
    {
        FeedEvent event = timed(u"dentist"_s, at(19, 14), at(19, 15));
        event.departures = {{u"Bus"_s, at(19, 13, 10), u"40 min"_s, QString()}, {u"Car"_s, at(19, 13, 35), QString(), u"~$24"_s}};
        const QLocale locale(QLocale::English, QLocale::UnitedStates);

        QCOMPARE(HomeFeed::pendingDepartures(event, at(19, 9)).size(), 2);
        QCOMPARE(HomeFeed::departureLabel(event.departures.at(0), locale), u"Bus "_s + locale.toString(QTime(13, 10), QLocale::ShortFormat));
        const QString carTime = locale.toString(QTime(13, 35), QLocale::ShortFormat);
        QCOMPARE(HomeFeed::departureLabel(event.departures.at(1), locale), u"Car "_s + carTime);
        QCOMPARE(HomeFeed::departureSummary(event.departures.at(0), locale), u"Bus "_s + locale.toString(QTime(13, 10), QLocale::ShortFormat) + u" · 40 min"_s);
        QCOMPARE(HomeFeed::departureSummary(event.departures.at(1), locale), u"Car "_s + carTime + u" ~$24"_s);
        // Past its leave time but before the event: still shown.
        QCOMPARE(HomeFeed::pendingDepartures(event, at(19, 13, 20)).size(), 2);
        QVERIFY(HomeFeed::pendingDepartures(event, at(19, 14)).isEmpty());
    }

    void upcomingEventsAreOrderedAndCapped()
    {
        const QList<FeedEvent> events{timed(u"c"_s, at(21, 9), at(21, 10)),
                                      timed(u"past"_s, at(18, 9), at(18, 10)),
                                      timed(u"a"_s, at(19, 9), at(19, 10)),
                                      timed(u"b"_s, at(20, 9), at(20, 10))};
        const QList<FeedEvent> upcoming = HomeFeed::upcomingEvents(events, at(19, 8), 2);
        QCOMPARE(upcoming.size(), 2);
        QCOMPARE(upcoming.at(0).data.uid(), u"a"_s);
        QCOMPARE(upcoming.at(1).data.uid(), u"b"_s);
        QCOMPARE(HomeFeed::upcomingEvents(events, at(19, 8), 10).size(), 3);
    }

    void listRowsNameTheirDayAndTime()
    {
        const QLocale locale(QLocale::English, QLocale::UnitedStates);
        const QDate today(2026, 10, 19);
        QCOMPARE(HomeFeed::dayHeading(today, today, locale), u"Today"_s);
        QCOMPARE(HomeFeed::dayHeading(QDate(2026, 10, 20), today, locale), u"Tomorrow"_s);
        QCOMPARE(HomeFeed::dayHeading(QDate(2026, 10, 22), today, locale), u"Thu Oct 22"_s);

        const FeedEvent overnight = timed(u"e"_s, at(18, 23), at(19, 1));
        // Started yesterday and still running: filed under today, as "now".
        QCOMPARE(HomeFeed::listDay(overnight.data, at(19, 0, 30)), today);
        QCOMPARE(HomeFeed::rowTimeLabel(overnight.data, at(19, 0, 30), locale), u"now"_s);
        QCOMPARE(HomeFeed::untilLabel(overnight.data, at(19, 0, 30), locale), u"until "_s + locale.toString(QTime(1, 0), QLocale::ShortFormat));

        const FeedEvent later = timed(u"e"_s, at(20, 13), at(20, 14));
        QCOMPARE(HomeFeed::listDay(later.data, at(19, 9)), QDate(2026, 10, 20));
        QCOMPARE(HomeFeed::rowTimeLabel(later.data, at(19, 9), locale), locale.toString(QTime(13, 0), QLocale::ShortFormat));
        QCOMPARE(HomeFeed::untilLabel(later.data, at(19, 9), locale), QString());

        const FeedEvent clinic = allDay(u"e"_s, QDate(2026, 10, 22), QDate(2026, 10, 22));
        QCOMPARE(HomeFeed::rowTimeLabel(clinic.data, at(19, 9), locale), u"all day"_s);
        QCOMPARE(HomeFeed::untilLabel(clinic.data, at(22, 9), locale), QString());
    }

private:
    // Wide enough to hold every valid fixture event.
    inline static const QDate kFrom{2026, 8, 25};
    inline static const QDate kTo{2026, 9, 30};
    ParseResult m_result;
};

QTEST_GUILESS_MAIN(FeedParserTest)

#include "feedparsertest.moc"
