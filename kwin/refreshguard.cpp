// SPDX-License-Identifier: GPL-2.0-or-later
#include "refreshguard.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <cmath>
#include <unistd.h>
#include <utility>

namespace {
RefreshGuard::Mode recordedMode(const QVariantMap &saved,bool applied)
{
 const QString prefix=applied?QStringLiteral("applied"):QString();
 const auto key=[&prefix](const char *plain,const char *capital){return prefix.isEmpty()?QString::fromLatin1(plain):prefix+QString::fromLatin1(capital);};
 return {{saved.value(key("width","Width")).toInt(),saved.value(key("height","Height")).toInt()},qRound(saved.value(key("hz","Hz")).toDouble()*1000)};
}
std::optional<RefreshGuard::Mode> findMode(const QList<RefreshGuard::Mode>&modes,const QSize &size,int milliHz,int tolerance=0)
{
 std::optional<RefreshGuard::Mode> best;
 for(const auto &mode:modes){
  const int distance=std::abs(mode.milliHz-milliHz);
  if(mode.size==size&&distance<=tolerance&&(!best||distance<std::abs(best->milliHz-milliHz)))best=mode;
 }
 return best;
}
bool ownsMode(const RefreshGuard::Snapshot &current,const QVariantMap &saved)
{
 return current.current==recordedMode(saved,true);
}
bool ownsPolicy(const RefreshGuard::Snapshot &current,const QVariantMap &saved)
{
 return current.policy==saved.value(QStringLiteral("appliedPolicy")).toInt();
}
}

RefreshGuard::RefreshGuard(QObject *parent,std::function<void(const QString&)> error,Backend backend)
 :QObject(parent),m_error(std::move(error)),m_backend(std::move(backend))
{
 load();
 if(!m_saved.isEmpty()){m_pending=true;process();}
}
QString RefreshGuard::journal()const
{
 return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+QStringLiteral("/uke-keyboard-refresh.json");
}
void RefreshGuard::load()
{
 const QFileInfo info(journal());
 if(!info.exists()||info.isSymLink()||!info.isFile()||info.ownerId()!=getuid()||info.size()>4096||(info.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)))return;
 QFile file(journal());if(!file.open(QIODevice::ReadOnly))return;
 const auto data=QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
 if(data.value(QStringLiteral("version")).toInt()!=1||data.value(QStringLiteral("name")).toString().size()>128||data.value(QStringLiteral("name")).toString().isEmpty())return;
 for(const QString &key:{QStringLiteral("width"),QStringLiteral("height"),QStringLiteral("appliedWidth"),QStringLiteral("appliedHeight")})if(data.value(key).toInt()<100||data.value(key).toInt()>16000)return;
 for(const QString &key:{QStringLiteral("hz"),QStringLiteral("appliedHz")})if(!std::isfinite(data.value(key).toDouble())||data.value(key).toDouble()<1||data.value(key).toDouble()>1000)return;
 if(data.value(QStringLiteral("policy")).toInt()<0||data.value(QStringLiteral("policy")).toInt()>2||data.value(QStringLiteral("appliedPolicy")).toInt()<0||data.value(QStringLiteral("appliedPolicy")).toInt()>2)return;
 m_saved=data;
}
bool RefreshGuard::save()
{
 QSaveFile file(journal());
 if(!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)){
  m_error(QStringLiteral("Cannot create private refresh recovery state; no refresh setting was changed."));return false;
 }
 const auto bytes=QJsonDocument(QJsonObject::fromVariantMap(m_saved)).toJson(QJsonDocument::Compact);
 if(file.write(bytes)!=bytes.size()||!file.commit()){
  m_error(QStringLiteral("Could not save temporary refresh recovery state."));return false;
 }
 return true;
}
void RefreshGuard::clear()
{
 m_saved.clear();
 if(QFile::exists(journal())&&!QFile::remove(journal()))m_error(QStringLiteral("Could not remove restored refresh recovery state."));
}
void RefreshGuard::begin(const QString &name){m_name=name;m_active=true;}
bool RefreshGuard::set(const QString &profile)
{
 if(!m_active||!QStringList{QStringLiteral("keep"),QStringLiteral("30"),QStringLiteral("60"),QStringLiteral("adaptive")}.contains(profile))return false;
 if(profile==QLatin1String("adaptive")){
  m_error(QStringLiteral("30–60 Hz VRR requires physical kernel/display qualification; no display setting was changed."));return false;
 }
 m_wanted=profile;m_pending=true;process();return true;
}
void RefreshGuard::end()
{
 // A synchronous compositor apply can emit topology callbacks. Do not queue
 // the same failed restoration repeatedly from an already-finished session.
 if(!m_active&&m_wanted==QLatin1String("keep")&&m_busy)return;
 m_active=false;m_wanted=QStringLiteral("keep");m_pending=true;process();
}
bool RefreshGuard::needsRestoration()const
{
 return !m_active||m_wanted==QLatin1String("keep")||(!m_saved.isEmpty()&&m_saved.value(QStringLiteral("name")).toString()!=m_name);
}
void RefreshGuard::topologyChanged()
{
 if(!m_busy&&!m_saved.isEmpty()&&needsRestoration()){m_pending=true;process();}
}
void RefreshGuard::process()
{
 if(m_busy||!m_pending)return;
 m_busy=true;
 while(m_pending){m_pending=false;step();}
 m_busy=false;
}
void RefreshGuard::step()
{
 const bool restore=needsRestoration();
 if(restore&&m_saved.isEmpty())return;
 const QString target=restore?m_saved.value(QStringLiteral("name")).toString():m_name;
 const auto current=m_backend.read(target);
 if(!current)return; // A missing/disabled panel is never enabled for recovery.
 if(restore){
  const bool modeOwned=ownsMode(*current,m_saved);
  const bool policyOwned=ownsPolicy(*current,m_saved);
  if(!modeOwned&&!policyOwned)clear();
  else{
   const auto original=recordedMode(m_saved,false);
   const auto mode=modeOwned?findMode(current->modes,original.size,original.milliHz):std::optional<Mode>(current->current);
   if(!mode){m_error(QStringLiteral("The original refresh mode is no longer advertised; the display was left unchanged."));return;}
   const int policy=policyOwned?m_saved.value(QStringLiteral("policy")).toInt():current->policy;
   if(!m_backend.apply(target,*mode,policy)){
    m_error(QStringLiteral("KWin refused the temporary refresh configuration."));return;
   }
   clear();
  }
  if(m_active&&m_wanted!=QLatin1String("keep"))m_pending=true;
  return;
 }

 const auto next=findMode(current->modes,current->current.size,m_wanted.toInt()*1000,600);
 if(!next){m_error(QStringLiteral("This display does not advertise the requested fixed refresh rate."));return;}
 if(!m_saved.isEmpty()){
  const bool modeOwned=ownsMode(*current,m_saved);
  const bool policyOwned=ownsPolicy(*current,m_saved);
  if(!modeOwned&&!policyOwned)clear();
  else{
   // A user edit relinquishes only that field. Keep the other field's original
   // baseline across later profile switches and restoration.
   if(!modeOwned){m_saved[QStringLiteral("width")]=current->current.size.width();m_saved[QStringLiteral("height")]=current->current.size.height();m_saved[QStringLiteral("hz")]=current->current.milliHz/1000.0;}
   if(!policyOwned)m_saved[QStringLiteral("policy")]=current->policy;
  }
 }
 if(current->current==*next&&current->policy==0){if(!m_saved.isEmpty())save();return;}
 const auto previous=m_saved;
 if(m_saved.isEmpty())m_saved={{QStringLiteral("version"),1},{QStringLiteral("name"),target},{QStringLiteral("width"),current->current.size.width()},{QStringLiteral("height"),current->current.size.height()},{QStringLiteral("hz"),current->current.milliHz/1000.0},{QStringLiteral("policy"),current->policy}};
 m_saved[QStringLiteral("appliedWidth")]=next->size.width();m_saved[QStringLiteral("appliedHeight")]=next->size.height();m_saved[QStringLiteral("appliedHz")]=next->milliHz/1000.0;m_saved[QStringLiteral("appliedPolicy")]=0;
 if(!save()){m_saved=previous;return;}
 if(!m_backend.apply(target,*next,0)){
  // Keep the last successfully owned setting when a subsequent 30/60 switch
  // fails. Recording the failed target would otherwise lose the original mode.
  m_saved=previous;
  if(m_saved.isEmpty())clear();else save();
  m_error(QStringLiteral("KWin refused the temporary refresh configuration."));
 }
}
