// SPDX-License-Identifier: GPL-2.0-or-later
#include "nativerefreshbackend.h"
#include "nativerefreshrequest.h"
#include <core/backendoutput.h>
#include <core/output.h>
#include <workspace.h>

namespace {
KWin::BackendOutput *findPanel(const QString &name)
{
 if(!KWin::workspace())return nullptr;
 for(auto *output:KWin::workspace()->outputs()){
  auto *panel=output->backendOutput();
  if(output->name()==name&&output->isInternal()&&panel&&panel->isInternal()&&panel->isEnabled()&&panel->currentMode())return panel;
 }
 return nullptr;
}
RefreshGuard::Mode describe(const std::shared_ptr<KWin::OutputMode> &mode)
{
 return {mode->size(),int(mode->refreshRate())};
}
}

RefreshGuard::Backend nativeRefreshBackend()
{
 static_assert(int(KWin::VrrPolicy::Never)==0&&int(KWin::VrrPolicy::Always)==1&&int(KWin::VrrPolicy::Automatic)==2);
 return {
  [](const QString &name)->std::optional<RefreshGuard::Snapshot>{
   auto *panel=findPanel(name);if(!panel)return std::nullopt;
   RefreshGuard::Snapshot state{describe(panel->currentMode()),{},int(panel->vrrPolicy())};
   for(const auto &mode:panel->modes())if(!mode->isRemoved())state.modes.append(describe(mode));
   return state;
  },
  [](const QString &name,const RefreshGuard::Mode &requested,int policy){
   auto *panel=findPanel(name);if(!panel||policy<0||policy>2)return false;
   for(const auto &mode:panel->modes()){
    if(mode->isRemoved()||describe(mode)!=requested)continue;
    KWin::OutputConfiguration config;
    config.source=KWin::OutputConfiguration::Source::System;
    *config.changeSet(panel)=UkeKeyboard::refreshChanges(mode->modeline(),KWin::VrrPolicy(policy));
    // Resolve and apply synchronously on the compositor thread. No copied
    // KScreen graph can overwrite intervening user/display configuration.
    return bool(KWin::workspace()->applyOutputConfiguration(config));
   }
   return false;
  }
 };
}
