/*
 SPDX-FileCopyrightText: 2026 Chechulin Serhii
 SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "touchholdwatcher.h"
#include "laptop/laptopcontroller.h"
#include "laptop/protocol.h"
#include <QDBusConnection>
#include <algorithm>
#include <cmath>
TouchHoldWatcher::TouchHoldWatcher(QObject*parent):QObject(parent),m_timer(this){
 m_timer.setSingleShot(true);
 QDBusConnection::sessionBus().connect(QLatin1String(UkeKeyboard::service),QLatin1String(UkeKeyboard::path),QLatin1String(UkeKeyboard::interface),QStringLiteral("Touch"),this,SLOT(touch(int,int,double,double,double,double)));
 connect(&m_timer,&QTimer::timeout,this,[this]{if(m_armed&&!m_moved&&m_contacts.size()==1){if(!m_persistent)m_armed=false;Q_EMIT longPress();}});
}
bool TouchHoldWatcher::arm(int threshold){
 if(!LaptopController::instance()||!LaptopController::instance()->backendReady())return false;
 m_threshold=std::clamp(threshold,100,5000);m_armed=true;m_persistent=false;
 if(m_contacts.size()==1&&!m_moved&&m_elapsed.isValid())m_timer.start(std::max(0,m_threshold-int(m_elapsed.elapsed())));
 return true;
}
bool TouchHoldWatcher::watchContinuously(int threshold){if(!arm(threshold))return false;m_persistent=true;return true;}
void TouchHoldWatcher::disarm(){m_armed=false;m_persistent=false;m_timer.stop();}
bool TouchHoldWatcher::takeCompletedLongPress(int threshold){const bool matched=m_completed>=threshold&&m_released.isValid()&&m_released.elapsed()<=std::clamp(threshold/2,50,250);m_completed=-1;m_released.invalidate();if(matched)disarm();return matched;}
void TouchHoldWatcher::touch(int phase,int id,double x,double y,double slopX,double slopY){
 if(!std::isfinite(x)||!std::isfinite(y))return;
 m_position={std::clamp(x,0.0,1.0),std::clamp(y,0.0,1.0)};
 if(phase==0){m_contacts.insert(id);if(m_contacts.size()==1){m_start=m_position;m_moved=false;m_elapsed.restart();if(m_armed)m_timer.start(m_threshold);}else{m_moved=true;m_timer.stop();}}
 else if(phase==1){if(std::abs(m_position.x()-m_start.x())>slopX||std::abs(m_position.y()-m_start.y())>slopY){m_moved=true;m_timer.stop();}}
 else if(phase==2){const bool single=m_contacts.size()==1;m_contacts.remove(id);m_timer.stop();if(single&&!m_moved&&m_elapsed.isValid()){m_completed=int(m_elapsed.elapsed());m_released.restart();}if(!m_persistent)m_armed=false;}
 else{m_contacts.clear();m_moved=true;m_completed=-1;m_timer.stop();}
 if(phase!=1)if(auto *laptop=LaptopController::instance())laptop->setTouchActive(!m_contacts.isEmpty());
}
