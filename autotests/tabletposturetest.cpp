// SPDX-License-Identifier: GPL-2.0-or-later
#include "tabletposture.h"
#include <QTest>
#include <limits>

class TabletPostureTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void thresholdAndHysteresis() {
        UkeKeyboard::TabletPosture posture;
        QVERIFY(!posture.known()); QVERIFY(!posture.allowed());
        QVERIFY(posture.update(45.0)); QVERIFY(posture.allowed());
        QVERIFY(posture.update(45.001)); QVERIFY(!posture.allowed());
        QVERIFY(!posture.update(44.0)); QVERIFY(!posture.allowed());
        QVERIFY(!posture.update(40.0)); QVERIFY(!posture.allowed());
        QVERIFY(posture.update(39.999)); QVERIFY(posture.allowed());
        QVERIFY(!posture.update(45.0)); QVERIFY(posture.allowed());
    }
    void missingInvalidAndFaceDownRefuseEntry() {
        UkeKeyboard::TabletPosture posture;
        posture.update(0.0); QVERIFY(posture.allowed());
        QVERIFY(posture.update({})); QVERIFY(!posture.known()); QVERIFY(!posture.allowed());
        posture.update(30.0); QVERIFY(posture.allowed());
        posture.update(90.0); QVERIFY(posture.known()); QVERIFY(!posture.allowed());
        posture.update(180.0); QVERIFY(!posture.allowed());
        posture.update({}); posture.update(43.0); QVERIFY(!posture.allowed());
        posture.update(0.0); QVERIFY(posture.allowed());
        posture.update(std::numeric_limits<double>::quiet_NaN()); QVERIFY(!posture.known());
        posture.update(std::numeric_limits<double>::infinity()); QVERIFY(!posture.known());
        posture.update(-1.0); QVERIFY(!posture.known());
        posture.update(181.0); QVERIFY(!posture.known());
        posture.reset(); posture.update(43.0); QVERIFY(posture.allowed());
    }
};
QTEST_GUILESS_MAIN(TabletPostureTest)
#include "tabletposturetest.moc"
