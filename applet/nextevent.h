// SPDX-License-Identifier: MIT
#pragma once

#include "feedparser.h"
#include "feedsource.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

// The feed's next event for the panel applet, and the events after it for its
// popup, re-evaluated on every fetch and at each minute boundary so "now",
// "leave now", and the day words stay current.
class NextEvent : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool valid READ isValid NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString when READ when NOTIFY changed)
    Q_PROPERTY(QString location READ location NOTIFY changed)
    // [{text, late}] for the panel line.
    Q_PROPERTY(QVariantList departures READ departures NOTIFY changed)
    // [{heading, time, title, lines, departures: [{text, summary, late}]}];
    // heading is set only on the first row of each day.
    Q_PROPERTY(QVariantList upcoming READ upcoming NOTIFY changed)
    // The feed's notices, shown under the popup's events.
    Q_PROPERTY(QStringList notices READ notices NOTIFY changed)

public:
    explicit NextEvent(QObject *parent = nullptr);

    bool isValid() const;
    const QString &title() const;
    const QString &when() const;
    const QString &location() const;
    const QVariantList &departures() const;
    const QVariantList &upcoming() const;
    const QStringList &notices() const;

Q_SIGNALS:
    void changed();

private:
    void update();
    void scheduleTick();

    HomeFeed::FeedSource m_source;
    QList<HomeFeed::FeedEvent> m_events;
    QTimer m_tick;

    bool m_valid = false;
    QString m_title;
    QString m_when;
    QString m_location;
    QVariantList m_departures;
    QVariantList m_upcoming;
    QStringList m_notices;
};
