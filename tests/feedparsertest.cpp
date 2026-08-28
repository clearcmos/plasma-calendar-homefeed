// SPDX-License-Identifier: MIT
#include "feedparser.h"

#include <QFile>
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

QList<QDate> days(const ParseResult &result, const QString &uid)
{
    const FeedEvent *event = find(result, uid);
    if (!event) {
        return {};
    }
    return HomeFeed::daysSpanned(event->data);
}

}

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
        events.append({fresh, QUrl()});

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

private:
    // Wide enough to hold every valid fixture event.
    inline static const QDate kFrom{2026, 8, 25};
    inline static const QDate kTo{2026, 9, 30};
    ParseResult m_result;
};

QTEST_GUILESS_MAIN(FeedParserTest)

#include "feedparsertest.moc"
