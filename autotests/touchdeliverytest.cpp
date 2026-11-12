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
    Q_PROPERTY(QVariantMap options READ options CONSTANT)
    Q_PROPERTY(bool typing READ typing CONSTANT)
    Q_PROPERTY(bool modifierActive READ typing CONSTANT)
public:
    QList<QPointF> motions;
    double pixelsPerMm()const{return 3.0;}
    bool typing()const{return false;}
    QVariantMap options()const{return {{QStringLiteral("palmGuard"),false},{QStringLiteral("leftButton"),false},{QStringLiteral("middleButton"),false},{QStringLiteral("rightButton"),false}};}
    Q_INVOKABLE void padMotion(double x,double y){motions.append(QPointF(x,y));}
    Q_INVOKABLE void resetPadMotion(){}
    Q_INVOKABLE void button(uint,bool){}
    Q_INVOKABLE void scroll(double,double){}
    Q_INVOKABLE void swipe(int,int,double,double){}
    Q_INVOKABLE void pinch(int,double,double,double,double){}
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
    }
};
int main(int argc,char **argv) {
    UkeKeyboard::configureTouchDelivery();
    QGuiApplication application(argc,argv);
    TouchDeliveryTest test;return QTest::qExec(&test,argc,argv);
}
#include "touchdeliverytest.moc"
