// SPDX-License-Identifier: MIT
#include <CalendarEvents/CalendarEventsPlugin>

#include <KPluginMetaData>

#include <QFile>
#include <QJsonObject>
#include <QPluginLoader>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::StringLiterals;

// Loads the built plugin the way plasma-workspace's EventPluginsManager does
// (KPluginMetaData for discovery, QPluginLoader for instantiation) and checks
// the contract the clock relies on. This is the behavioral gate for the plugin
// glue that the parser tests cannot reach.
class PluginLoadTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // CTest points XDG_CONFIG_HOME at an empty directory, so the plugin
        // finds no FeedUrl and stays offline for the whole test.
        QVERIFY(qEnvironmentVariableIsSet("XDG_CONFIG_HOME"));
        const QString rc = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/plasma-calendar-homefeedrc"_s;
        QVERIFY2(!QFile::exists(rc), qPrintable(rc));
    }

    void metadataFollowsKPluginConventions()
    {
        const KPluginMetaData metaData(QStringLiteral(PLUGIN_PATH));
        QVERIFY(metaData.isValid());
        QCOMPARE(metaData.pluginId(), u"homefeedevents"_s);
        QCOMPARE(metaData.name(), u"Home feed"_s);
        QCOMPARE(metaData.license(), u"MIT"_s);
        QVERIFY(!metaData.description().isEmpty());
        QVERIFY(!metaData.iconName().isEmpty());
        // Regression: an explicit "Id" in the JSON made KPluginMetaData log a
        // warning on every plasmashell start (the id comes from the file name).
        QVERIFY(!metaData.rawData().value("KPlugin"_L1).toObject().contains("Id"_L1));
    }

    void loadsAndImplementsBothInterfaces()
    {
        QPluginLoader loader(QStringLiteral(PLUGIN_PATH));
        QCOMPARE(loader.metaData().value("IID"_L1).toString(), u"org.kde.CalendarEventsPlugin"_s);
        QVERIFY2(loader.load(), qPrintable(loader.errorString()));

        QObject *instance = loader.instance();
        QVERIFY(instance);
        auto *plugin = qobject_cast<CalendarEvents::CalendarEventsPlugin *>(instance);
        QVERIFY(plugin);
        auto *show = qobject_cast<CalendarEvents::ShowEventInterface *>(instance);
        QVERIFY(show);

        // Unconfigured plugin: a range request is a safe no-op and unknown uids
        // must not open anything.
        plugin->loadEventsForDateRange(QDate(2026, 8, 1), QDate(2026, 9, 12));
        QVERIFY(!show->showEvent(u"unknown"_s));
    }
};

QTEST_GUILESS_MAIN(PluginLoadTest)

#include "pluginloadtest.moc"
