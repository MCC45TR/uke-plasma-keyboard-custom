// SPDX-License-Identifier: MIT
#include "physicalgeometry.h"
#include <QtTest>
class PhysicalGeometryTest:public QObject {
 Q_OBJECT
private Q_SLOTS:
 void scaleIndependent(){for(double scale:{1.0,2.0,2.65}){const double ppm=UkeKeyboard::unitsPerMm(3200/scale,254);QVERIFY(std::abs((14*ppm*scale/3200*254)-14)<0.000001);}}
 void invalid(){QVERIFY(!UkeKeyboard::plausibleSize({0,0}));QVERIFY(!UkeKeyboard::plausibleSize({10000,100}));QVERIFY(UkeKeyboard::plausibleSize({254,170}));QCOMPARE(UkeKeyboard::unitsPerMm(100,0),96.0/25.4);}
 void constrainedPortrait(){const auto key=UkeKeyboard::fittedKeyWidth(170,14.6,14,1,0.8);QVERIFY(key<14);QVERIFY(key>7);QVERIFY(key*14.6+13.6*0.8<=170.0001);}
};
QTEST_GUILESS_MAIN(PhysicalGeometryTest)
#include "physicalgeometrytest.moc"
