// SPDX-License-Identifier: GPL-2.0-or-later
#include "pointerconfinement.h"
#include <QtTest>

class PointerConfinementTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void outputInteriorIsStable_data() {
        QTest::addColumn<QRectF>("area");
        QTest::newRow("actual-dp")<<QRectF(0,0,2560,1440);
        QTest::newRow("negative")<<QRectF(-2560,-1440,2560,1440);
        QTest::newRow("fractional-scale")<<QRectF(1280.25,-720.5,1024.6,768.4);
        QTest::newRow("portrait")<<QRectF(0,1220.5,800,1280);
    }
    void outputInteriorIsStable() {
        QFETCH(QRectF,area);
        const QList<QRectF> outputs{area};
        for(const auto &point:{area.topLeft()-QPointF(100,100),area.bottomRight()+QPointF(100,100),area.center(),area.topRight(),area.bottomLeft()}) {
            const auto target=UkeKeyboard::externalPointerPosition(outputs,point);
            QVERIFY(std::floor(target.x())>=area.left());QVERIFY(std::floor(target.x())<area.right());
            QVERIFY(std::floor(target.y())>=area.top());QVERIFY(std::floor(target.y())<area.bottom());
            QCOMPARE(UkeKeyboard::externalPointerPosition(outputs,target),target);
            const QPointF native(std::clamp(target.x(),area.left(),area.right()-1.0),std::clamp(target.y(),area.top(),area.bottom()-1.0));
            QCOMPARE(UkeKeyboard::externalPointerPosition(outputs,native),native);
        }
        const QPointF fractional=area.center()+QPointF(0.001,0.002);
        QCOMPARE(UkeKeyboard::externalPointerPosition(outputs,fractional),fractional);
    }
    void nearestExternalAndEmpty() {
        const QPointF point(2000,500);
        QCOMPARE(UkeKeyboard::externalPointerPosition({},point),point);
        QCOMPARE(UkeKeyboard::externalPointerPosition({QRectF(0,0,100,100),QRectF(2100,0,100,1000)},point),QPointF(2100,500));
    }
    void correctionIsDeferredAndCoalesced() {
        int calls=0,depth=0,maxDepth=0;
        UkeKeyboard::DeferredPointerCorrection *self=nullptr;
        UkeKeyboard::DeferredPointerCorrection correction([&]{
            ++depth;maxDepth=std::max(maxDepth,depth);++calls;
            if(calls==1)for(int i=0;i<100;++i)self->request();
            --depth;
        });self=&correction;
        for(int i=0;i<100;++i)correction.request();
        QCOMPARE(calls,0);QTRY_COMPARE(calls,2);QCOMPARE(maxDepth,1);
    }
    void unloadedContextCancelsCorrection() {
        int calls=0;
        auto *correction=new UkeKeyboard::DeferredPointerCorrection([&]{++calls;});
        correction->request();delete correction;
        QCoreApplication::processEvents();QCOMPARE(calls,0);
    }
};
QTEST_GUILESS_MAIN(PointerConfinementTest)
#include "pointerconfinementtest.moc"
