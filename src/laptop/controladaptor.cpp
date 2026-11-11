// SPDX-License-Identifier: GPL-2.0-or-later
#include "controladaptor.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFileInfo>
#include <unistd.h>
KeyboardControlAdaptor::KeyboardControlAdaptor(QObject*parent,std::function<QVariantMap()>status,std::function<void()>toggle,CallerVerifier verifier):QDBusAbstractAdaptor(parent),m_verifier(std::move(verifier)),m_status(std::move(status)),m_toggle(std::move(toggle)){setAutoRelaySignals(false);}
QVariantMap KeyboardControlAdaptor::status()const{return m_status();}
bool KeyboardControlAdaptor::trustedCaller(const QDBusMessage &request)const{
 if(request.type()!=QDBusMessage::MethodCallMessage||request.service().isEmpty())return false;
 auto *interface=m_bus.interface();if(!interface)return false;
 const auto uid=interface->serviceUid(request.service()),pid=interface->servicePid(request.service());
 if(!uid.isValid()||uid.value()!=getuid()||!pid.isValid())return false;
 const QFileInfo installed(QStringLiteral("/usr/bin/uke-plasma-keyboard-custom"));
 const QFileInfo process(QStringLiteral("/proc/%1/exe").arg(pid.value()));
 return installed.ownerId()==0&&!(installed.permissions()&(QFile::WriteGroup|QFile::WriteOther))&&process.symLinkTarget()==installed.canonicalFilePath();
}
bool KeyboardControlAdaptor::toggle(const QDBusMessage &request){if(!(m_verifier?m_verifier(m_bus,request):trustedCaller(request)))return false;m_toggle();return true;}

bool KeyboardControlAdaptor::publish(QDBusConnection bus,const QString&path){m_bus=bus;return bus.registerObject(path,parent(),QDBusConnection::ExportAdaptors);}
