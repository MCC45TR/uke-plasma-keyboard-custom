// SPDX-License-Identifier: GPL-2.0-or-later
#include "controladaptor.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFileInfo>
#include <unistd.h>
KeyboardControlAdaptor::KeyboardControlAdaptor(QObject*parent,std::function<QVariantMap()>status,std::function<void()>toggle):QDBusAbstractAdaptor(parent),m_status(std::move(status)),m_toggle(std::move(toggle)){setAutoRelaySignals(false);}
QVariantMap KeyboardControlAdaptor::status()const{return m_status();}
bool KeyboardControlAdaptor::trustedCaller()const{
 if(!calledFromDBus())return false;
 auto *interface=connection().interface();if(!interface)return false;
 const auto uid=interface->serviceUid(message().service()),pid=interface->servicePid(message().service());
 if(!uid.isValid()||uid.value()!=getuid()||!pid.isValid())return false;
 const QFileInfo installed(QStringLiteral("/usr/bin/uke-plasma-keyboard-custom"));
 const QFileInfo process(QStringLiteral("/proc/%1/exe").arg(pid.value()));
 return installed.ownerId()==0&&!(installed.permissions()&(QFile::WriteGroup|QFile::WriteOther))&&process.symLinkTarget()==installed.canonicalFilePath();
}
bool KeyboardControlAdaptor::toggle(){if(!trustedCaller())return false;m_toggle();return true;}

bool KeyboardControlAdaptor::publish(QDBusConnection bus,const QString&path){return bus.registerObject(path,parent(),QDBusConnection::ExportAdaptors);}
