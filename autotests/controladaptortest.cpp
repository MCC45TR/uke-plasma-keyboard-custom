// SPDX-License-Identifier: GPL-2.0-or-later
#include "controladaptor.h"
#include <QtTest>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusConnectionInterface>
#include <unistd.h>
class InputParent:public QObject{
 Q_OBJECT
public:
 int injected=0;
 Q_INVOKABLE void key(uint,bool){++injected;}
};
class ControlAdaptorTest:public QObject{
 Q_OBJECT
private Q_SLOTS:
 void callerMessageIsDelivered(){
  QObject target;int toggles=0;auto server=QDBusConnection::sessionBus();
  auto client=QDBusConnection::connectToBus(QDBusConnection::SessionBus,QStringLiteral("uke-control-test-client"));QVERIFY(client.isConnected());
  const auto sender=client.baseService();
  KeyboardControlAdaptor adaptor(&target,[]{return QVariantMap{};},[&]{++toggles;},[&](const QDBusConnection&bus,const QDBusMessage&request){
   if(request.service()!=sender||request.member()!=QLatin1String("toggle")||!request.arguments().isEmpty())return false;
   const auto uid=bus.interface()->serviceUid(request.service());return uid.isValid()&&uid.value()==getuid();
  });
  QVERIFY(adaptor.publish(server,QStringLiteral("/MessageProbe")));
  auto message=QDBusMessage::createMethodCall(server.baseService(),QStringLiteral("/MessageProbe"),QStringLiteral("org.uke.PlasmaKeyboard.Control1"),QStringLiteral("toggle"));
  QDBusPendingReply<bool> reply=client.asyncCall(message);QTRY_VERIFY(reply.isFinished());QVERIFY(!reply.isError());QVERIFY(reply.value());QCOMPARE(toggles,1);
  QDBusPendingReply<bool> denied=server.asyncCall(message);QTRY_VERIFY(denied.isFinished());QVERIFY(!denied.isError());QVERIFY(!denied.value());QCOMPARE(toggles,1);
  server.unregisterObject(QStringLiteral("/MessageProbe"));QDBusConnection::disconnectFromBus(QStringLiteral("uke-control-test-client"));
 }
 void parentInputIsNotExported(){
  InputParent target;int toggles=0;
  KeyboardControlAdaptor adaptor(&target,[]{return QVariantMap{{QStringLiteral("active"),false}};},[&]{++toggles;});
  auto bus=QDBusConnection::sessionBus();QVERIFY(bus.isConnected());
  QVERIFY(adaptor.publish(bus,QStringLiteral("/TestControl")));
  auto message=QDBusMessage::createMethodCall(bus.baseService(),QStringLiteral("/TestControl"),QStringLiteral("org.freedesktop.DBus.Introspectable"),QStringLiteral("Introspect"));
  QDBusPendingReply<QString> introspection=bus.asyncCall(message);QTRY_VERIFY(introspection.isFinished());QVERIFY(!introspection.isError());
  const QString xml=introspection.value();QVERIFY(xml.contains(QStringLiteral("name=\"status\"")));QVERIFY(xml.contains(QStringLiteral("name=\"toggle\"")));
  for(const auto&name:{QStringLiteral("key"),QStringLiteral("motion"),QStringLiteral("button"),QStringLiteral("deleteWords"),QStringLiteral("releaseAll")})QVERIFY(!xml.contains(QStringLiteral("name=\"%1\"").arg(name)));
  auto call=QDBusMessage::createMethodCall(bus.baseService(),QStringLiteral("/TestControl"),QStringLiteral("org.uke.PlasmaKeyboard.Control1"),QStringLiteral("toggle"));
  QDBusPendingReply<bool> reply=bus.asyncCall(call);QTRY_VERIFY(reply.isFinished());QVERIFY(!reply.isError());QVERIFY(!reply.value());QCOMPARE(toggles,0);
  auto injection=QDBusMessage::createMethodCall(bus.baseService(),QStringLiteral("/TestControl"),QStringLiteral("org.uke.PlasmaKeyboard.Control1"),QStringLiteral("key"));injection<<uint(30)<<true;
  QDBusPendingReply<> denied=bus.asyncCall(injection);QTRY_VERIFY(denied.isFinished());QVERIFY(denied.isError());QCOMPARE(target.injected,0);
  bus.unregisterObject(QStringLiteral("/TestControl"));
 }
};
QTEST_GUILESS_MAIN(ControlAdaptorTest)
#include "controladaptortest.moc"
