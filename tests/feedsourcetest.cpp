// SPDX-License-Identifier: MIT
#include "feedsource.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>

using namespace Qt::StringLiterals;

// Drives the shared fetcher against file:// URLs, which QNetworkAccessManager
// serves through the same reply path as HTTP, so no server is needed.
class FeedSourceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // CTest points XDG_CONFIG_HOME into the build tree; never write the
        // user's real rc file.
        QVERIFY(qEnvironmentVariableIsSet("XDG_CONFIG_HOME"));
        QVERIFY(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation).startsWith(qEnvironmentVariable("XDG_CONFIG_HOME")));
    }

    void fetchesTheConfiguredFeed()
    {
        configure(fixtureUrl(u"feed.json"_s), u"#3daee9"_s);
        HomeFeed::FeedSource source;
        QSignalSpy fetched(&source, &HomeFeed::FeedSource::eventsFetched);
        QVERIFY(fetched.wait());
        QCOMPARE(fetched.takeFirst().at(0).value<QList<HomeFeed::FeedEvent>>().size(), 8);
        QVERIFY(!source.isFetching());
        QCOMPARE(source.eventColor(), u"#3daee9"_s);
    }

    void staysOfflineWithoutAUrl()
    {
        configure(QString(), QString());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"No FeedUrl configured"_s));
        HomeFeed::FeedSource source;
        QVERIFY(!source.isFetching());
    }

    void reportsNothingWhenTheFetchFails()
    {
        configure(fixtureUrl(u"missing.json"_s), QString());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Feed fetch failed"_s));
        HomeFeed::FeedSource source;
        QSignalSpy fetched(&source, &HomeFeed::FeedSource::eventsFetched);
        QTRY_VERIFY(!source.isFetching());
        QCOMPARE(fetched.size(), 0);
    }

    void reportsNothingForARejectedDocument()
    {
        configure(fixtureUrl(u"rejected.json"_s), QString());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Feed rejected"_s));
        HomeFeed::FeedSource source;
        QSignalSpy fetched(&source, &HomeFeed::FeedSource::eventsFetched);
        QTRY_VERIFY(!source.isFetching());
        QCOMPARE(fetched.size(), 0);
    }

private:
    static QString fixtureUrl(const QString &name)
    {
        return QUrl::fromLocalFile(QStringLiteral(FIXTURE_DIR) + u'/' + name).toString();
    }

    static void configure(const QString &feedUrl, const QString &eventColor)
    {
        KConfigGroup general = KSharedConfig::openConfig(u"plasma-calendar-homefeedrc"_s, KConfig::NoGlobals)->group(u"General"_s);
        general.writeEntry("FeedUrl", feedUrl);
        general.writeEntry("EventColor", eventColor);
        general.sync();
    }
};

QTEST_GUILESS_MAIN(FeedSourceTest)

#include "feedsourcetest.moc"
