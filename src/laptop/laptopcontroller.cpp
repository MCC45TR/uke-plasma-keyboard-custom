// SPDX-License-Identifier: GPL-2.0-or-later
#include "laptopcontroller.h"
#include "controladaptor.h"
#include "physicalgeometry.h"
#include "layoutregistry.h"
#include "protocol.h"
#include "sessionlockstate.h"
#include "prediction/hunspelldictionary.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/SetConfigOperation>
#include <KScreen/Output>
#include <KScreen/Mode>
#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QInputDevice>
#include <QLocale>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QXmlStreamReader>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-names.h>
#include <algorithm>
#include <tuple>
#include <cmath>

namespace {
bool modifier(uint code) { return code==KEY_LEFTSHIFT||code==KEY_RIGHTSHIFT||code==KEY_LEFTCTRL||code==KEY_RIGHTCTRL||code==KEY_LEFTALT||code==KEY_RIGHTALT||code==KEY_LEFTMETA||code==KEY_RIGHTMETA; }
QDBusMessage layoutCall(const QString &method,const QVariantList &args={}) {
 auto m=QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"),QStringLiteral("/Layouts"),QStringLiteral("org.kde.KeyboardLayouts"),method);m.setArguments(args);return m;
}

}
LaptopController *LaptopController::s_instance=nullptr;
LaptopController *LaptopController::instance(){return s_instance;}
QVariantMap LaptopController::defaults() {
 return {{"keyWidthMm",14.0},{"keyHeightMm",12.0},{"gapMm",0.8},{"touchpadHeightMm",45.0},
 {"touchpadWidthPercent",60},{"touchpadFullWidth",false},{"pointerSpeed",1.3},{"naturalScroll",true},{"palmGuard",true},
 {"leftButton",false},{"middleButton",false},{"rightButton",false},{"functionRow",true},{"modifierRow",true},{"arrowRow",true},
 {"navigationRow",true},{"systemRow",true},{"nativePcLayout",true},{"normalPcLayout",false},{"spaceGesture",true},{"deleteGesture",true},
 {"keyPreview",false},{"calibrationRuler",false},{"refreshProfile",QStringLiteral("keep")},{"physicalWidthMm",0.0},{"physicalHeightMm",0.0},
 {"syncLayout",true},{"toolbar",QStringList{"volume","brightness","media","clipboard","emoji","language","settings"}}};
}
LaptopController::LaptopController(QObject *parent):QObject(parent),m_bus(QDBusConnection::sessionBus()),m_files(this),m_xkb(xkb_context_new(XKB_CONTEXT_NO_FLAGS)) {
 s_instance=this;m_clock.start();reload();loadLayouts();
 m_typingTimer.setSingleShot(true);m_typingTimer.setInterval(250);
 connect(&m_typingTimer,&QTimer::timeout,this,[this]{m_typing=false;Q_EMIT typingChanged();});
 const QString config=QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
 m_files.addPath(config);
 if(QFile::exists(QStringLiteral("/usr/share/hunspell")))m_files.addPath(QStringLiteral("/usr/share/hunspell"));
 connect(&m_files,&QFileSystemWatcher::directoryChanged,this,[this](const QString &path){reload();loadLayouts();if(path==QLatin1String("/usr/share/hunspell"))Q_EMIT layoutChanged();});
 m_bus.connect(QStringLiteral("org.kde.keyboard"),QStringLiteral("/Layouts"),QStringLiteral("org.kde.KeyboardLayouts"),QStringLiteral("layoutChanged"),this,SLOT(systemLayout(uint)));
 auto *lock=SessionLockState::instance();
 connect(lock,&SessionLockState::stateChanged,this,[this,lock]{m_locked=!lock->isKnown()||lock->isLocked()||lock->isPreparingLock();if(m_locked){leave();releaseAll();}else if(!m_ready&&!m_preferencesOnly&&!qEnvironmentVariableIsSet("PLASMA_KEYBOARD_TEST_MODE"))connectBackend();Q_EMIT stateChanged();});
 m_locked=!lock->isKnown()||lock->isLocked();
 m_preferencesOnly=QCoreApplication::arguments().contains(QStringLiteral("--preferences"));
 if(m_preferencesOnly){
  m_bus.connect(QStringLiteral("org.uke.plasma.keyboard"),QStringLiteral("/Laptop"),QStringLiteral("org.uke.PlasmaKeyboard.Control1"),QStringLiteral("stateChanged"),this,SLOT(refreshRemote()));
  refreshRemote();return;
 }
 if(qEnvironmentVariableIsSet("PLASMA_KEYBOARD_TEST_MODE"))return;
 m_bus.registerService(QStringLiteral("org.uke.plasma.keyboard"));
 auto *adaptor=new KeyboardControlAdaptor(this,[this]{return status();},[this]{toggle();});
 connect(this,&LaptopController::stateChanged,adaptor,&KeyboardControlAdaptor::stateChanged);
 adaptor->publish(m_bus,QStringLiteral("/Laptop"));
 auto system=QDBusConnection::systemBus();
 system.connect(QStringLiteral("org.freedesktop.UPower"),QStringLiteral("/org/freedesktop/UPower"),QStringLiteral("org.freedesktop.DBus.Properties"),QStringLiteral("PropertiesChanged"),this,SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
 auto lid=QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.UPower"),QStringLiteral("/org/freedesktop/UPower"),QStringLiteral("org.freedesktop.DBus.Properties"),QStringLiteral("GetAll"));lid<<QStringLiteral("org.freedesktop.UPower");
 auto *lidWatcher=new QDBusPendingCallWatcher(system.asyncCall(lid),this);
 connect(lidWatcher,&QDBusPendingCallWatcher::finished,this,[this](auto*w){QDBusPendingReply<QVariantMap> reply=*w;if(!reply.isError()){const auto p=reply.value();m_lidClosed=p.value("LidIsPresent").toBool()&&p.value("LidIsClosed").toBool();updateTopology();}w->deleteLater();});
 m_bus.connect(QLatin1String(UkeKeyboard::service),QLatin1String(UkeKeyboard::path),QLatin1String(UkeKeyboard::interface),QStringLiteral("StaticChanged"),this,SLOT(backendStatic(bool)));
 m_bus.connect(QLatin1String(UkeKeyboard::service),QLatin1String(UkeKeyboard::path),QLatin1String(UkeKeyboard::interface),QStringLiteral("ProfileError"),this,SLOT(profileError(QString)));
 m_bus.connect(QLatin1String(UkeKeyboard::service),QLatin1String(UkeKeyboard::path),QLatin1String(UkeKeyboard::interface),QStringLiteral("Locks"),this,SLOT(syncLocks(bool,bool,bool)));
 auto *watcher=new QDBusServiceWatcher(QLatin1String(UkeKeyboard::service),m_bus,QDBusServiceWatcher::WatchForOwnerChange,this);
 connect(watcher,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const auto&,const auto&,const QString &owner){m_ready=false;if(owner.isEmpty()){backendStatic(false);m_reason=i18nd("uke-plasma-keyboard-custom", "The KWin integration is unavailable or needs rebuilding for this KWin version.");Q_EMIT stateChanged();}else connectBackend();});
 auto load=QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),QStringLiteral("/Plugins"),QStringLiteral("org.kde.KWin.Plugins"),QStringLiteral("LoadPlugin"));load<<QStringLiteral("uke_keyboard_backend");
 auto *loader=new QDBusPendingCallWatcher(m_bus.asyncCall(load),this);
 connect(loader,&QDBusPendingCallWatcher::finished,this,[this](auto*w){w->deleteLater();connectBackend();});
 requestTopology();
 connect(KScreen::ConfigMonitor::instance(),&KScreen::ConfigMonitor::configurationChanged,this,&LaptopController::updateTopology);
 connect(qGuiApp,&QGuiApplication::screenAdded,this,[this]{updateTopology();});
 connect(qGuiApp,&QGuiApplication::screenRemoved,this,[this]{updateTopology();});
}
LaptopController::~LaptopController(){leave();releaseAll();if(s_instance==this)s_instance=nullptr;}
void LaptopController::connectBackend(){
 auto *w=new QDBusPendingCallWatcher(backendCall(QStringLiteral("Attach")),this);
 connect(w,&QDBusPendingCallWatcher::finished,this,[this](auto*call){QDBusPendingReply<bool> reply=*call;m_ready=!reply.isError()&&reply.value();if(!m_ready)m_reason=i18nd("uke-plasma-keyboard-custom", "Select Uke Plasma Keyboard in System Settings. Its matching KWin integration must be installed.");else{auto*locks=new QDBusPendingCallWatcher(backendCall(QStringLiteral("Locks")),this);connect(locks,&QDBusPendingCallWatcher::finished,this,[this](auto*w){QDBusPendingReply<QVariantMap>r=*w;if(!r.isError()){const auto p=r.value();syncLocks(p.value("caps").toBool(),p.value("num").toBool(),p.value("scroll").toBool());}w->deleteLater();});}updateTopology();call->deleteLater();});
}
QDBusPendingCall LaptopController::backendCall(const QString &method,const QVariantList &args)const {
 auto m=QDBusMessage::createMethodCall(QLatin1String(UkeKeyboard::service),QLatin1String(UkeKeyboard::path),QLatin1String(UkeKeyboard::interface),method);m.setArguments(args);m.setAutoStartService(false);return m_bus.asyncCall(m);
}
void LaptopController::requestTopology(){
 auto *op=new KScreen::GetConfigOperation(KScreen::GetConfigOperation::NoEDID,this);
 connect(op,&KScreen::ConfigOperation::finished,this,[this,op](KScreen::ConfigOperation*){if(!op->hasError()){m_config=op->config();KScreen::ConfigMonitor::instance()->addConfig(m_config);updateTopology();}});
}
void LaptopController::updateTopology(){
 m_available=false; m_screen=nullptr; m_tabletName.clear();
 if(m_config){
  QList<KScreen::OutputPtr> external;
  KScreen::OutputPtr panel;
  for(const auto&o:m_config->outputs()) {
   if(!o->isEnabled()||!o->isConnected())continue;
   if(o->type()==KScreen::Output::Panel){if(panel){panel.reset();break;}panel=o;}
   else external.append(o);
  }
  if(panel){
   m_tabletName=panel->name();
   for(auto*s:qGuiApp->screens())if(s->name()==m_tabletName)m_screen=s;
   bool touch=false;for(const auto*d:QInputDevice::devices())if(d->type()==QInputDevice::DeviceType::TouchScreen)touch=true;
   bool mirrored=panel->replicationSource()!=0;
   for(const auto&o:external)if(o->replicationSource()!=0||o->geometry().intersects(panel->geometry()))mirrored=true;
   m_available=touch&&m_screen&&!external.isEmpty()&&!mirrored;
   if(!touch)m_reason=i18nd("uke-plasma-keyboard-custom", "A touchscreen is required.");
   else if(external.isEmpty())m_reason=i18nd("uke-plasma-keyboard-custom", "Connect and enable an external display.");
   else if(mirrored)m_reason=i18nd("uke-plasma-keyboard-custom", "Use extended displays instead of mirroring.");
  }else m_reason=i18nd("uke-plasma-keyboard-custom", "The tablet touchscreen must already be enabled. This application does not enable displays.");
 }
 if(m_lidClosed)m_reason=i18nd("uke-plasma-keyboard-custom", "Open the tablet cover before entering laptop mode.");
 else if(m_locked)m_reason=i18nd("uke-plasma-keyboard-custom", "Unlock the session before entering laptop mode.");
 else if(m_available&&m_ready)m_reason.clear();
 if(m_active&&!available())leave();
 Q_EMIT stateChanged();
}
void LaptopController::propertiesChanged(const QString &iface,const QVariantMap&changed,const QStringList&){
 if(iface==QLatin1String("org.freedesktop.UPower")&&changed.contains("LidIsClosed")){m_lidClosed=changed.value("LidIsClosed").toBool();updateTopology();}
}
void LaptopController::toggle(){if(m_preferencesOnly){m_bus.asyncCall(QDBusMessage::createMethodCall(QStringLiteral("org.uke.plasma.keyboard"),QStringLiteral("/Laptop"),QStringLiteral("org.uke.PlasmaKeyboard.Control1"),QStringLiteral("toggle")));return;}if(m_active)leave();else enter();}
void LaptopController::enter(){
 if(m_preferencesOnly){m_bus.asyncCall(QDBusMessage::createMethodCall(QStringLiteral("org.uke.plasma.keyboard"),QStringLiteral("/Laptop"),QStringLiteral("org.uke.PlasmaKeyboard.Control1"),QStringLiteral("enter")));return;}
 if(!available()||m_active)return;
 auto*w=new QDBusPendingCallWatcher(backendCall(QStringLiteral("BeginStatic"),{m_tabletName}),this);
 connect(w,&QDBusPendingCallWatcher::finished,this,[this](auto*call){QDBusPendingReply<bool>reply=*call;if(!reply.isError()&&reply.value()){m_active=true;refreshProfile();Q_EMIT stateChanged();}else{m_reason=i18nd("uke-plasma-keyboard-custom", "KWin refused the current display layout.");Q_EMIT stateChanged();}call->deleteLater();});
}
void LaptopController::leave(){if(m_preferencesOnly){return;}if(!m_active)return;backendCall(QStringLiteral("EndStatic"));backendStatic(false);}
void LaptopController::backendStatic(bool active){
 if(m_active==active)return;
 m_active=active;if(!active){releaseAll();refreshProfile(true);}Q_EMIT stateChanged();
}
QVariantMap LaptopController::options()const{return m_options;}
void LaptopController::reload(){
 const auto config=KSharedConfig::openConfig(QStringLiteral("uke-plasma-keyboard-customrc"));config->reparseConfiguration();const auto g=config->group(QStringLiteral("Laptop"));auto next=defaults();
 for(auto i=next.begin();i!=next.end();++i)i.value()=g.readEntry(i.key(),i.value());
 if(next!=m_options){const bool syncChanged=!m_options.isEmpty()&&m_options.value("syncLayout")!=next.value("syncLayout");m_options=next;if(syncChanged&&!m_layoutCodes.isEmpty()){releaseAll();m_layoutCodes.clear();loadLayouts();}Q_EMIT optionsChanged();Q_EMIT layoutChanged();Q_EMIT stateChanged();}
}
void LaptopController::setOption(const QString&name,const QVariant&value){
 const auto d=defaults();if(!d.contains(name))return;QVariant v=value;
 const QHash<QString,QPair<double,double>> ranges{{"keyWidthMm",{7,24}},{"keyHeightMm",{7,24}},{"gapMm",{0,4}},{"touchpadHeightMm",{20,150}},{"touchpadWidthPercent",{30,100}},{"pointerSpeed",{0.2,4}},{"physicalWidthMm",{0,1000}},{"physicalHeightMm",{0,1000}}};
 if(ranges.contains(name)){const double n=value.toDouble();if(!std::isfinite(n))return;v=std::clamp(n,ranges.value(name).first,ranges.value(name).second);}
 if(name==QLatin1String("refreshProfile")&&!QStringList{"keep","30","60","adaptive"}.contains(value.toString()))return;
 if(name==QLatin1String("toolbar")){const QStringList allowed=defaults().value("toolbar").toStringList();QStringList selected;for(const auto&x:value.toStringList())if(allowed.contains(x)&&!selected.contains(x))selected.append(x);v=selected;}
 auto c=KSharedConfig::openConfig(QStringLiteral("uke-plasma-keyboard-customrc"));c->group(QStringLiteral("Laptop")).writeEntry(name,v,KConfig::Notify);c->sync();reload();
 if(m_active&&name==QLatin1String("refreshProfile"))refreshProfile();
}
double LaptopController::pixelsPerMm()const{
 if(!m_screen)return m_remoteDensity>0?m_remoteDensity:96.0/25.4;
 QSizeF mm(m_options.value("physicalWidthMm").toDouble(),m_options.value("physicalHeightMm").toDouble());
 if(!UkeKeyboard::plausibleSize(mm))mm=m_screen->physicalSize();
 const bool portrait=m_screen->geometry().height()>m_screen->geometry().width();
 if((portrait&&mm.width()>mm.height())||(!portrait&&mm.height()>mm.width()))mm.transpose();
 return UkeKeyboard::unitsPerMm(m_screen->geometry().width(),mm.width());
}
void LaptopController::loadLayouts(){
 const auto c=KSharedConfig::openConfig(QStringLiteral("kxkbrc"));c->reparseConfiguration();const auto g=c->group(QStringLiteral("Layout"));
 auto codes=g.readEntry(QStringLiteral("LayoutList"),QStringList{});auto variants=g.readEntry(QStringLiteral("VariantList"),QStringList{});
 if(!m_options.value("syncLayout").toBool()){
  const auto own=KSharedConfig::openConfig(QStringLiteral("uke-plasma-keyboard-customrc"))->group(QStringLiteral("Laptop"));
  codes=own.readEntry(QStringLiteral("VirtualLayoutList"),codes);variants=own.readEntry(QStringLiteral("VirtualVariantList"),variants);
  m_layoutIndex=own.readEntry(QStringLiteral("IndependentLayout"),0);
 }
 if(codes.isEmpty())codes={UkeKeyboard::LayoutRegistry::system().defaultLayout()};
 if(codes==m_layoutCodes&&variants==m_variants&&!m_layouts.isEmpty())return;
 m_layoutCodes=codes;m_variants=variants;m_layouts.clear();const auto all=allLayouts();
 for(int i=0;i<codes.size();i++){QString title=codes[i];for(const auto&v:all){const auto m=v.toMap();if(m.value("code")==codes[i])title=m.value("label").toString();}if(!variants.value(i).isEmpty())title+=QStringLiteral(" (%1)").arg(variants[i]);m_layouts.append(QVariantMap{{"code",codes[i]},{"variant",variants.value(i)},{"locale",UkeKeyboard::LayoutRegistry::system().locale(codes[i],variants.value(i))},{"label",title}});}
 m_layoutIndex=std::clamp(m_layoutIndex,0,int(codes.size())-1);rebuildKeymap();
 auto*w=new QDBusPendingCallWatcher(m_bus.asyncCall(layoutCall(QStringLiteral("getLayout"))),this);
 connect(w,&QDBusPendingCallWatcher::finished,this,[this](auto*call){QDBusPendingReply<uint>r=*call;if(!r.isError())systemLayout(r.value());call->deleteLater();});
}
void LaptopController::rebuildKeymap(){
 if(!m_xkb)return;
 const QByteArray code=m_layoutCodes.value(m_layoutIndex,QStringLiteral("us")).toLatin1(),variant=m_variants.value(m_layoutIndex).toLatin1();
 const auto c=KSharedConfig::openConfig(QStringLiteral("kxkbrc"));const QByteArray options=c->group(QStringLiteral("Layout")).readEntry(QStringLiteral("Options"),QString()).toLatin1();
 const xkb_rule_names names{"evdev","pc105",code.constData(),variant.constData(),options.constData()};
 m_state.reset();m_keymap.reset(xkb_keymap_new_from_names(m_xkb.get(),&names,XKB_KEYMAP_COMPILE_NO_FLAGS));if(m_keymap)m_state.reset(xkb_state_new(m_keymap.get()));updateModifiers();Q_EMIT layoutChanged();
}
void LaptopController::updateModifiers(){if(!m_state)return;const auto pressed=m_held|m_latched;for(uint k:pressed)xkb_state_update_key(m_state.get(),k+8,XKB_KEY_DOWN);applyLocks();Q_EMIT layoutChanged();}
void LaptopController::applyLocks(){
 if(!m_state||!m_keymap)return;
 auto locked=xkb_state_serialize_mods(m_state.get(),XKB_STATE_MODS_LOCKED);
 const auto caps=xkb_keymap_mod_get_index(m_keymap.get(),XKB_MOD_NAME_CAPS);
 const auto num=xkb_keymap_mod_get_index(m_keymap.get(),XKB_MOD_NAME_NUM);
 for(const auto&entry:{std::pair{caps,m_caps},std::pair{num,m_num}})if(entry.first!=XKB_MOD_INVALID){const xkb_mod_mask_t mask=xkb_mod_mask_t(1)<<entry.first;if(entry.second)locked|=mask;else locked&=~mask;}
 xkb_state_update_mask(m_state.get(),xkb_state_serialize_mods(m_state.get(),XKB_STATE_MODS_DEPRESSED),xkb_state_serialize_mods(m_state.get(),XKB_STATE_MODS_LATCHED),locked,0,0,0);
}
void LaptopController::syncLocks(bool caps,bool num,bool scroll){m_caps=caps;m_num=num;m_scroll=scroll;applyLocks();++m_revision;Q_EMIT keyStateChanged();}
bool LaptopController::latched(uint code)const{return code==KEY_CAPSLOCK?m_caps:code==KEY_SCROLLLOCK?m_scroll:m_latched.contains(code)||m_lockedKeys.contains(code);}
bool LaptopController::modifierActive()const{const auto keys=m_held|m_latched;return std::any_of(keys.begin(),keys.end(),modifier);}
QString LaptopController::layoutName()const{return m_layouts.value(m_layoutIndex).toMap().value("label").toString();}
QString LaptopController::locale()const{return UkeKeyboard::LayoutRegistry::system().locale(m_layoutCodes.value(m_layoutIndex,QStringLiteral("us")),m_variants.value(m_layoutIndex));}
bool LaptopController::complexLayout()const{const auto language=QLocale(locale()).language();return language==QLocale::Japanese||language==QLocale::Korean||language==QLocale::Chinese||language==QLocale::Thai;}
bool LaptopController::dictionaryAvailable()const{
 return HunspellDictionary::hasDictionaryFiles(locale());
}
QString LaptopController::dictionaryPackage()const{return QStringLiteral("hunspell-%1").arg(locale().section('_',0,0));}
void LaptopController::systemLayout(uint index){if(!m_options.value("syncLayout").toBool()||index>=uint(m_layoutCodes.size())||int(index)==m_layoutIndex)return;m_layoutIndex=int(index);releaseAll();rebuildKeymap();}
void LaptopController::selectLayout(int index){if(index<0||index>=m_layoutCodes.size())return;releaseAll();m_layoutIndex=index;rebuildKeymap();if(m_options.value("syncLayout").toBool())m_bus.asyncCall(layoutCall(QStringLiteral("setLayout"),{uint(index)}));else{auto config=KSharedConfig::openConfig(QStringLiteral("uke-plasma-keyboard-customrc"));config->group(QStringLiteral("Laptop")).writeEntry(QStringLiteral("IndependentLayout"),index);config->sync();}}
void LaptopController::nextLayout(){selectLayout((m_layoutIndex+1)%std::max(1,int(m_layoutCodes.size())));}
QVariantList LaptopController::allLayouts()const{return UkeKeyboard::LayoutRegistry::system().layouts();}
QVariantList LaptopController::variants(const QString&code)const{return UkeKeyboard::LayoutRegistry::system().variants(code);}
void LaptopController::addLayout(const QString&layout,const QString&variant){
 bool found=false;for(const auto&v:allLayouts())if(v.toMap().value("code")==layout)found=true;if(!found)return;
 if(!variant.isEmpty()){found=false;for(const auto&v:variants(layout))if(v.toMap().value("code")==variant)found=true;if(!found)return;}
 if(m_layoutCodes.size()>=4)return;
 if(!m_options.value("syncLayout").toBool()){
  auto config=KSharedConfig::openConfig(QStringLiteral("uke-plasma-keyboard-customrc"));auto group=config->group(QStringLiteral("Laptop"));auto codes=m_layoutCodes;auto variants=m_variants;
  while(variants.size()<codes.size())variants.append(QString());codes.append(layout);variants.append(variant);
  group.writeEntry(QStringLiteral("VirtualLayoutList"),codes);group.writeEntry(QStringLiteral("VirtualVariantList"),variants);config->sync();loadLayouts();return;
 }
 auto c=KSharedConfig::openConfig(QStringLiteral("kxkbrc"));auto g=c->group(QStringLiteral("Layout"));auto codes=m_layoutCodes;auto vars=m_variants;while(vars.size()<codes.size())vars.append(QString());codes.append(layout);vars.append(variant);
 g.writeEntry(QStringLiteral("Use"),true,KConfig::Notify);g.writeEntry(QStringLiteral("LayoutList"),codes,KConfig::Notify);g.writeEntry(QStringLiteral("VariantList"),vars,KConfig::Notify);c->sync();
 auto signal=QDBusMessage::createSignal(QStringLiteral("/Layouts"),QStringLiteral("org.kde.keyboard"),QStringLiteral("reloadConfig"));m_bus.send(signal);loadLayouts();
}
QVariantList LaptopController::rows()const{
 QVariantList rows;auto row=[&](const std::initializer_list<std::tuple<uint,const char*,double>>&keys){QVariantList r;for(const auto&[code,title,units]:keys){QString text;char buffer[128]{};if(m_state&&code&&xkb_state_key_get_utf8(m_state.get(),code+8,buffer,sizeof(buffer))>0)text=QString::fromUtf8(buffer);QString label=QString::fromUtf8(title);if(label.isEmpty())label=text;if(label.isEmpty())label=QStringLiteral("·");r.append(QVariantMap{{"code",code},{"label",label},{"text",text},{"units",units},{"modifier",modifier(code)},{"dynamic",title[0]==0},{"latched",m_latched.contains(code)||m_lockedKeys.contains(code)}});}rows.append(QVariant(r));};
 if(m_options.value("functionRow").toBool())row({{KEY_ESC,"Esc",1},{KEY_F1,"F1",1},{KEY_F2,"F2",1},{KEY_F3,"F3",1},{KEY_F4,"F4",1},{KEY_F5,"F5",1},{KEY_F6,"F6",1},{KEY_F7,"F7",1},{KEY_F8,"F8",1},{KEY_F9,"F9",1},{KEY_F10,"F10",1},{KEY_F11,"F11",1},{KEY_F12,"F12",1}});
 row({{KEY_GRAVE,"",1},{KEY_1,"",1},{KEY_2,"",1},{KEY_3,"",1},{KEY_4,"",1},{KEY_5,"",1},{KEY_6,"",1},{KEY_7,"",1},{KEY_8,"",1},{KEY_9,"",1},{KEY_0,"",1},{KEY_MINUS,"",1},{KEY_EQUAL,"",1},{KEY_BACKSPACE,"⌫",1.6}});
 row({{KEY_TAB,"Tab",1.4},{KEY_Q,"",1},{KEY_W,"",1},{KEY_E,"",1},{KEY_R,"",1},{KEY_T,"",1},{KEY_Y,"",1},{KEY_U,"",1},{KEY_I,"",1},{KEY_O,"",1},{KEY_P,"",1},{KEY_LEFTBRACE,"",1},{KEY_RIGHTBRACE,"",1},{KEY_BACKSLASH,"",1.2}});
 row({{KEY_CAPSLOCK,"Caps",1.6},{KEY_A,"",1},{KEY_S,"",1},{KEY_D,"",1},{KEY_F,"",1},{KEY_G,"",1},{KEY_H,"",1},{KEY_J,"",1},{KEY_K,"",1},{KEY_L,"",1},{KEY_SEMICOLON,"",1},{KEY_APOSTROPHE,"",1},{KEY_ENTER,"Enter",2}});
 row({{KEY_LEFTSHIFT,"Shift",1.5},{KEY_102ND,"",1},{KEY_Z,"",1},{KEY_X,"",1},{KEY_C,"",1},{KEY_V,"",1},{KEY_B,"",1},{KEY_N,"",1},{KEY_M,"",1},{KEY_COMMA,"",1},{KEY_DOT,"",1},{KEY_SLASH,"",1},{KEY_RIGHTSHIFT,"Shift",2.1}});
 if(m_options.value("modifierRow").toBool())row({{KEY_LEFTCTRL,"Ctrl",1.2},{KEY_LEFTMETA,"Meta",1.2},{KEY_LEFTALT,"Alt",1.2},{KEY_SPACE,"",6.2},{KEY_RIGHTALT,"AltGr",1.2},{KEY_RIGHTCTRL,"Ctrl",1.2},{KEY_COMPOSE,"Menu",1.2}});else row({{KEY_SPACE,"",14.4}});
 if(m_options.value("arrowRow").toBool()||m_options.value("navigationRow").toBool()||m_options.value("systemRow").toBool()){
  QVariantList r;auto add=[&](uint code,const QString&label){r.append(QVariantMap{{"code",code},{"label",label},{"text",QString()},{"units",1},{"modifier",false},{"dynamic",false},{"latched",false}});};
  if(m_options.value("arrowRow").toBool()){add(KEY_LEFT,QStringLiteral("←"));add(KEY_UP,QStringLiteral("↑"));add(KEY_DOWN,QStringLiteral("↓"));add(KEY_RIGHT,QStringLiteral("→"));}
  if(m_options.value("navigationRow").toBool()){add(KEY_INSERT,QStringLiteral("Ins"));add(KEY_DELETE,QStringLiteral("Del"));add(KEY_HOME,QStringLiteral("Home"));add(KEY_END,QStringLiteral("End"));add(KEY_PAGEUP,QStringLiteral("PgUp"));add(KEY_PAGEDOWN,QStringLiteral("PgDn"));}
  if(m_options.value("systemRow").toBool()){add(KEY_SYSRQ,QStringLiteral("PrtSc"));add(KEY_SCROLLLOCK,QStringLiteral("ScrLk"));add(KEY_PAUSE,QStringLiteral("Pause"));}
  rows.append(QVariant(r));
 }
 return rows;
}
void LaptopController::setRawKey(uint code,bool pressed){backendCall(QStringLiteral("Key"),{code,pressed});if(m_state)xkb_state_update_key(m_state.get(),code+8,pressed?XKB_KEY_DOWN:XKB_KEY_UP);}
void LaptopController::key(uint code,bool pressed){
 if(!m_ready||m_locked)return;
 const auto time=m_clock.elapsed();
 if(pressed){
  if(m_held.contains(code))return;m_held.insert(code);m_pressTimes[code]=time;
  if(modifier(code)){
   if(m_lockedKeys.remove(code)){m_latched.remove(code);m_cancelLatch.insert(code);setRawKey(code,false);}
   else if(!m_latched.contains(code))setRawKey(code,true);
  }else{for(uint k:m_held)if(modifier(k))m_modifierUsed.insert(k);setRawKey(code,true);markTyping();}
 }else{
  if(!m_held.remove(code))return;
  if(modifier(code)){
   if(m_cancelLatch.remove(code)){}
   else if(m_modifierUsed.remove(code)||time-m_pressTimes.value(code)>250){m_latched.remove(code);m_lockedKeys.remove(code);setRawKey(code,false);}
   else if(m_latched.contains(code)&&time-m_tapTimes.value(code)<400){m_lockedKeys.insert(code);}
   else if(m_latched.remove(code)){setRawKey(code,false);}
   else{m_latched.insert(code);m_tapTimes[code]=time;}
  }else{setRawKey(code,false);const auto latched=m_latched;for(uint k:latched)if(!m_lockedKeys.contains(k)&&!m_held.contains(k)){setRawKey(k,false);m_latched.remove(k);}}
 }
 ++m_revision;Q_EMIT keyStateChanged();
}
void LaptopController::tap(uint code){key(code,true);key(code,false);}
void LaptopController::markTyping(){if(!m_typing){m_typing=true;Q_EMIT typingChanged();}m_typingTimer.start();}
void LaptopController::moveCursor(int steps){for(int n=0;n<std::min(64,std::abs(steps));n++)tap(steps<0?KEY_LEFT:KEY_RIGHT);}
void LaptopController::deleteWords(int count){if(count<=0)return;const bool temporary=!m_held.contains(KEY_LEFTCTRL)&&!m_latched.contains(KEY_LEFTCTRL);if(temporary)setRawKey(KEY_LEFTCTRL,true);for(int n=0;n<std::min(32,count);n++)tap(KEY_BACKSPACE);if(temporary)setRawKey(KEY_LEFTCTRL,false);}
void LaptopController::releaseAll(){backendCall(QStringLiteral("ReleaseAll"));m_held.clear();m_latched.clear();m_lockedKeys.clear();m_modifierUsed.clear();m_cancelLatch.clear();if(m_keymap)m_state.reset(xkb_state_new(m_keymap.get()));applyLocks();++m_revision;Q_EMIT keyStateChanged();Q_EMIT layoutChanged();}
void LaptopController::motion(double x,double y){const double speed=m_options.value("pointerSpeed").toDouble();backendCall(QStringLiteral("Motion"),{x*speed,y*speed});}
void LaptopController::button(uint code,bool pressed){backendCall(QStringLiteral("Button"),{code,pressed});}
void LaptopController::scroll(double x,double y){const double sign=m_options.value("naturalScroll").toBool()?-1:1;backendCall(QStringLiteral("Scroll"),{x*sign,y*sign});}
void LaptopController::swipe(int fingers,int phase,double x,double y){backendCall(QStringLiteral("Swipe"),{fingers,phase,x,y});}
void LaptopController::pinch(int phase,double scale,double angle,double x,double y){backendCall(QStringLiteral("Pinch"),{phase,scale,angle,x,y});}
void LaptopController::openPreferences(){Q_EMIT preferencesRequested();}
QString LaptopController::toolLabel(const QString&name)const{
 const QHash<QString,QString> labels{{"volume",i18nd("uke-plasma-keyboard-custom", "Volume")},{"brightness",i18nd("uke-plasma-keyboard-custom", "Brightness")},{"media",i18nd("uke-plasma-keyboard-custom", "Media")},{"volume-down",i18nd("uke-plasma-keyboard-custom", "Volume down")},{"volume-up",i18nd("uke-plasma-keyboard-custom", "Volume up")},{"mute",i18nd("uke-plasma-keyboard-custom", "Mute")},{"brightness-down",i18nd("uke-plasma-keyboard-custom", "Decrease brightness")},{"brightness-up",i18nd("uke-plasma-keyboard-custom", "Increase brightness")},{"previous",i18nd("uke-plasma-keyboard-custom", "Previous track")},{"play",i18nd("uke-plasma-keyboard-custom", "Play or pause")},{"next",i18nd("uke-plasma-keyboard-custom", "Next track")},{"clipboard",i18nd("uke-plasma-keyboard-custom", "Clipboard")},{"emoji",i18nd("uke-plasma-keyboard-custom", "Emoji")},{"language",i18nd("uke-plasma-keyboard-custom", "Input language")},{"settings",i18nd("uke-plasma-keyboard-custom", "Keyboard settings")}};
 return labels.value(name,name);
}
void LaptopController::tool(const QString&name){
 const QHash<QString,uint> keys{{"volume-down",KEY_VOLUMEDOWN},{"volume-up",KEY_VOLUMEUP},{"mute",KEY_MUTE},{"brightness-down",KEY_BRIGHTNESSDOWN},{"brightness-up",KEY_BRIGHTNESSUP},{"play",KEY_PLAYPAUSE},{"previous",KEY_PREVIOUSSONG},{"next",KEY_NEXTSONG}};
 if(keys.contains(name)){tap(keys.value(name));return;}
 if(name==QLatin1String("settings"))openPreferences();else if(name==QLatin1String("language"))nextLayout();
}
void LaptopController::refreshProfile(bool restore){if(!restore&&m_active)backendCall(QStringLiteral("Profile"),{m_options.value("refreshProfile").toString()});}
void LaptopController::profileError(const QString&reason){m_reason=reason;Q_EMIT stateChanged();}

QString LaptopController::keyLabel(uint code,const QString&fallback)const{
 if(!fallback.isEmpty())return fallback;
 if(code==KEY_SPACE)return layoutName();
 char text[128]{};if(m_state&&xkb_keysym_to_utf8(xkb_state_key_get_one_sym(m_state.get(),code+8),text,sizeof(text))>1)return QString::fromUtf8(text);return QStringLiteral("·");
}
QStringList LaptopController::alternatives(const QString&text)const{
 static const QHash<QString,QString> values{{"a","áàâäãåæā"},{"e","éèêëē"},{"i","ıíìîïī"},{"ı","iíìîï"},{"o","öóòôõøœō"},{"u","üúùûū"},{"c","çćč"},{"g","ğǧ"},{"s","şßśš"},{"n","ñń"},{"y","ýÿ"},{"z","źžż"}};
 const QString variants=values.value(text.toLower());QStringList result;for(QChar c:variants)result.append(text==text.toUpper()?QString(c).toUpper():QString(c));return result;
}
QVariantMap LaptopController::status()const{return {{"active",active()},{"available",available()},{"ready",m_ready},{"reason",m_reason},{"density",pixelsPerMm()}};}
void LaptopController::refreshRemote(){
 auto*w=new QDBusPendingCallWatcher(m_bus.asyncCall(QDBusMessage::createMethodCall(QStringLiteral("org.uke.plasma.keyboard"),QStringLiteral("/Laptop"),QStringLiteral("org.uke.PlasmaKeyboard.Control1"),QStringLiteral("status"))),this);
 connect(w,&QDBusPendingCallWatcher::finished,this,[this](auto*call){QDBusPendingReply<QVariantMap>r=*call;if(!r.isError()){const auto p=r.value();m_active=p.value("active").toBool();m_ready=p.value("ready").toBool();m_available=p.value("available").toBool();m_locked=false;m_lidClosed=false;m_reason=p.value("reason").toString();m_remoteDensity=p.value("density").toDouble();}else m_reason=i18nd("uke-plasma-keyboard-custom", "Select and start Uke Plasma Keyboard to control laptop mode. Layout and appearance settings can be changed now.");Q_EMIT stateChanged();call->deleteLater();});
}
QVariantList LaptopController::emojiMatches(const QString&query)const{
 struct Entry{QString emoji,name;};
 static QList<Entry> entries;
 static QHash<QString,QString> turkish;
 if(entries.isEmpty()){
  QFile annotations(QStandardPaths::locate(QStandardPaths::GenericDataLocation,QStringLiteral("uke-plasma-keyboard-custom/emoji/tr.xml")));
  if(annotations.open(QIODevice::ReadOnly)){QXmlStreamReader xml(&annotations);while(!xml.atEnd()){xml.readNext();if(xml.isStartElement()&&xml.name()==QLatin1String("annotation")){const QString cp=xml.attributes().value(QLatin1String("cp")).toString();turkish[cp]+=QStringLiteral(" ")+xml.readElementText();}}}
  QFile file(QStandardPaths::locate(QStandardPaths::GenericDataLocation,QStringLiteral("uke-plasma-keyboard-custom/emoji/emoji-test.txt")));
  if(file.open(QIODevice::ReadOnly|QIODevice::Text))while(!file.atEnd()){
   const QString line=QString::fromUtf8(file.readLine());
   if(!line.contains(QLatin1String("; fully-qualified")))continue;
   const QString metadata=line.section('#',1).trimmed();const int space=metadata.indexOf(' ');if(space<0)continue;
   const QString emoji=metadata.left(space);const QString name=metadata.mid(space+1).section(' ',1).trimmed();
   entries.append({emoji,name});
  }
 }
 QVariantList result;const QString needle=query.trimmed().toCaseFolded();
 for(const auto&e:entries)if(needle.isEmpty()||e.name.toCaseFolded().contains(needle)||turkish.value(e.emoji).toCaseFolded().contains(needle)||e.emoji==needle){result.append(QVariantMap{{"emoji",e.emoji},{"name",e.name}});if(result.size()>=120)break;}
 return result;
}
