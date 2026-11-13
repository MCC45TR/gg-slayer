// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include <algorithm>
#include <iostream>
#include <exception>
#include <set>
namespace slayer {
static std::string get(const Io&io,const std::string&p){return trim(io.read(p));}
std::vector<std::string> cpuPolicies(const Io&io){
    std::vector<std::string> paths;
    for(const auto&n:io.list("/sys/devices/system/cpu/cpufreq")){
        if(!n.starts_with("policy")||n.size()<7||n.substr(6).find_first_not_of("0123456789")!=std::string::npos)continue;
        auto p="/sys/devices/system/cpu/cpufreq/"+n;
        if(io.exists(p+"/scaling_max_freq"))paths.push_back(p);
    }
    if(paths.size()>256)throw Error("CPU policy sınırı aşıldı.");
    return paths;
}
void cpuControl(const Io&io,const std::vector<std::string>&a){
    const std::string smt="/sys/devices/system/cpu/smt/control";
    if(!a.empty()&&(a[0]=="topology"||a[0]=="ccd")){
        if((a[0]=="topology"&&a.size()!=1)||(a[0]=="ccd"&&a.size()>1&&!(a.size()==2&&a[1]=="status")))
            throw Error("CCD kapatma BIOS'a aittir; Linux hotplug firmware CCD güç kapatma işlemi olarak sunulmaz.",4);
        std::set<std::string> cores,ccds;unsigned int online=0;
        for(const auto&n:io.list("/sys/devices/system/cpu")){
            if(!n.starts_with("cpu")||n.size()<4||n.substr(3).find_first_not_of("0123456789")!=std::string::npos)continue;
            const auto p="/sys/devices/system/cpu/"+n;
            const auto core=get(io,p+"/topology/core_id"),pkg=get(io,p+"/topology/physical_package_id");
            if(core.empty()||pkg.empty())continue;
            cores.insert(pkg+":"+core);
            auto shared=get(io,p+"/cache/index3/shared_cpu_list");if(!shared.empty())ccds.insert(shared);
            if(!io.exists(p+"/online")||get(io,p+"/online")=="1")++online;
        }
        std::cout<<"Fiziksel çekirdek: "<<cores.size()<<"\nÇevrimiçi iş parçacığı: "<<online<<"\nSMT: "<<get(io,smt)<<"\n";
        for(const auto&group:ccds)std::cout<<"L3 / CCD grubu CPU'ları: "<<group<<'\n';
        std::cout<<"BIOS: CCD seçimi Setup 0xd0; SMT 0xd1. Doğrudan Setup yazması sunulmaz.\n";return;
    }
    if(!a.empty()&&a[0]=="smt"){
        if(a.size()!=2||(a[1]!="on"&&a[1]!="off"&&a[1]!="status"))throw Error("Kullanım: cpu smt on|off|status");
        if(!io.exists(smt))throw Error("Kernel SMT kontrolü sunmuyor.",4);
        auto old=get(io,smt);
        if(a[1]=="status"){std::cout<<"SMT: "<<old<<"\nAktif: "<<get(io,"/sys/devices/system/cpu/smt/active")<<"\nCPU online: "<<get(io,"/sys/devices/system/cpu/online")<<'\n';return;}
        io.requireCompatible();
        if(old!="on"&&old!="off")throw Error("SMT durumu on/off değil; forceoff veya BIOS kilidi aşılmaz.",4);
        semanticWrite(io,smt,a[1],old,"cpu");
        std::cout<<(io.dry()?"SMT önizlemesi.":"SMT kernel ayarı uygulandı.")<<" Fiziksel çekirdek kapatma değildir.\n";return;
    }
    const std::string base="/sys/devices/system/cpu/cpufreq";
    auto policies=cpuPolicies(io);
    if(policies.empty())throw Error("CPUFreq arayüzü bulunamadı.",4);
    if(a.empty()||(a.size()==1&&a[0]=="status")){
        std::cout<<"CPU policy sayısı: "<<policies.size()<<"\nGlobal boost: "<<get(io,base+"/boost")<<'\n';
        for(const auto&p:policies)std::cout<<fs::path(p).filename().string()<<": "<<get(io,p+"/scaling_driver")<<" / "<<get(io,p+"/scaling_min_freq")<<".."<<get(io,p+"/scaling_max_freq")<<" kHz / donanım="<<get(io,p+"/cpuinfo_min_freq")<<".."<<get(io,p+"/cpuinfo_max_freq")<<" kHz / boost="<<get(io,p+"/boost")<<'\n';
        return;
    }
    if(a[0]=="power"||a[0]=="voltage"||a[0]=="overclock")unverified("CPU watt/voltaj/OC");
    io.requireCompatible();
    FileLock guard(io,"cpu");
    std::vector<std::pair<std::string,std::string>> changes,saved;
    auto schedule=[&](const std::string&p,int value){
        auto old=get(io,p);integer(old,p.ends_with("/boost")?0:1,p.ends_with("/boost")?1:20000000,"CPU eski değer");
        if(!io.exists(p))throw Error("CPU arayüzü eksik.",4);
        saved.emplace_back(p,old);changes.emplace_back(p,std::to_string(value));
    };
    if(a[0]=="boost"&&a.size()==2&&(a[1]=="on"||a[1]=="off")){
        const int value=a[1]=="on"?1:0;bool found=false;
        if(io.exists(base+"/boost")){schedule(base+"/boost",value);found=true;}
        for(const auto&p:policies)if(io.exists(p+"/boost")){schedule(p+"/boost",value);found=true;}
        if(!found)throw Error("Kernel boost kontrolü sunmuyor.",4);
    }else if(a[0]=="limits"&&(a.size()==3||(a.size()==2&&a[1]=="reset"))){
        const bool reset=a.size()==2;int minimum=0,maximum=0;
        if(!reset){minimum=integer(a[1],100,10000,"Alt frekans MHz")*1000;maximum=integer(a[2],100,10000,"Üst frekans MHz")*1000;if(minimum>maximum)throw Error("Alt frekans üstten büyük olamaz.");}
        for(const auto&p:policies){
            int lo=integer(get(io,p+"/cpuinfo_min_freq"),1,20000000,"Donanım alt sınırı");
            int hi=integer(get(io,p+"/cpuinfo_max_freq"),lo,20000000,"Donanım üst sınırı");
            int oldMax=integer(get(io,p+"/scaling_max_freq"),lo,hi,"Geçerli üst sınır");
            integer(get(io,p+"/scaling_min_freq"),lo,oldMax,"Geçerli alt sınır");
            const int targetMin=reset?lo:minimum,targetMax=reset?hi:maximum;
            if(targetMin<lo||targetMax>hi)throw Error("İstek kernelin bildirdiği frekans aralığı dışında. Boost kapalıysa üst sınır daha düşük olabilir.",4);
            if(targetMin>oldMax){schedule(p+"/scaling_max_freq",targetMax);schedule(p+"/scaling_min_freq",targetMin);}
            else{schedule(p+"/scaling_min_freq",targetMin);schedule(p+"/scaling_max_freq",targetMax);}
        }
    }else throw Error("Kullanım: cpu status | cpu boost on|off | cpu limits ALT_MHz ÜST_MHz | cpu limits reset");
    std::vector<std::size_t> attempted;
    try{
        for(std::size_t i=0;i<changes.size();++i){
            const auto&[path,value]=changes[i];
            if(io.dry()||get(io,path)!=value){attempted.push_back(i);io.write(path,value+"\n");}
            if(!io.dry()&&get(io,path)!=value)throw Error("CPU ayarı geri okuması uyuşmadı.",4);
        }
    }catch(...){
        auto failure=std::current_exception();bool restored=true;
        std::set<std::string> affected;
        bool globalBoost=false;
        for(auto i:attempted){affected.insert(saved[i].first);globalBoost|=saved[i].first==base+"/boost";}
        if(!io.dry()){
            // A global boost store can affect all policies even before their
            // explicit writes. Otherwise only undo paths actually attempted.
            for(const auto&[path,value]:saved)if(path.ends_with("/boost")&&(globalBoost||affected.contains(path)))
                try{if(get(io,path)!=value)io.write(path,value+"\n");}catch(...){restored=false;}
            for(const auto& policy:policies){
                const auto loPath=policy+"/scaling_min_freq",hiPath=policy+"/scaling_max_freq";
                if(!affected.contains(loPath)&&!affected.contains(hiPath))continue;
                affected.insert(loPath);affected.insert(hiPath);
                std::string oldMin,oldMax;
                for(const auto&[path,value]:saved){if(path==loPath)oldMin=value;if(path==hiPath)oldMax=value;}
                try{
                    const int lo=integer(oldMin,1,20000000,"Önceki alt frekans");
                    integer(oldMax,lo,20000000,"Önceki üst frekans");
                    const int liveMax=integer(get(io,hiPath),1,20000000,"Canlı üst frekans");
                    // Restore the old envelope directly; never open a temporary
                    // CPU maximum above both the old and requested limits.
                    if(lo>liveMax){
                        io.write(hiPath,oldMax+"\n");io.write(loPath,oldMin+"\n");
                    }else{
                        io.write(loPath,oldMin+"\n");io.write(hiPath,oldMax+"\n");
                    }
                }catch(...){restored=false;}
            }
            for(const auto&[path,value]:saved)if(affected.contains(path)||(globalBoost&&path.ends_with("/boost")))
                if(get(io,path)!=value)restored=false;
        }
        if(!restored)throw Error("CPU işlemi başarısız ve önceki durum tam geri yüklenemedi; cpu status ile denetleyin.",4);
        std::rethrow_exception(failure);
    }
    std::cout<<(io.dry()?"CPU önizlemesi tamamlandı.":"CPU kernel isteği uygulandı. Masaüstü güç yöneticisi daha sonra bu ayarları değiştirebilir.")<<'\n';
}
}
