// SPDX-License-Identifier: GPL-2.0-or-later
#include "controladaptor.h"
#include <QtTest>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
class InputParent:public QObject{
 Q_OBJECT
public:
 int injected=0;
 Q_INVOKABLE void key(uint,bool){++injected;}
};
class ControlAdaptorTest:public QObject{
 Q_OBJECT
private Q_SLOTS:
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
