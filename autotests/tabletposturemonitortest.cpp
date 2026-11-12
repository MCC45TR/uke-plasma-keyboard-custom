// SPDX-License-Identifier: GPL-2.0-or-later
#include "tabletposturemonitor.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVirtualObject>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTest>
#include <QUuid>
#include <ctime>
#include <limits>

namespace {
constexpr auto service = "org.senemos.Uke.Motion1";
constexpr auto path = "/org/senemos/Uke/Motion1";
constexpr auto interface = "org.senemos.Uke.Motion1";
qulonglong nowUsec() {
    timespec time{};
    if (clock_gettime(CLOCK_BOOTTIME,&time)!=0) qFatal("CLOCK_BOOTTIME is required");
    return qulonglong(time.tv_sec)*1000000+qulonglong(time.tv_nsec)/1000;
}

class Provider final : public QDBusVirtualObject {
public:
    const QString connectionName=QUuid::createUuid().toString(QUuid::Id128);
    QDBusConnection bus=QDBusConnection::connectToBus(QDBusConnection::SessionBus,connectionName);
    QList<QDBusMessage> claims;
    QStringList releases,disappeared;
    Provider() {
        if(!bus.isConnected()||!bus.registerService(QLatin1String(service))||
           !bus.registerVirtualObject(QLatin1String(path),this))qFatal("Cannot register private posture fixture");
        auto *watcher=new QDBusServiceWatcher(this);
        watcher->setConnection(bus);watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        QObject::connect(watcher,&QDBusServiceWatcher::serviceUnregistered,this,[this](const QString &name){disappeared.append(name);});
        m_watcher=watcher;
    }
    ~Provider() override {
        bus.unregisterService(QLatin1String(service));
        bus.unregisterObject(QLatin1String(path));
        QDBusConnection::disconnectFromBus(connectionName);
    }
    QString introspect(const QString &)const override {return {};}
    bool handleMessage(const QDBusMessage &message,const QDBusConnection &connection)override {
        if(message.interface()!=QLatin1String(interface))return false;
        if(message.member()==QLatin1String("Claim")){
            message.setDelayedReply(true);claims.append(message);m_watcher->addWatchedService(message.service());return true;
        }
        if(message.member()==QLatin1String("Release")){
            releases.append(message.service());
            if(message.isReplyRequired())connection.send(message.createReply());
            return true;
        }
        return false;
    }
    void reply(int index,bool known,double angle,std::optional<qulonglong> timestamp={}) {
        const QVariantList values{known,angle,QVariant::fromValue(timestamp.value_or(nowUsec()))};
        if(!bus.send(claims.at(index).createReply(values)))qFatal("Cannot send private posture claim reply");
    }
    void change(const QString &destination,bool known,double angle,std::optional<qulonglong> timestamp={}) {
        auto message=QDBusMessage::createTargetedSignal(destination,QLatin1String(path),QLatin1String(interface),QStringLiteral("PostureChanged"));
        message.setArguments({known,angle,QVariant::fromValue(timestamp.value_or(nowUsec()))});
        if(!bus.send(message))qFatal("Cannot send private posture signal");
    }
private:
    QDBusServiceWatcher *m_watcher=nullptr;
};
}

class TabletPostureMonitorTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        QCOMPARE(qgetenv("PLASMA_KEYBOARD_TEST_ISOLATED_BUS"),QByteArray("1"));
        QCOMPARE(qgetenv("DBUS_SYSTEM_BUS_ADDRESS"),qgetenv("DBUS_SESSION_BUS_ADDRESS"));
        QCOMPARE(qgetenv("UKE_POSTURE_SYSTEM_POLICY"),QByteArray("1"));
    }
    void demandAndReleaseLeaveNoLease() {
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        QCoreApplication::processEvents();QVERIFY(provider.claims.isEmpty());
        monitor.setRequested(true);monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const auto caller=provider.claims.first().service();provider.reply(0,true,12.0);
        QTRY_COMPARE(observations.size(),1);QCOMPARE(observations.last().at(0).toBool(),true);QCOMPARE(observations.last().at(1).toDouble(),12.0);
        monitor.setRequested(false);QCOMPARE(observations.size(),2);QCOMPARE(observations.last().at(0).toBool(),false);
        QTRY_VERIFY(provider.disappeared.contains(caller)||provider.releases.contains(caller));
        QTest::qWait(30);QCOMPARE(provider.claims.size(),1);QVERIFY(!monitor.requested());
    }
    void lateInitialReplyCannotUndoUnknownSignal() {
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const auto caller=provider.claims.first().service();
        const auto initial=nowUsec();provider.change(caller,false,0,initial+1);provider.reply(0,true,0,initial);
        QTRY_VERIFY(monitor.findChildren<QDBusPendingCallWatcher*>().isEmpty());QVERIFY(observations.isEmpty());
        provider.change(caller,true,20);QTRY_COMPARE(observations.size(),1);QVERIFY(observations.last().at(0).toBool());
    }
    void unrelatedPeerCannotForgeAReply_data() {
        QTest::addColumn<bool>("error");QTest::newRow("return")<<false;QTest::newRow("error")<<true;
    }
    void unrelatedPeerCannotForgeAReply() {
        QFETCH(bool,error);
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const QString name=QUuid::createUuid().toString(QUuid::Id128);
        auto forger=QDBusConnection::connectToBus(QDBusConnection::SessionBus,name);
        const auto cleanup=qScopeGuard([&]{QDBusConnection::disconnectFromBus(name);});
        QVERIFY(forger.isConnected());QVERIFY(forger.baseService()!=provider.bus.baseService());
        const auto original=provider.claims.first();
        const auto forged=error?original.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),QStringLiteral("Forged fixture reply")):
            original.createReply(QVariantList{true,0.0,QVariant::fromValue(nowUsec())});
        QVERIFY(forger.send(forged));
        auto fence=QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),QStringLiteral("/org/freedesktop/DBus"),QStringLiteral("org.freedesktop.DBus"),QStringLiteral("ListNames"));
        auto pending=forger.asyncCall(fence);QTRY_VERIFY(pending.isFinished());QCoreApplication::processEvents();
        QVERIFY(observations.isEmpty());
        provider.reply(0,true,0);QTRY_COMPARE(observations.size(),1);QVERIFY(observations.last().at(0).toBool());
    }
    void releasedClaimAndOldConnectionCannotEnableANewLease() {
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const auto oldCaller=provider.claims.first().service();
        monitor.setRequested(false);monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),2);
        const auto newCaller=provider.claims.last().service();QVERIFY(newCaller!=oldCaller);
        provider.reply(0,true,0);provider.change(oldCaller,true,0);provider.reply(1,false,0);
        QTRY_VERIFY(monitor.findChildren<QDBusPendingCallWatcher*>().isEmpty());QVERIFY(observations.isEmpty());
        provider.change(newCaller,true,0);QTRY_COMPARE(observations.size(),1);
    }
    void timestamplessUnknownPreservesTheReplayBarrier() {
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const auto caller=provider.claims.first().service();const auto initial=nowUsec();
        provider.change(caller,true,0,initial);provider.change(caller,false,0,qulonglong(0));provider.reply(0,true,0,initial);
        QTRY_VERIFY(monitor.findChildren<QDBusPendingCallWatcher*>().isEmpty());QVERIFY(observations.isEmpty());
        provider.change(caller,true,0,initial);QTest::qWait(30);QVERIFY(observations.isEmpty());
        provider.change(caller,true,0);QTRY_COMPARE(observations.size(),1);
        provider.change(caller,false,0,qulonglong(0));QTRY_COMPARE(observations.size(),2);QVERIFY(!observations.last().at(0).toBool());
    }
    void signalTriggeredReleaseCannotPoisonTheNextLease() {
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        bool renewed=false;
        connect(&monitor,&TabletPostureMonitor::observation,this,[&](bool known,double){
            if(!known||renewed)return;
            renewed=true;monitor.setRequested(false);monitor.setRequested(true);
        });
        monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const auto sample=nowUsec();provider.change(provider.claims.first().service(),true,0,sample);provider.reply(0,true,0,sample);
        QTRY_COMPARE(provider.claims.size(),2);QCOMPARE(observations.size(),2);
        // A still-fresh physical sample is valid in a new, independent lease.
        provider.reply(1,true,0,sample);QTRY_COMPARE(observations.size(),3);QVERIFY(observations.last().at(0).toBool());
    }
    void providerReplacementRevokesAndPinsTheNewOwner() {
        Provider first;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        monitor.setRequested(true);QTRY_COMPARE(first.claims.size(),1);first.reply(0,true,0);QTRY_COMPARE(observations.size(),1);
        first.bus.unregisterService(QLatin1String(service));QTRY_COMPARE(observations.size(),2);QVERIFY(!observations.last().at(0).toBool());
        Provider second;QTRY_COMPARE(second.claims.size(),1);
        const auto caller=second.claims.first().service();second.reply(0,false,0);
        QTRY_VERIFY(monitor.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        first.change(caller,true,0);QTest::qWait(30);QCOMPARE(observations.size(),2);
        second.change(caller,true,0);QTRY_COMPARE(observations.size(),3);QVERIFY(observations.last().at(0).toBool());
        // First's destructor must not unregister second's name (D-Bus ownership).
    }
    void staleInvalidAndOutOfOrderSamplesCannotRestoreKnown() {
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const auto caller=provider.claims.first().service();provider.reply(0,true,0);QTRY_COMPARE(observations.size(),1);
        provider.change(caller,true,0,nowUsec()-3000000);QTRY_COMPARE(observations.size(),2);QVERIFY(!observations.last().at(0).toBool());
        const auto fresh=nowUsec();provider.change(caller,true,5,fresh);QTRY_COMPARE(observations.size(),3);
        provider.change(caller,false,0,fresh+1);QTRY_COMPARE(observations.size(),4);
        provider.change(caller,true,0,fresh);QTest::qWait(30);QCOMPARE(observations.size(),4);
        provider.change(caller,true,0,nowUsec());QTRY_COMPARE(observations.size(),5);
        provider.change(caller,true,std::numeric_limits<double>::quiet_NaN());QTRY_COMPARE(observations.size(),6);QVERIFY(!observations.last().at(0).toBool());
        provider.change(caller,true,0);QTRY_COMPARE(observations.size(),7);
        provider.change(caller,true,0,nowUsec()+5000000);QTRY_COMPARE(observations.size(),8);QVERIFY(!observations.last().at(0).toBool());
    }
    void rejectedClaimRetriesOnlyAfterExplicitRequest() {
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const auto caller=provider.claims.first().service();
        provider.bus.send(provider.claims.first().createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),QStringLiteral("Fixture refusal")));
        QTRY_VERIFY(monitor.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        provider.change(caller,true,0);QTest::qWait(30);QVERIFY(observations.isEmpty());QCOMPARE(provider.claims.size(),1);
        monitor.retry();QTRY_COMPARE(provider.claims.size(),2);provider.reply(1,true,0);QTRY_COMPARE(observations.size(),1);
    }
    void sleepReleasesAndResumeRequiresANewSample() {
        Provider provider;TabletPostureMonitor monitor;QSignalSpy observations(&monitor,&TabletPostureMonitor::observation);
        auto system=QDBusConnection::sessionBus();QVERIFY(system.registerService(QStringLiteral("org.freedesktop.login1")));
        monitor.setRequested(true);QTRY_COMPARE(provider.claims.size(),1);provider.reply(0,true,0);QTRY_COMPARE(observations.size(),1);
        auto sleep=QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/login1"),QStringLiteral("org.freedesktop.login1.Manager"),QStringLiteral("PrepareForSleep"));
        sleep<<true;system.send(sleep);QTRY_COMPARE(observations.size(),2);QVERIFY(!observations.last().at(0).toBool());
        auto awake=QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/login1"),QStringLiteral("org.freedesktop.login1.Manager"),QStringLiteral("PrepareForSleep"));
        awake<<false;system.send(awake);QTRY_COMPARE(provider.claims.size(),2);
        provider.reply(1,false,0);QTRY_VERIFY(monitor.findChildren<QDBusPendingCallWatcher*>().isEmpty());QCOMPARE(observations.size(),2);
        provider.change(provider.claims.last().service(),true,0);QTRY_COMPARE(observations.size(),3);
        system.unregisterService(QStringLiteral("org.freedesktop.login1"));
    }
    void destructionReleasesAnOutstandingClaim() {
        Provider provider;
        auto monitor=std::make_unique<TabletPostureMonitor>();monitor->setRequested(true);QTRY_COMPARE(provider.claims.size(),1);
        const auto caller=provider.claims.first().service();monitor.reset();
        QTRY_VERIFY(provider.disappeared.contains(caller)||provider.releases.contains(caller));
        provider.reply(0,true,0);QCoreApplication::processEvents();
    }
};
QTEST_GUILESS_MAIN(TabletPostureMonitorTest)
#include "tabletposturemonitortest.moc"
