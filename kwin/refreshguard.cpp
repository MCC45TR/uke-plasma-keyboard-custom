// SPDX-License-Identifier: GPL-2.0-or-later
#include "refreshguard.h"
#include <KScreen/Config>
#include <KScreen/GetConfigOperation>
#include <KScreen/SetConfigOperation>
#include <KScreen/Output>
#include <KScreen/Mode>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>
#include <unistd.h>
namespace {
KScreen::ModePtr findMode(const KScreen::OutputPtr &output,int width,int height,double hz){
 for(const auto&mode:output->modes())if(mode->size()==QSize(width,height)&&std::abs(mode->refreshRate()-hz)<0.6)return mode;
 return {};
}
bool matches(const KScreen::ModePtr &mode,const QVariantMap&saved){
 return mode&&mode->size()==QSize(saved.value("appliedWidth").toInt(),saved.value("appliedHeight").toInt())&&std::abs(mode->refreshRate()-saved.value("appliedHz").toDouble())<0.6;
}
}
RefreshGuard::RefreshGuard(QObject*parent,std::function<void(const QString&)>error):QObject(parent),m_error(std::move(error)){load();if(!m_saved.isEmpty()){m_pending=true;process();}}
QString RefreshGuard::journal()const{return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+QStringLiteral("/uke-keyboard-refresh.json");}
void RefreshGuard::load(){
 const QFileInfo info(journal());
 if(!info.exists()||info.isSymLink()||!info.isFile()||info.ownerId()!=getuid()||info.size()>4096||(info.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)))return;
 QFile f(journal());if(!f.open(QIODevice::ReadOnly))return;
 const auto data=QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
 if(data.value("version").toInt()!=1||data.value("name").toString().size()>128||data.value("name").toString().isEmpty())return;
 for(const QString&key:{QStringLiteral("width"),QStringLiteral("height"),QStringLiteral("appliedWidth"),QStringLiteral("appliedHeight")})if(data.value(key).toInt()<100||data.value(key).toInt()>16000)return;
 for(const QString&key:{QStringLiteral("hz"),QStringLiteral("appliedHz")})if(!std::isfinite(data.value(key).toDouble())||data.value(key).toDouble()<1||data.value(key).toDouble()>1000)return;
 if(data.value("policy").toInt()<0||data.value("policy").toInt()>2||data.value("appliedPolicy").toInt()<0||data.value("appliedPolicy").toInt()>2)return;
 m_saved=data;
}
bool RefreshGuard::save(){QSaveFile file(journal());if(!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)){m_error(QStringLiteral("Cannot create private refresh recovery state; no refresh setting was changed."));return false;}file.write(QJsonDocument(QJsonObject::fromVariantMap(m_saved)).toJson(QJsonDocument::Compact));if(!file.commit()){m_error(QStringLiteral("Could not save temporary refresh recovery state."));return false;}return true;}
void RefreshGuard::clear(){m_saved.clear();QFile::remove(journal());}
void RefreshGuard::begin(const QString&name){m_name=name;m_active=true;}
bool RefreshGuard::set(const QString&profile){if(!m_active||!QStringList{"keep","30","60","adaptive"}.contains(profile))return false;if(profile==QLatin1String("adaptive")){m_error(QStringLiteral("30–60 Hz VRR requires physical kernel/display qualification; no display setting was changed."));return false;}m_wanted=profile;m_pending=true;process();return true;}
void RefreshGuard::end(){m_active=false;m_wanted=QStringLiteral("keep");m_pending=true;process();}
void RefreshGuard::topologyChanged(){if(!m_saved.isEmpty()&&(!m_active||m_wanted==QLatin1String("keep"))){m_pending=true;process();}}
void RefreshGuard::finish(){m_busy=false;if(m_pending)process();}
void RefreshGuard::process(){
 if(m_busy||!m_pending)return;m_pending=false;
 const bool restore=!m_active||m_wanted==QLatin1String("keep");
 if(restore&&m_saved.isEmpty())return;
 m_busy=true;const QString target=restore?m_saved.value("name").toString():m_name;
 const QString wanted=m_wanted;
 auto *get=new KScreen::GetConfigOperation(KScreen::GetConfigOperation::NoEDID,this);
 connect(get,&KScreen::ConfigOperation::finished,this,[this,get,restore,target,wanted](auto*){
  if(get->hasError()){m_error(QStringLiteral("Cannot read display state for the refresh profile."));finish();return;}
  if(!restore&&(!m_active||m_name!=target||m_wanted!=wanted)){finish();return;}
  auto config=get->config()->clone();KScreen::OutputPtr panel;
  for(const auto&output:config->outputs())if(output->name()==target&&output->type()==KScreen::Output::Panel&&output->isConnected()&&output->isEnabled())panel=output;
  if(!panel||!panel->currentMode()){finish();return;}
  if(restore){
   if(!matches(panel->currentMode(),m_saved)||int(panel->vrrPolicy())!=m_saved.value("appliedPolicy").toInt()){clear();finish();return;}
   const auto old=findMode(panel,m_saved.value("width").toInt(),m_saved.value("height").toInt(),m_saved.value("hz").toDouble());
   if(!old){m_error(QStringLiteral("The original refresh mode is no longer advertised; the display was left unchanged."));finish();return;}
   panel->setCurrentModeId(old->id());panel->setVrrPolicy(KScreen::Output::VrrPolicy(m_saved.value("policy").toInt()));
  }else{
   const auto current=panel->currentMode();const auto next=findMode(panel,current->size().width(),current->size().height(),wanted.toInt());
   if(!next){m_error(QStringLiteral("This display does not advertise the requested fixed refresh rate."));finish();return;}
   if(m_saved.isEmpty()||m_saved.value("name").toString()!=target||!matches(current,m_saved)||int(panel->vrrPolicy())!=m_saved.value("appliedPolicy").toInt())m_saved={{"version",1},{"name",target},{"width",current->size().width()},{"height",current->size().height()},{"hz",current->refreshRate()},{"policy",int(panel->vrrPolicy())}};
   m_saved["appliedWidth"]=next->size().width();m_saved["appliedHeight"]=next->size().height();m_saved["appliedHz"]=next->refreshRate();m_saved["appliedPolicy"]=int(KScreen::Output::VrrPolicy::Never);
   if(!save()){finish();return;}panel->setCurrentModeId(next->id());panel->setVrrPolicy(KScreen::Output::VrrPolicy::Never);
  }
  auto *set=new KScreen::SetConfigOperation(config,this);
  connect(set,&KScreen::ConfigOperation::finished,this,[this,set,restore](auto*){if(set->hasError())m_error(QStringLiteral("KWin refused the temporary refresh configuration."));else if(restore)clear();finish();});
 });
}
