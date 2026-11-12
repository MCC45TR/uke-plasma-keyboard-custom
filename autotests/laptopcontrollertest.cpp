// SPDX-License-Identifier: GPL-2.0-or-later
#include "laptopcontroller.h"
#include <QtTest>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVirtualObject>
#include <QTemporaryDir>

class DeferredPower final : public QDBusVirtualObject {
public:
    QDBusMessage request;
    int queries=0;
    void reply(const QVariantMap &properties) {
        QDBusConnection::sessionBus().send(request.createReply(QVariantList{properties}));
    }
    QString introspect(const QString &)const override {return QString();}
    bool handleMessage(const QDBusMessage &message,const QDBusConnection &)override {
        if(message.interface()!=QLatin1String("org.freedesktop.DBus.Properties")||message.member()!=QLatin1String("GetAll"))return false;
        message.setDelayedReply(true);request=message;++queries;return true;
    }
};
class DeferredBackend final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.uke.PlasmaKeyboard.Backend1")
public:
    QDBusMessage request;
    int staticBegins=0,dynamicBegins=0,staticEnds=0,dynamicEnds=0,profiles=0;
    QDBusConnection server{QDBusConnection::connectToBus(QDBusConnection::SessionBus,QStringLiteral("fixture-backend"))};
    void reply() { server.send(request.createReply(QVariantList{true})); }
public Q_SLOTS:
    bool BeginStatic(const QString &) {request=message();setDelayedReply(true);++staticBegins;return false;}
    bool BeginDynamic(const QString &) {request=message();setDelayedReply(true);++dynamicBegins;return false;}
    void EndStatic() {++staticEnds;}
    void EndDynamic() {++dynamicEnds;}
    bool Profile(const QString &) {++profiles;return true;}
    void ReleaseAll() {}
};
class PluginManager final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.Plugins")
public:
    QStringList operations;
public Q_SLOTS:
    void UnloadPlugin(const QString &id) {operations.append(QStringLiteral("unload:")+id);}
    bool LoadPlugin(const QString &id) {operations.append(QStringLiteral("load:")+id);return true;}
};
class LaptopControllerTest final : public QObject {
    Q_OBJECT
    QTemporaryDir m_directory;
    DeferredPower m_power;
    DeferredBackend m_backend;
    PluginManager m_plugins;
    static void eligible(LaptopController &controller,bool dynamic) {
        controller.m_ready=true;controller.m_available=true;controller.m_locked=false;
        controller.m_lidClosed=false;controller.m_tabletName=QStringLiteral("synthetic-panel");
        controller.m_options[QStringLiteral("presentation")]=dynamic?QStringLiteral("dynamic"):QStringLiteral("static");
    }
private Q_SLOTS:
    void initTestCase() {
        QCOMPARE(qgetenv("PLASMA_KEYBOARD_TEST_ISOLATED_BUS"),QByteArray("1"));
        QCOMPARE(qgetenv("DBUS_SYSTEM_BUS_ADDRESS"),qgetenv("DBUS_SESSION_BUS_ADDRESS"));
        QVERIFY(m_directory.isValid());
        qputenv("XDG_CONFIG_HOME",m_directory.path().toUtf8());
        auto bus=QDBusConnection::sessionBus();
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.UPower")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/UPower"),&m_power));
        QVERIFY(m_backend.server.isConnected());
        QVERIFY(m_backend.server.registerService(QStringLiteral("org.uke.PlasmaKeyboard.Backend")));
        QVERIFY(m_backend.server.registerObject(QStringLiteral("/org/uke/PlasmaKeyboard/Backend"),&m_backend,QDBusConnection::ExportAllSlots));
        QVERIFY(m_backend.server.registerService(QStringLiteral("org.kde.KWin")));
        QVERIFY(m_backend.server.registerObject(QStringLiteral("/Plugins"),&m_plugins,QDBusConnection::ExportAllSlots));
    }
    void lateOpenReplyCannotUndoCoverClose() {
        LaptopController controller;
        const int previous=m_power.queries;
        controller.queryLid();
        QTRY_COMPARE(m_power.queries,previous+1);
        controller.propertiesChanged(QStringLiteral("org.freedesktop.UPower"),{{QStringLiteral("LidIsClosed"),true}},{});
        m_power.reply({{QStringLiteral("LidIsPresent"),true},{QStringLiteral("LidIsClosed"),false}});
        QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        QVERIFY(controller.m_lidClosed);
    }
    void unknownCoverMetadataStaysClosed() {
        LaptopController controller;
        const int previous=m_power.queries;
        controller.queryLid();QTRY_COMPARE(m_power.queries,previous+1);
        m_power.reply({{QStringLiteral("LidIsPresent"),true}});
        QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        QVERIFY(controller.m_lidClosed);
        controller.queryLid();QTRY_COMPARE(m_power.queries,previous+2);
        m_power.reply({{QStringLiteral("LidIsPresent"),true},{QStringLiteral("LidIsClosed"),false}});
        QTRY_VERIFY(!controller.m_lidClosed);
        controller.propertiesChanged(QStringLiteral("org.freedesktop.UPower"),{},{QStringLiteral("LidIsClosed")});
        QVERIFY(controller.m_lidClosed);
        QTRY_COMPARE(m_power.queries,previous+3);
        m_power.reply({{QStringLiteral("LidIsPresent"),true},{QStringLiteral("LidIsClosed"),true}});
        QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        QVERIFY(controller.m_lidClosed);
    }
    void leaveInvalidatesEntry_data() {
        QTest::addColumn<bool>("dynamic");QTest::newRow("static")<<false;QTest::newRow("dynamic")<<true;
    }
    void leaveInvalidatesEntry() {
        QFETCH(bool,dynamic);LaptopController controller;eligible(controller,dynamic);
        const int before=m_backend.staticBegins+m_backend.dynamicBegins;
        controller.enter();QTRY_COMPARE(m_backend.staticBegins+m_backend.dynamicBegins,before+1);
        QVERIFY(controller.m_starting);QVERIFY(!controller.active());
        controller.leave();m_backend.reply();
        QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        QVERIFY(!controller.active());QVERIFY(!controller.m_starting);
    }
    void dynamicNeverAcquiresRefreshOwnership() {
        LaptopController controller;eligible(controller,true);
        controller.m_options[QStringLiteral("refreshProfile")]=QStringLiteral("30");
        const int before=m_backend.dynamicBegins,profiles=m_backend.profiles,staticBegins=m_backend.staticBegins;
        controller.enter();QTRY_COMPARE(m_backend.dynamicBegins,before+1);m_backend.reply();
        QTRY_VERIFY(controller.dynamicActive());
        QCOMPARE(m_backend.staticBegins,staticBegins);QCOMPARE(m_backend.profiles,profiles);
        controller.leave();QVERIFY(!controller.active());
    }
    void reloadsOnlyTheOwnedIntegrationBeforeLoading() {
        m_plugins.operations.clear();
        LaptopController controller;
        controller.loadBackend();
        QTRY_COMPARE(m_plugins.operations.size(),2);
        const QStringList expected{QStringLiteral("unload:uke_keyboard_backend"),QStringLiteral("load:uke_keyboard_backend")};
        QCOMPARE(m_plugins.operations,expected);
        QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
    }
};
QTEST_MAIN(LaptopControllerTest)
#include "laptopcontrollertest.moc"
