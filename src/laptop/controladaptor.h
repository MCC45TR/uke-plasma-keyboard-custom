// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariantMap>
#include <functional>
// Only these methods are exported. Native input methods on the parent QObject
// must never become a D-Bus proxy around the compositor's caller checks.
class KeyboardControlAdaptor final : public QDBusAbstractAdaptor {
 Q_OBJECT
 Q_CLASSINFO("D-Bus Interface","org.uke.PlasmaKeyboard.Control1")
public:
 using CallerVerifier=std::function<bool(const QDBusConnection &,const QDBusMessage &)>;
 KeyboardControlAdaptor(QObject *parent,std::function<QVariantMap()>status,std::function<void()>toggle,CallerVerifier verifier={});
 bool publish(QDBusConnection bus,const QString &path);
public Q_SLOTS:
 QVariantMap status()const;
 bool toggle(const QDBusMessage &request);
Q_SIGNALS:
 void stateChanged();
private:
 bool trustedCaller(const QDBusMessage &request)const;
 QDBusConnection m_bus{QDBusConnection::sessionBus()};
 CallerVerifier m_verifier;
 std::function<QVariantMap()>m_status;
 std::function<void()>m_toggle;
};
