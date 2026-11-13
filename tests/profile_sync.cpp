// SPDX-License-Identifier: MIT
#include "../src/profile_sync.hpp"
#include <iostream>
int main(){
    const std::vector<std::string> choices{"low-power","balanced","performance"};
    const std::vector<std::string> desktops{"power-saver","balanced","performance"};
    unsigned count=0;
    for(unsigned h=0;h<3;++h)for(unsigned d=0;d<3;++d)for(bool hardware:{false,true}){
        auto plan=slayer::planProfileSync(true,hardware,choices[h],desktops[d],choices);
        if(h==d){if(plan.action!=slayer::ProfileSyncPlan::None)return 1;}
        else if(hardware){if(plan.action!=slayer::ProfileSyncPlan::Desktop||plan.target!=desktops[h])return 2;}
        else if(plan.action!=slayer::ProfileSyncPlan::Hardware||plan.target!=choices[d])return 3;
        if(slayer::planProfileSync(false,hardware,choices[h],desktops[d],choices).action!=slayer::ProfileSyncPlan::None)return 4;
        ++count;
    }
    try{slayer::planProfileSync(true,true,"custom","balanced",choices);return 5;}catch(const slayer::Error&){}
    try{slayer::planProfileSync(true,false,"balanced","power-saver",{"balanced","performance"});return 6;}catch(const slayer::Error&){}
    try{slayer::planProfileSync(true,false,"balanced","unknown",choices);return 7;}catch(const slayer::Error&){}
    std::cout<<count<<" two-way transitions, independent mode, unknown state and missing capability verified.\n";
}
