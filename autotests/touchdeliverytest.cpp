// SPDX-License-Identifier: GPL-2.0-or-later
#include "touchdeliverypolicy.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlComponent>
#include <QFile>
#include <QTemporaryDir>
#include <QTouchEvent>
#include <QtTest>
#include <KLocalizedQmlContext>
#include <qqml.h>
#include <qpa/qwindowsysteminterface.h>
#include <cmath>

// Recording facade only: the gesture and motion logic comes from Touchpad.qml.
class RecordingLaptop final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double pixelsPerMm READ pixelsPerMm CONSTANT)
    Q_PROPERTY(QVariantMap options READ options NOTIFY stateChanged)
    Q_PROPERTY(bool typing READ typing NOTIFY stateChanged)
    Q_PROPERTY(bool modifierActive READ modifierActive CONSTANT)
public:
    QList<QPointF> motions;
    QList<QPair<int,double>> pinches;
    QList<QPair<uint,bool>> buttons;
    bool guard=false,isTyping=false;
    double pixelsPerMm()const{return 3.0;}
    bool typing()const{return isTyping;}
    bool modifierActive()const{return false;}
    void setTyping(bool value){isTyping=value;Q_EMIT stateChanged();}
    void enableGuard(){guard=true;Q_EMIT stateChanged();}
    QVariantMap options()const{return {{QStringLiteral("palmGuard"),guard},{QStringLiteral("leftButton"),false},{QStringLiteral("middleButton"),false},{QStringLiteral("rightButton"),false}};}
    Q_INVOKABLE void padMotion(double x,double y){motions.append(QPointF(x,y));}
    Q_INVOKABLE void resetPadMotion(){}
    Q_INVOKABLE void button(uint code,bool pressed){buttons.append({code,pressed});}
    Q_INVOKABLE void scroll(double,double){}
    Q_INVOKABLE void swipe(int,int,double,double){}
    Q_INVOKABLE void pinch(int phase,double,double angle,double,double){pinches.append({phase,angle});}
Q_SIGNALS:
    void stateChanged();
};
class TouchDeliveryTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void smallMotionDoesNotWaitForRenderedFrames() {
        RecordingLaptop laptop;
        qmlRegisterSingletonInstance("UkeTouchpadTest",1,0,"Laptop",&laptop);
        QVERIFY(qmlProtectModule("UkeTouchpadTest",1));
        QQuickView window;window.resize(600,300);
        KLocalization::setupLocalizedContext(window.engine());
        QFile source(QStringLiteral(TOUCHPAD_SOURCE));QVERIFY(source.open(QIODevice::ReadOnly));
        auto data=source.readAll();
        const QByteArray bridgeImport="import org.uke.plasma.keyboard.lib as Keyboard";
        QCOMPARE(data.count(bridgeImport),1);
        data.replace(bridgeImport,"import UkeTouchpadTest 1.0 as Keyboard");
        // Only substitute the recording bridge import. Keep every gesture and
        // movement handler byte-identical, outside the app's implicit module.
        QTemporaryDir directory;QVERIFY(directory.isValid());
        QQmlComponent component(window.engine());
        component.setData(data,QUrl::fromLocalFile(directory.filePath(QStringLiteral("Touchpad.qml"))));
        QScopedPointer<QObject> object(component.create());
        auto *pad=qobject_cast<QQuickItem*>(object.data());
        QVERIFY2(pad,qPrintable(component.errorString()));
        pad->setParentItem(window.contentItem());pad->setSize(QSizeF(600,300));
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();
        const auto *device=QTest::createTouchDevice();
        ulong timestamp=1;
        const auto deliver=[&](QEventPoint::State state,QPointF position){
            QWindowSystemInterface::TouchPoint point;
            point.id=0;point.state=state;
            point.area=QRectF(QPointF(window.mapToGlobal(QPoint()))+position-QPointF(1,1),QSizeF(2,2));
            point.pressure=state==QEventPoint::State::Released?0:1;
            QWindowSystemInterface::handleTouchEvent(&window,timestamp++,device,{point});
            QWindowSystemInterface::sendWindowSystemEvents(QEventLoop::AllEvents);
        };
        deliver(QEventPoint::State::Pressed,QPointF(40,40));
        // No event-loop turn or rendered frame between these hardware updates.
        // Each quarter-pixel delta must reach the actual touchpad's output.
        for(int i=1;i<=24;++i)
            deliver(QEventPoint::State::Updated,QPointF(40+i*0.25,40));
        QCOMPARE(laptop.motions.size(),24);
        QPointF total;for(const auto &delta:laptop.motions){
            QVERIFY(std::abs(delta.x()-0.25/3.0)<0.00001);QCOMPARE(delta.y(),0.0);total+=delta;
        }
        QVERIFY(std::abs(total.x()-2.0)<0.00001);QCOMPARE(total.y(),0.0);
        deliver(QEventPoint::State::Released,QPointF(46,40));
        const auto pair=[&](QEventPoint::State firstState,QPointF first,QEventPoint::State secondState,QPointF second){
            QList<QWindowSystemInterface::TouchPoint> points;
            for(int id=0;id<2;++id){
                QWindowSystemInterface::TouchPoint point;point.id=id;
                point.state=id==0?firstState:secondState;
                point.area=QRectF(QPointF(window.mapToGlobal(QPoint()))+(id==0?first:second)-QPointF(1,1),QSizeF(2,2));
                point.pressure=point.state==QEventPoint::State::Released?0:1;points.append(point);
            }
            QWindowSystemInterface::handleTouchEvent(&window,timestamp++,device,points);
            QWindowSystemInterface::sendWindowSystemEvents(QEventLoop::AllEvents);
        };
        deliver(QEventPoint::State::Pressed,QPointF(300,150));
        pair(QEventPoint::State::Stationary,QPointF(300,150),QEventPoint::State::Pressed,QPointF(200,152));
        pair(QEventPoint::State::Stationary,QPointF(300,150),QEventPoint::State::Updated,QPointF(200,148));
        // The two-degree crossing of atan2's boundary must not start a pinch.
        QVERIFY(laptop.pinches.isEmpty());
        pair(QEventPoint::State::Stationary,QPointF(300,150),QEventPoint::State::Updated,QPointF(201,168));
        QVERIFY(!laptop.pinches.isEmpty());
        pair(QEventPoint::State::Stationary,QPointF(300,150),QEventPoint::State::Updated,QPointF(200,148));
        bool updated=false;
        for(const auto &event:laptop.pinches)if(event.first==1){updated=true;QVERIFY(std::abs(event.second)<30);}
        QVERIFY(updated);
        pair(QEventPoint::State::Released,QPointF(300,150),QEventPoint::State::Released,QPointF(200,148));
        laptop.motions.clear();
        deliver(QEventPoint::State::Pressed,QPointF(60,60));
        for(int i=1;i<=600;++i){
            if(i==200)QTest::qWait(40);
            deliver(QEventPoint::State::Updated,QPointF(60+i*0.35,60));
        }
        QCOMPARE(laptop.motions.size(),600);
        total={};for(const auto& delta:laptop.motions)total+=delta;
        QVERIFY(std::abs(total.x()-70.0)<0.0001);QCOMPARE(total.y(),0.0);
        deliver(QEventPoint::State::Released,QPointF(270,60));
        const auto clicks=laptop.buttons.size();
        laptop.enableGuard();laptop.setTyping(true);laptop.motions.clear();
        deliver(QEventPoint::State::Pressed,QPointF(40,60));
        deliver(QEventPoint::State::Updated,QPointF(50,60));QVERIFY(laptop.motions.isEmpty());
        laptop.setTyping(false);
        deliver(QEventPoint::State::Updated,QPointF(60,60));QVERIFY(laptop.motions.isEmpty());
        deliver(QEventPoint::State::Updated,QPointF(60.25,60));QCOMPARE(laptop.motions.size(),1);
        QVERIFY(std::abs(laptop.motions.last().x()-0.25/3.0)<0.00001);
        laptop.setTyping(true);
        deliver(QEventPoint::State::Updated,QPointF(70,60));QCOMPARE(laptop.motions.size(),1);
        laptop.setTyping(false);
        deliver(QEventPoint::State::Updated,QPointF(80,60));QCOMPARE(laptop.motions.size(),1);
        deliver(QEventPoint::State::Updated,QPointF(80.25,60));QCOMPARE(laptop.motions.size(),2);
        deliver(QEventPoint::State::Released,QPointF(80.25,60));
        QCOMPARE(laptop.buttons.size(),clicks);
    }
};
int main(int argc,char **argv) {
    UkeKeyboard::configureTouchDelivery();
    QGuiApplication application(argc,argv);
    TouchDeliveryTest test;return QTest::qExec(&test,argc,argv);
}
#include "touchdeliverytest.moc"
