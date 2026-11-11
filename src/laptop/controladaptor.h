// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusContext>
#include <QVariantMap>
#include <functional>
// Only these methods are exported. Native input methods on the parent QObject
// must never become a D-Bus proxy around the compositor's caller checks.
class KeyboardControlAdaptor final : public QDBusAbstractAdaptor,protected QDBusContext {
 Q_OBJECT
 Q_CLASSINFO("D-Bus Interface","org.uke.PlasmaKeyboard.Control1")
public:
 KeyboardControlAdaptor(QObject *parent,std::function<QVariantMap()>status,std::function<void()>toggle);
 bool publish(const QDBusConnection &bus,const QString &path);
public Q_SLOTS:
 QVariantMap status()const;
 bool toggle();
Q_SIGNALS:
 void stateChanged();
private:
 bool trustedCaller()const;
 std::function<QVariantMap()>m_status;
 std::function<void()>m_toggle;
};
