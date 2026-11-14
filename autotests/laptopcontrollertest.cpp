// SPDX-License-Identifier: GPL-2.0-or-later
#include "laptopcontroller.h"
#include "protocol.h"
#include "tabletposturemonitor.h"
#include <QtTest>
#include <QSignalSpy>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVirtualObject>
#include <QTemporaryDir>
#include <QScopeGuard>

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
    int staticBegins=0,dynamicBegins=0,staticEnds=0,dynamicEnds=0,profiles=0,attaches=0;
    QString lastReceiver;
    bool deferCapabilities=false;
    QDBusMessage capabilitiesRequest;
    int capabilityQueries=0;
    QStringList inputs;
    QList<bool> inputReplies;
    QList<QPointF> motions;
    QList<QPair<uint,bool>> keys;
    QDBusConnection server{QDBusConnection::connectToBus(QDBusConnection::SessionBus,QStringLiteral("fixture-backend"))};
    void reply(bool accepted=true) { server.send(request.createReply(QVariantList{accepted})); }
public Q_SLOTS:
    QVariantMap Capabilities() {
        ++capabilityQueries;
        if(deferCapabilities){capabilitiesRequest=message();setDelayedReply(true);return {};}
        return {{QStringLiteral("usbKeyboardPresent"),false}};
    }
    bool Attach() {++attaches;return true;}
    bool BeginStatic(const QString &) {request=message();setDelayedReply(true);++staticBegins;return false;}
    bool BeginDynamic(const QString &) {lastReceiver=connection().baseService();request=message();setDelayedReply(true);++dynamicBegins;return false;}
    void EndStatic() {++staticEnds;}
    void EndDynamic() {++dynamicEnds;}
    bool Profile(const QString &) {++profiles;return true;}
    void ReleaseAll() {inputs.append(QStringLiteral("release"));inputReplies.append(message().isReplyRequired());}
    void Motion(double x,double y) {inputs.append(QStringLiteral("motion"));inputReplies.append(message().isReplyRequired());motions.append(QPointF(x,y));}
    void Button(uint,bool pressed) {inputs.append(pressed?QStringLiteral("press"):QStringLiteral("release-button"));inputReplies.append(message().isReplyRequired());}
    void Key(uint code,bool pressed) {keys.append({code,pressed});}
};
class PluginManager final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.Plugins")
    Q_PROPERTY(QStringList LoadedPlugins READ loadedPlugins)
public:
    QStringList operations;
    QStringList loadedPlugins()const{return {QStringLiteral("unrelated-plugin"),QStringLiteral("uke_keyboard_backend"),QStringLiteral("uke_keyboard_backend_previous")};}
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
    void eligible(LaptopController &controller,bool dynamic) {
        controller.m_backendOwner=m_backend.server.baseService();
        controller.m_ready=true;controller.m_available=true;controller.m_locked=false;
        controller.m_lidClosed=false;controller.m_tabletName=QStringLiteral("synthetic-panel");
        controller.m_usbKeyboardKnown=true;controller.m_usbKeyboardPresent=false;
        controller.m_posture.update(0.0);
        controller.m_options[QStringLiteral("presentation")]=dynamic?QStringLiteral("dynamic"):QStringLiteral("static");
        controller.m_options[QStringLiteral("staticAutoShow")]=false;
    }
private Q_SLOTS:
    void modifiersFollowPhysicalPressAndRelease() {
        LaptopController controller;eligible(controller,false);
        for(uint modifier:{29U,42U,54U,56U,97U,100U,125U,126U}){
            m_backend.keys.clear();
            for(int tap=0;tap<2;++tap){
                controller.key(modifier,true);controller.key(modifier,true);
                QVERIFY(controller.modifierActive());
                controller.key(modifier,false);controller.key(modifier,false);
                QVERIFY(!controller.modifierActive());QVERIFY(!controller.latched(modifier));
            }
            const QList<QPair<uint,bool>> expected{{modifier,true},{modifier,false},{modifier,true},{modifier,false}};
            QTRY_COMPARE(m_backend.keys,expected);
            m_backend.keys.clear();
            controller.key(modifier,true);controller.key(30,true);controller.key(30,false);
            QVERIFY(controller.modifierActive());controller.key(modifier,false);
            QVERIFY(!controller.modifierActive());QVERIFY(controller.m_held.isEmpty());
            const QList<QPair<uint,bool>> chord{{modifier,true},{30,true},{30,false},{modifier,false}};
            QTRY_COMPARE(m_backend.keys,chord);
        }
        m_backend.keys.clear();controller.key(125,true);controller.m_locked=true;controller.key(125,false);
        QVERIFY(controller.m_held.isEmpty());QVERIFY(!controller.modifierActive());
        const QList<QPair<uint,bool>> released{{125,true},{125,false}};QTRY_COMPARE(m_backend.keys,released);
    }
    void navigationClusterIsSeparateAndUsesInvertedTArrows() {
        LaptopController controller;
        controller.m_options[QStringLiteral("arrowRow")]=true;
        controller.m_options[QStringLiteral("navigationRow")]=false;
        controller.m_options[QStringLiteral("systemRow")]=false;
        const auto navigation=controller.navigationRows();QCOMPARE(navigation.size(),4);
        const auto codes=[](const QVariant& row){QList<uint> result;for(const auto& key:row.toList())result.append(key.toMap().value(QStringLiteral("code")).toUInt());return result;};
        QCOMPARE(codes(navigation.at(0)),QList<uint>({110,102,104}));
        QCOMPARE(codes(navigation.at(1)),QList<uint>({111,107,109}));
        QCOMPARE(codes(navigation.at(2)),QList<uint>({0,103,0}));
        QCOMPARE(codes(navigation.at(3)),QList<uint>({105,108,106}));
        for(const auto& row:controller.rows())for(uint code:codes(row))QVERIFY(!QList<uint>({102,103,104,105,106,107,108,109,110,111}).contains(code));
        controller.m_options[QStringLiteral("arrowRow")]=false;QVERIFY(controller.navigationRows().isEmpty());
        controller.m_options[QStringLiteral("navigationRow")]=true;QCOMPARE(controller.navigationRows().size(),2);
    }
    void tiltRevokesInputAndReopensOnlyBelowForty() {
        LaptopController controller;eligible(controller,false);
        controller.m_options[QStringLiteral("staticAutoShow")]=true;
        const int before=m_backend.staticBegins;
        controller.postureObserved(true,45.0);controller.updateEligibility();
        QTRY_COMPARE(m_backend.staticBegins,before+1);m_backend.reply();QTRY_VERIFY(controller.staticActive());
        controller.m_held.insert(30);controller.m_pointerButtons.insert(272);
        controller.postureObserved(true,45.001);
        QVERIFY(!controller.active());QVERIFY(controller.m_held.isEmpty());QVERIFY(controller.m_pointerButtons.isEmpty());
        QVERIFY(controller.pixelShift().isNull());QVERIFY(!controller.m_pixelShiftTimer.isActive());
        controller.postureObserved(true,44.0);controller.postureObserved(true,40.0);
        QCoreApplication::processEvents();QCOMPARE(m_backend.staticBegins,before+1);
        controller.postureObserved(true,39.999);QTRY_COMPARE(m_backend.staticBegins,before+2);
        m_backend.reply();QTRY_VERIFY(controller.staticActive());controller.leave();
    }
    void unknownPostureBlocksEntryAndCancelsALateReply() {
        LaptopController controller;eligible(controller,false);
        controller.m_options[QStringLiteral("staticAutoShow")]=true;
        controller.postureObserved(false,0);QVERIFY(!controller.available());QVERIFY(controller.canRequest());
        const int before=m_backend.staticBegins;
        controller.updateEligibility();QCoreApplication::processEvents();QCOMPARE(m_backend.staticBegins,before);
        controller.postureObserved(true,0);QTRY_COMPARE(m_backend.staticBegins,before+1);
        controller.m_held.insert(30);controller.m_pointerButtons.insert(272);
        controller.postureObserved(false,0);QVERIFY(!controller.m_starting);
        QVERIFY(controller.m_held.isEmpty());QVERIFY(controller.m_pointerButtons.isEmpty());
        m_backend.reply();QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());QVERIFY(!controller.active());
        controller.postureObserved(true,0);QTRY_COMPARE(m_backend.staticBegins,before+2);
        m_backend.reply();QTRY_VERIFY(controller.staticActive());controller.leave();
    }
    void manualStaticRequestWaitsForPostureAndNormalStaysDismissed() {
        LaptopController controller;eligible(controller,false);controller.postureObserved(false,0);
        const int before=m_backend.staticBegins;
        controller.enter();QVERIFY(controller.m_manualStaticRequest);
        QCoreApplication::processEvents();QCOMPARE(m_backend.staticBegins,before);
        controller.postureObserved(true,0);QTRY_COMPARE(m_backend.staticBegins,before+1);
        m_backend.reply();QTRY_VERIFY(controller.staticActive());
        controller.m_options[QStringLiteral("staticAutoShow")]=true;controller.leave();
        controller.postureObserved(false,0);controller.postureObserved(true,0);controller.updateEligibility();
        QCoreApplication::processEvents();QCOMPARE(m_backend.staticBegins,before+1);
        QVERIFY(controller.m_staticAutoSuppressed);QVERIFY(!controller.active());
        controller.enter();QTRY_COMPARE(m_backend.staticBegins,before+2);
        m_backend.reply();QTRY_VERIFY(controller.staticActive());controller.leave();
    }
    void postureDemandFollowsStaticEligibilityAndManualDismissal() {
        LaptopController controller;eligible(controller,false);
        controller.m_postureMonitor=std::make_unique<TabletPostureMonitor>();
        controller.m_options[QStringLiteral("staticAutoShow")]=true;
        controller.updateEligibility();QVERIFY(controller.m_postureMonitor->requested());
        controller.leave();QVERIFY(!controller.m_postureMonitor->requested());
        controller.m_lidClosed=true;controller.updateEligibility();
        controller.m_lidClosed=false;controller.updateEligibility();QVERIFY(controller.m_postureMonitor->requested());
        controller.usbKeyboardChanged(true);QVERIFY(!controller.m_postureMonitor->requested());
        controller.usbKeyboardChanged(false);QVERIFY(controller.m_postureMonitor->requested());
        controller.m_locked=true;controller.updateEligibility();QVERIFY(!controller.m_postureMonitor->requested());
        controller.m_locked=false;controller.updateEligibility();QVERIFY(controller.m_postureMonitor->requested());
        controller.m_available=false;controller.updateEligibility();QVERIFY(!controller.m_postureMonitor->requested());
        controller.m_available=true;controller.m_options[QStringLiteral("presentation")]=QStringLiteral("dynamic");
        controller.updateEligibility();QVERIFY(!controller.m_postureMonitor->requested());
        controller.postureObserved(false,0);QVERIFY(controller.available());
        controller.leave();
    }
    void toggleCancelsARequestWaitingForPosture() {
        LaptopController controller;eligible(controller,false);controller.postureObserved(false,0);
        QSignalSpy changes(&controller,&LaptopController::stateChanged);
        const int before=m_backend.staticBegins;
        controller.toggle();QVERIFY(controller.entryPending());
        const int waiting=changes.size();
        controller.toggle();QVERIFY(!controller.entryPending());QVERIFY(changes.size()>waiting);
        controller.postureObserved(true,0);QCoreApplication::processEvents();QCOMPARE(m_backend.staticBegins,before);
        controller.toggle();QVERIFY(controller.entryPending());QTRY_COMPARE(m_backend.staticBegins,before+1);
        controller.toggle();QVERIFY(!controller.entryPending());
        m_backend.reply();QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());QVERIFY(!controller.active());
    }
    void refusedManualEntryReleasesPostureDemand() {
        LaptopController controller;eligible(controller,false);
        controller.m_postureMonitor=std::make_unique<TabletPostureMonitor>();
        const int before=m_backend.staticBegins;
        controller.enter();QVERIFY(controller.entryPending());QVERIFY(controller.m_postureMonitor->requested());
        QTRY_COMPARE(m_backend.staticBegins,before+1);m_backend.reply(false);
        QTRY_VERIFY(!controller.entryPending());QVERIFY(!controller.active());QVERIFY(!controller.m_postureMonitor->requested());
    }
    void dynamicModeRequiresManualActionEvenWithLegacyAutoPreference() {
        LaptopController controller;eligible(controller,true);
        controller.m_options[QStringLiteral("dynamicAutoShow")]=true;
        const int before=m_backend.dynamicBegins;
        controller.textFocusChanged(true);controller.textFocusChanged(false);
        QCoreApplication::processEvents();QCOMPARE(m_backend.dynamicBegins,before);
        controller.enter();QTRY_COMPARE(m_backend.dynamicBegins,before+1);
        controller.textFocusChanged(true);controller.textFocusChanged(false);
        QVERIFY(controller.m_starting);
        m_backend.reply();QTRY_VERIFY(controller.dynamicActive());
        controller.textFocusChanged(false);QVERIFY(controller.dynamicActive());
        controller.leave();QVERIFY(!controller.active());
    }
    void pixelShiftStopsAndDefersForTouchesAndButtons() {
        LaptopController controller;eligible(controller,false);controller.m_active=true;
        controller.syncPixelShift();QVERIFY(controller.m_pixelShiftTimer.isActive());
        QCOMPARE(controller.m_pixelShiftTimer.timerType(),Qt::VeryCoarseTimer);
        controller.setTouchActive(true);controller.advancePixelShift();
        QVERIFY(controller.pixelShift().isNull());QVERIFY(controller.m_pixelShift.pending());QVERIFY(!controller.m_pixelShiftTimer.isActive());
        controller.setTouchActive(false);QCOMPARE(controller.pixelShift(),QPoint(-1,-1));QVERIFY(controller.m_pixelShiftTimer.isActive());
        controller.button(272,true);controller.advancePixelShift();QCOMPARE(controller.pixelShift(),QPoint(-1,-1));
        controller.button(272,false);QCOMPARE(controller.pixelShift(),QPoint(0,-1));
        controller.m_options[QStringLiteral("pixelShift")]=false;controller.syncPixelShift();
        QVERIFY(controller.pixelShift().isNull());QVERIFY(!controller.m_pixelShiftTimer.isActive());
        controller.m_options[QStringLiteral("pixelShift")]=true;controller.syncPixelShift();controller.leave();
        QVERIFY(controller.pixelShift().isNull());QVERIFY(!controller.m_pixelShiftTimer.isActive());
    }
    void inputMessagesKeepOrderAndDoNotRequestReplies() {
        LaptopController controller;eligible(controller,false);
        auto fence=controller.backendCall(QStringLiteral("Profile"),{QStringLiteral("keep")});
        QTRY_VERIFY(fence.isFinished());
        m_backend.inputs.clear();m_backend.inputReplies.clear();m_backend.motions.clear();
        controller.m_options[QStringLiteral("pointerSpeed")]=1.0;
        controller.button(272,true);
        for(int i=0;i<96;++i)controller.motion(0.125,-0.25);
        controller.button(272,false);controller.releaseAll();
        QTRY_COMPARE(m_backend.inputs.size(),99);
        QCOMPARE(m_backend.inputs.first(),QStringLiteral("press"));
        QCOMPARE(m_backend.inputs.at(97),QStringLiteral("release-button"));
        QCOMPARE(m_backend.inputs.last(),QStringLiteral("release"));
        QCOMPARE(m_backend.motions.size(),96);
        QPointF total;for(const auto &delta:m_backend.motions)total+=delta;
        QCOMPARE(total,QPointF(12,-24));
        for(bool required:m_backend.inputReplies)QVERIFY(!required);
    }
    void completedUsbDetectionRestoresTheTopologyReason() {
        LaptopController controller;eligible(controller,false);
        controller.m_usbKeyboardKnown=false;controller.updateEligibility();
        const auto pending=controller.reason();QVERIFY(pending.contains(QStringLiteral("USB")));
        controller.m_config=KScreen::ConfigPtr::create();
        controller.usbKeyboardChanged(false);
        QVERIFY(controller.reason()!=pending);QVERIFY(!controller.reason().contains(QStringLiteral("USB")));
        QVERIFY(!controller.available());
    }
    void automaticStaticEntryAndManualDismissal() {
        QVERIFY(LaptopController::defaults().value(QStringLiteral("staticAutoShow")).toBool());
        LaptopController controller;eligible(controller,false);
        controller.m_options[QStringLiteral("staticAutoShow")]=true;
        const int before=m_backend.staticBegins;
        controller.updateEligibility();QTRY_COMPARE(m_backend.staticBegins,before+1);
        controller.updateEligibility();controller.updateEligibility();
        m_backend.reply();QTRY_VERIFY(controller.staticActive());
        controller.textFocusChanged(true);controller.textFocusChanged(false);QVERIFY(controller.staticActive());
        controller.leave();controller.updateEligibility();
        QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        QCOMPARE(m_backend.staticBegins,before+1);QVERIFY(!controller.active());
        controller.m_lidClosed=true;controller.updateEligibility();
        controller.m_lidClosed=false;controller.updateEligibility();
        QTRY_COMPARE(m_backend.staticBegins,before+2);
        m_backend.reply();QTRY_VERIFY(controller.staticActive());controller.leave();
    }
    void automaticEntryRespectsUsbAndRearmsAfterRemoval() {
        LaptopController controller;eligible(controller,false);
        controller.m_options[QStringLiteral("staticAutoShow")]=true;
        const int before=m_backend.staticBegins;
        controller.usbKeyboardChanged(true);QCoreApplication::processEvents();QCOMPARE(m_backend.staticBegins,before);
        controller.usbKeyboardChanged(false);QTRY_COMPARE(m_backend.staticBegins,before+1);
        controller.usbKeyboardChanged(true);QVERIFY(!controller.m_starting);
        m_backend.reply();QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());QVERIFY(!controller.active());
        controller.usbKeyboardChanged(false);QTRY_COMPARE(m_backend.staticBegins,before+2);
        m_backend.reply();QTRY_VERIFY(controller.staticActive());controller.leave();
    }
    void queuedAutomaticEntryCanBeDisabledOrDismissed() {
        LaptopController controller;eligible(controller,false);
        controller.m_options[QStringLiteral("staticAutoShow")]=true;
        const int before=m_backend.staticBegins;
        controller.updateEligibility();controller.leave();
        QCoreApplication::processEvents();QCOMPARE(m_backend.staticBegins,before);
        controller.m_staticAutoSuppressed=false;controller.m_staticAutoAttempted=false;
        controller.updateEligibility();controller.m_options[QStringLiteral("staticAutoShow")]=false;
        QCoreApplication::processEvents();QCOMPARE(m_backend.staticBegins,before);
    }
    void pendingNativeRevocationWinsOverEntryReply_data() {
        QTest::addColumn<bool>("dynamic");QTest::newRow("static")<<false;QTest::newRow("dynamic")<<true;
    }
    void pendingNativeRevocationWinsOverEntryReply() {
        QFETCH(bool,dynamic);LaptopController controller;eligible(controller,dynamic);
        const int before=m_backend.staticBegins+m_backend.dynamicBegins;
        controller.enter();QTRY_COMPARE(m_backend.staticBegins+m_backend.dynamicBegins,before+1);
        if(dynamic)controller.backendDynamic(false);else controller.backendStatic(false);
        QVERIFY(!controller.m_starting);
        m_backend.reply();QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());QVERIFY(!controller.active());
    }
    void usbKeyboardBlocksOnlyStatic() {
        LaptopController controller;eligible(controller,false);
        controller.usbKeyboardChanged(true);QVERIFY(!controller.available());
        QVERIFY(controller.reason().contains(QStringLiteral("USB")));
        const int before=m_backend.staticBegins;controller.enter();QCOMPARE(m_backend.staticBegins,before);
        controller.m_options[QStringLiteral("presentation")]=QStringLiteral("dynamic");
        QVERIFY(controller.available());
        controller.m_usbKeyboardKnown=false;QVERIFY(controller.available());
        controller.m_options[QStringLiteral("presentation")]=QStringLiteral("static");
        QVERIFY(!controller.available());
        controller.usbKeyboardChanged(false);QVERIFY(controller.available());QVERIFY(!controller.active());
    }
    void usbHotplugCancelsPendingStaticEntry() {
        LaptopController controller;eligible(controller,false);
        const int begins=m_backend.staticBegins,ends=m_backend.staticEnds;
        controller.enter();QTRY_COMPARE(m_backend.staticBegins,begins+1);
        controller.usbKeyboardChanged(true);QVERIFY(!controller.m_starting);
        QTRY_COMPARE(m_backend.staticEnds,ends+1);
        m_backend.reply();QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        QVERIFY(!controller.active());QVERIFY(!controller.available());
    }
    void usbHotplugLeavesActiveStaticButKeepsDynamic() {
        LaptopController controller;eligible(controller,false);controller.m_active=true;
        const int ends=m_backend.staticEnds;
        controller.usbKeyboardChanged(true);QVERIFY(!controller.active());QTRY_COMPARE(m_backend.staticEnds,ends+1);
        eligible(controller,true);controller.m_active=true;controller.m_dynamic=true;
        controller.usbKeyboardChanged(true);QVERIFY(controller.dynamicActive());QVERIFY(controller.available());
        controller.leave();
    }
    void usbSignalWinsOverOldCapabilityReply() {
        LaptopController controller;eligible(controller,false);
        m_backend.deferCapabilities=true;
        const auto restore=qScopeGuard([&]{m_backend.deferCapabilities=false;});
        const int before=m_backend.capabilityQueries;
        controller.queryUsbKeyboard();QTRY_COMPARE(m_backend.capabilityQueries,before+1);
        controller.usbKeyboardChanged(true);
        m_backend.server.send(m_backend.capabilitiesRequest.createReply(QVariantList{QVariantMap{{QStringLiteral("usbKeyboardPresent"),false}}}));
        QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
        QVERIFY(controller.m_usbKeyboardPresent);QVERIFY(!controller.available());
    }
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
        QVERIFY(m_backend.server.registerObject(QStringLiteral("/Plugins"),&m_plugins,QDBusConnection::ExportAllSlots|QDBusConnection::ExportAllProperties));
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
        QTRY_COMPARE(m_plugins.operations.size(),3);
        const QStringList expected{QStringLiteral("unload:uke_keyboard_backend"),QStringLiteral("unload:uke_keyboard_backend_previous"),QStringLiteral("load:")+QLatin1String(UkeKeyboard::pluginId)};
        QCOMPARE(m_plugins.operations,expected);
        QTRY_VERIFY(controller.findChildren<QDBusPendingCallWatcher*>().isEmpty());
    }
    void refusesABackendOutsideItsCompositorParent() {
        LaptopController controller;
        const int before=m_backend.attaches;
        controller.connectBackend();
        QVERIFY(!controller.m_ready);
        QVERIFY(controller.m_backendOwner.isEmpty());
        QCOMPARE(m_backend.attaches,before);
    }
    void nativeCallsStayOnTheAuthenticatedUniqueConnection() {
        LaptopController controller;eligible(controller,true);
        auto imposter=QDBusConnection::connectToBus(QDBusConnection::SessionBus,QStringLiteral("fixture-imposter"));
        QVERIFY(imposter.isConnected());
        QVERIFY(m_backend.server.unregisterService(QLatin1String(UkeKeyboard::service)));
        const auto restore=qScopeGuard([&]{imposter.unregisterService(QLatin1String(UkeKeyboard::service));m_backend.server.registerService(QLatin1String(UkeKeyboard::service));});
        QVERIFY(imposter.registerService(QLatin1String(UkeKeyboard::service)));
        QVERIFY(imposter.registerObject(QLatin1String(UkeKeyboard::path),&m_backend,QDBusConnection::ExportAllSlots));
        const int before=m_backend.dynamicBegins;
        controller.enter();QTRY_COMPARE(m_backend.dynamicBegins,before+1);
        QCOMPARE(m_backend.lastReceiver,m_backend.server.baseService());
        m_backend.reply();QTRY_VERIFY(controller.dynamicActive());controller.leave();
    }
};
QTEST_MAIN(LaptopControllerTest)
#include "laptopcontrollertest.moc"
