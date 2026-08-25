// SPDX-License-Identifier: MIT
#pragma once

#include "feedparser.h"

#include <CalendarEvents/CalendarEventsPlugin>

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>

#include <QDate>
#include <QHash>
#include <QList>
#include <QMultiHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

// Calendar events plugin for the Plasma digital clock. Periodically fetches a
// JSON feed over HTTP(S) and republishes its events to the clock's DaysModel.
//
// Publishing contract (mirrors plasma-workspace's holidays plugin): the model
// clears its store and calls loadEventsForDateRange() whenever the visible
// month changes, so a full dataReady() for the requested range is the answer
// to that call. Between calls, refreshes only emit the difference as
// eventRemoved()/dataReady() so nothing is duplicated.
class HomeFeedEventsPlugin : public CalendarEvents::CalendarEventsPlugin, public CalendarEvents::ShowEventInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.CalendarEventsPlugin" FILE "homefeedevents.json")
    Q_INTERFACES(CalendarEvents::CalendarEventsPlugin CalendarEvents::ShowEventInterface)

public:
    explicit HomeFeedEventsPlugin(QObject *parent = nullptr);
    ~HomeFeedEventsPlugin() override;

    void loadEventsForDateRange(const QDate &startDate, const QDate &endDate) override;
    bool showEvent(const QString &uid) override;

private:
    void readSettings(const KConfigGroup &group);
    void refresh();
    void onReplyFinished(QNetworkReply *reply);
    void applyEvents(const QList<HomeFeed::FeedEvent> &events);
    void forgetPublished();

    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_configWatcher;
    QUrl m_feedUrl;
    QString m_defaultColor;
    bool m_warnedMissingUrl = false;

    QNetworkAccessManager m_network;
    QNetworkReply *m_pendingReply = nullptr;
    QTimer m_refreshTimer;

    bool m_fetched = false;
    QList<HomeFeed::FeedEvent> m_events;
    QHash<QString, QUrl> m_links;

    QDate m_rangeStart;
    QDate m_rangeEnd;
    // Exactly what the model currently holds from this plugin for the range.
    QMultiHash<QDate, CalendarEvents::EventData> m_published;
};
