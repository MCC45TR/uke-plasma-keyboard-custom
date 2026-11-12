// SPDX-License-Identifier: MIT
#include "nativerefreshrequest.h"
#include <QtTest>

class NativeRefreshRequestTest:public QObject {
 Q_OBJECT
private Q_SLOTS:
 void onlyTemporaryRefreshFieldsArePopulated(){
  const KWin::OutputModeline mode(QSize(3200,2136),60000);
  for(const auto policy:{KWin::VrrPolicy::Never,KWin::VrrPolicy::Always,KWin::VrrPolicy::Automatic}){
   const auto request=UkeKeyboard::refreshChanges(mode,policy);
   QVERIFY(request.currentMode.has_value());QVERIFY(*request.currentMode==mode);
   QVERIFY(request.vrrPolicy==policy);
   QVERIFY(!request.desiredMode);QVERIFY(!request.enabled);QVERIFY(!request.dpmsMode);
   QVERIFY(!request.pos);QVERIFY(!request.scale);QVERIFY(!request.scaleSetting);
   QVERIFY(!request.transform);QVERIFY(!request.manualTransform);
   QVERIFY(!request.replicationSource);QVERIFY(!request.priority);
   QVERIFY(!request.brightness);QVERIFY(!request.currentHardwareBrightness);
   QVERIFY(!request.highDynamicRange);QVERIFY(!request.wideColorGamut);
   QVERIFY(!request.iccProfilePath);QVERIFY(!request.hdrIccProfilePath);
   QVERIFY(!request.maxBitsPerColor);QVERIFY(!request.automaticBrightness);
  }
 }
};
QTEST_GUILESS_MAIN(NativeRefreshRequestTest)
#include "nativerefreshrequesttest.moc"
