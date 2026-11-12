// SPDX-License-Identifier: GPL-2.0-or-later
#include "touchpadmotion.h"
#include "pixelshift.h"
#include <QtTest>
#include <limits>

class LaptopInteractionTest final : public QObject {
    Q_OBJECT
    static double travel(double speed,int rate) {
        UkeKeyboard::TouchpadMotion motion;motion.reset(0);double total=0;
        for(int i=1;i<=rate;++i)total+=motion.map(QPointF(speed/rate,0),qint64(i)*1000000/rate).x();
        return total;
    }
private Q_SLOTS:
    void precisionHasNoDeadZone() {
        UkeKeyboard::TouchpadMotion motion;motion.reset(0);
        const auto delta=motion.map(QPointF(0.0005,-0.00025),8333);
        QCOMPARE(delta,QPointF(0.004,-0.002));
    }
    void fastSwipesCrossTheDisplaySooner() {
        const double slowGain=travel(5,120)/5,fastGain=travel(180,120)/180;
        QCOMPARE(slowGain,8.0);QVERIFY(fastGain>slowGain*4);QVERIFY(fastGain<=40);
    }
    void gainDependsOnTimeRatherThanEventRate() {
        const double reference=travel(90,120);
        for(int rate:{60,90,240})QVERIFY(std::abs(travel(90,rate)/reference-1)<0.05);
    }
    void liftingPausingAndReversingRestorePrecision() {
        UkeKeyboard::TouchpadMotion motion;motion.reset(0);
        for(int i=1;i<=60;++i)motion.map(QPointF(2,0),i*8333);
        auto precise=motion.map(QPointF(0.001,0),61*8333);QCOMPARE(precise,QPointF(0.008,0));
        motion.reset(62*8333);precise=motion.map(QPointF(-0.001,0),63*8333);QCOMPARE(precise,QPointF(-0.008,0));
        for(int i=64;i<=100;++i)motion.map(QPointF(2,0),i*8333);
        precise=motion.map(QPointF(-0.001,0),101*8333);QCOMPARE(precise,QPointF(-0.008,0));
        precise=motion.map(QPointF(0.001,0),2000000);QCOMPARE(precise,QPointF(0.008,0));
    }
    void flatProfileAndInvalidInput() {
        UkeKeyboard::TouchpadMotion motion;motion.reset(0);
        QCOMPARE(motion.map(QPointF(20,-3),8333,false),QPointF(160,-24));
        QVERIFY(motion.map(QPointF(std::numeric_limits<double>::infinity(),1),16000).isNull());
        QVERIFY(motion.map(QPointF(std::numeric_limits<double>::quiet_NaN(),0),24000).isNull());
    }
    void pixelPatternStaysWithinOnePhysicalPixel() {
        UkeKeyboard::PixelShift shift;QVERIFY(!shift.step());shift.setActive(true);
        QList<QPoint> seen;
        for(int i=0;i<9;++i){QVERIFY(shift.step());const auto p=shift.offset();QVERIFY(std::abs(p.x())<=1);QVERIFY(std::abs(p.y())<=1);QVERIFY(!seen.contains(p));seen.append(p);}
        QCOMPARE(shift.offset(),QPoint());
    }
    void busyInteractionDefersOneStepAndExitResetsIt() {
        UkeKeyboard::PixelShift shift;shift.setActive(true);shift.setBusy(true);
        for(int i=0;i<5;++i)QVERIFY(!shift.step());
        QVERIFY(shift.pending());QCOMPARE(shift.offset(),QPoint());
        QVERIFY(shift.setBusy(false));QCOMPARE(shift.offset(),QPoint(-1,-1));QVERIFY(!shift.pending());
        shift.setBusy(true);shift.step();QVERIFY(shift.setActive(false));QVERIFY(!shift.pending());QCOMPARE(shift.offset(),QPoint());
        shift.setBusy(false);QVERIFY(!shift.step());
    }
};
QTEST_GUILESS_MAIN(LaptopInteractionTest)
#include "laptopinteractiontest.moc"
