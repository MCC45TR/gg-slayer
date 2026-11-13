// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
namespace slayer {
struct ProfileSyncPlan { enum Action { None, Hardware, Desktop } action=None; std::string target; };
inline std::string desktopForHardware(const std::string& value){
    if(value=="low-power")return "power-saver";
    if(value=="balanced"||value=="performance")return value;
    throw Error("Donanım profili tanınmadı; eşitleme uygulanmadı.",4);
}
inline ProfileSyncPlan planProfileSync(bool enabled,bool fromHardware,const std::string& hardware,
                                      const std::string& desktop,const std::vector<std::string>& choices){
    if(!enabled)return {};
    auto mapped=desktopForHardware(hardware);
    if(desktop!="power-saver"&&desktop!="balanced"&&desktop!="performance")throw Error("Masaüstü profili tanınmadı.",4);
    if(mapped==desktop)return {};
    if(fromHardware)return {ProfileSyncPlan::Desktop,mapped};
    auto desired=desktop=="power-saver"?"low-power":desktop;
    bool present=false;for(const auto& choice:choices)if(choice==desired)present=true;
    if(!present)throw Error("Kernel istenen fabrika modunu sunmuyor.",4);
    return {ProfileSyncPlan::Hardware,desired};
}
void synchronizeProfiles(const Io&,bool fromHardware);
void profileSynchronization(const Io&,const std::vector<std::string>&);
}
