// SPDX-License-Identifier: MIT
#pragma once

#include "feedparser.h"
#include "feedsource.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQmlIntegration/qqmlintegration.h>

// The feed's next event for the panel applet, re-evaluated on every fetch and
// at each minute boundary so "now" and the day words stay current.
class NextEvent : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool valid READ isValid NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString when READ when NOTIFY changed)
    Q_PROPERTY(QString location READ location NOTIFY changed)

public:
    explicit NextEvent(QObject *parent = nullptr);

    bool isValid() const;
    const QString &title() const;
    const QString &when() const;
    const QString &location() const;

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
};
