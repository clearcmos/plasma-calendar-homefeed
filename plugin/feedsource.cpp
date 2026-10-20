// SPDX-License-Identifier: MIT
#include "feedsource.h"
#include "homefeed_debug.h"

#include <QNetworkReply>
#include <QNetworkRequest>

using namespace Qt::StringLiterals;

namespace
{

constexpr int kTransferTimeoutMs = 15'000;
constexpr int kDefaultRefreshMinutes = 5;

}

namespace HomeFeed
{

FeedSource::FeedSource(QObject *parent)
    : QObject(parent)
    , m_config(KSharedConfig::openConfig(u"plasma-calendar-homefeedrc"_s, KConfig::NoGlobals))
{
    m_network.setTransferTimeout(kTransferTimeoutMs);
    m_network.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    m_refreshTimer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, &FeedSource::refresh);

    readSettings(m_config->group(u"General"_s));

    m_configWatcher = KConfigWatcher::create(m_config);
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() != "General"_L1) {
            return;
        }
        readSettings(group);
        Q_EMIT settingsChanged();
        refresh();
    });

    refresh();
}

FeedSource::~FeedSource() = default;

bool FeedSource::isFetching() const
{
    return m_pendingReply != nullptr;
}

const QString &FeedSource::eventColor() const
{
    return m_eventColor;
}

void FeedSource::readSettings(const KConfigGroup &group)
{
    m_feedUrl = QUrl(group.readEntry("FeedUrl", QString()));
    m_eventColor = group.readEntry("EventColor", QString());
    const int minutes = qMax(1, group.readEntry("RefreshMinutes", kDefaultRefreshMinutes));
    m_refreshTimer.setInterval(minutes * 60 * 1000);
    m_warnedMissingUrl = false;

    if (m_feedUrl.isValid() && !m_feedUrl.isEmpty()) {
        m_refreshTimer.start();
    } else {
        m_refreshTimer.stop();
    }
}

void FeedSource::refresh()
{
    if (!m_feedUrl.isValid() || m_feedUrl.isEmpty()) {
        if (!m_warnedMissingUrl) {
            qCWarning(HOMEFEED) << "No FeedUrl configured in plasma-calendar-homefeedrc [General]; nothing to show";
            m_warnedMissingUrl = true;
        }
        return;
    }
    if (m_pendingReply) {
        return;
    }

    QNetworkRequest request(m_feedUrl);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setHeader(QNetworkRequest::UserAgentHeader, u"plasma-calendar-homefeed"_s);
    m_pendingReply = m_network.get(request);
    connect(m_pendingReply, &QNetworkReply::finished, this, [this, reply = m_pendingReply] {
        onReplyFinished(reply);
    });
}

void FeedSource::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply == m_pendingReply) {
        m_pendingReply = nullptr;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(HOMEFEED) << "Feed fetch failed, keeping previous events:" << reply->url().toDisplayString() << reply->errorString();
        return;
    }

    const ParseResult result = parseFeed(reply->readAll());
    if (!result.ok()) {
        qCWarning(HOMEFEED) << "Feed rejected, keeping previous events:" << result.error;
        return;
    }
    if (result.skipped > 0) {
        qCInfo(HOMEFEED) << "Skipped" << result.skipped << "malformed feed entries";
    }
    Q_EMIT noticesFetched(result.notices);
    Q_EMIT eventsFetched(result.events);
}

}

#include "moc_feedsource.cpp"
