// SPDX-License-Identifier: MIT
#include "profile_sync.hpp"
#include "rgb.hpp"
#include "controls.hpp"
#include <iostream>
namespace slayer {
namespace {
const std::string testDesktop="/run/slayerctl/test-desktop-profile";
std::string desktopProfile(const Io& io){
    if(io.fixture())return trim(io.read(testDesktop));
    auto text=capture({"/usr/libexec/slayerctl-desktop","--power-list"},7000);
    auto begin=text.find("profile: ");
    if(begin==std::string::npos)throw Error("Masaüstü profili okunamadı.",4);
    begin+=9;return trim(text.substr(begin,text.find('\n',begin)-begin));
}
void requireSyncSupport(const Io& io){
    const auto profilePath=hardwareProfilePath(io);
    if(!io.exists("/sys/devices/platform/slayerctl/thermal_mode"))throw Error("Bağımsız fabrika modu için güncel R9T sürücüsü gerekli.",4);
    io.requireCompatible();
    const auto choices=words(io.read(profilePath+"_choices"));
    for(auto required:{"low-power","balanced","performance"}){
        bool found=false;for(const auto& choice:choices)if(choice==required)found=true;
        if(!found)throw Error("Eşitleme için Office, Balance ve Performance sunan güncel R9T sürücüsü gerekli.",4);
    }
    if(!trim(io.read("/sys/devices/platform/slayerctl/profile_events")).starts_with("acpi-query"))
        throw Error("Fiziksel mod bildirimleri için güncel R9T ACPI olay izleyicisi gerekli.",4);
}
}
void synchronizeProfiles(const Io& io,bool fromHardware){
    const auto profilePath=hardwareProfilePath(io);
    const auto config=configurationValues(io);
    if(!config.contains("performance_sync")||config.at("performance_sync")!="on")return;
    requireSyncSupport(io);
    const auto current=trim(io.read(profilePath));
    const auto desktop=desktopProfile(io);
    const auto decision=planProfileSync(true,fromHardware,current,desktop,words(io.read(profilePath+"_choices")));
    if(decision.action==ProfileSyncPlan::None)return;
    if(decision.action==ProfileSyncPlan::Hardware){profile(io,{"set",decision.target});return;}
    if(io.dry())std::cout<<"Önizleme: KDE güç profili = "<<decision.target<<'\n';
    else if(io.fixture())saveText(io,testDesktop,decision.target+"\n");
    else std::cout<<capture({"/usr/libexec/slayerctl-desktop","--power-set",decision.target},7000)<<'\n';
}
void profileSynchronization(const Io& io,const std::vector<std::string>& args){
    const auto profilePath=hardwareProfilePath(io);
    if(args.size()!=1)throw Error("Kullanım: profile sync on|off|status|from-hardware|from-desktop");
    const auto& mode=args[0];
    if(mode=="status"){
        const auto cfg=configurationValues(io);
        std::cout<<"performance_sync: "<<(cfg.contains("performance_sync")?cfg.at("performance_sync"):"off")<<'\n';return;
    }
    if(mode=="from-hardware"||mode=="from-desktop"){synchronizeProfiles(io,mode=="from-hardware");return;}
    if(mode=="off"){configuration(io,{"set","performance_sync","off"});return;}
    if(mode!="on")throw Error("Geçersiz eşitleme işlemi.");
    requireSyncSupport(io);
    // Validate both current states before persisting consent or writing anything.
    const auto hardware=trim(io.read(profilePath));
    const auto desktop=desktopProfile(io);
    planProfileSync(true,true,hardware,desktop,words(io.read(profilePath+"_choices")));
    if(io.dry()){std::cout<<"Önizleme: KDE ile eşitle; ilk açılışta donanım modu esas alınır.\n";return;}
    const auto previous=configurationValues(io);
    configuration(io,{"set","performance_sync","on"});
    try{synchronizeProfiles(io,true);}
    catch(...){
        try{if(previous.contains("performance_sync"))configuration(io,{"set","performance_sync",previous.at("performance_sync")});
            else configuration(io,{"unset","performance_sync"});}
        catch(...){throw Error("Eşitleme başarısız; kayıt geri alınamadı. profile sync off ile kapatın.",4);}
        throw;
    }
}
}
