// SPDX-License-Identifier: MIT
#pragma once

#include "feedparser.h"

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

namespace HomeFeed
{

// Fetches the feed named by plasma-calendar-homefeedrc [General] FeedUrl on
// construction and every RefreshMinutes, and reports each document that
// parses. A failed fetch or a rejected document is only logged, so consumers
// keep what they had. Shared by the clock plugin and the panel applet.
class FeedSource : public QObject
{
    Q_OBJECT

public:
    explicit FeedSource(QObject *parent = nullptr);
    ~FeedSource() override;

    void refresh();
    bool isFetching() const;
    const QString &eventColor() const;

Q_SIGNALS:
    // [General] changed: events from the old settings no longer apply. A fetch
    // with the new settings follows.
    void settingsChanged();
    void eventsFetched(const QList<HomeFeed::FeedEvent> &events);

private:
    void readSettings(const KConfigGroup &group);
    void onReplyFinished(QNetworkReply *reply);

    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_configWatcher;
    QUrl m_feedUrl;
    QString m_eventColor;
    bool m_warnedMissingUrl = false;

    QNetworkAccessManager m_network;
    QNetworkReply *m_pendingReply = nullptr;
    QTimer m_refreshTimer;
};

}
