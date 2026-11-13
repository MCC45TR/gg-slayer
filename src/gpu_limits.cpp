// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include <charconv>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <unistd.h>
namespace slayer {
static constexpr auto testBase="/sys/devices/platform/slayerctl/test-gpu";
static int milliwatts(std::string value){
    value=trim(value);if(value=="[N/A]"||value=="N/A"||value.empty())return -1;
    auto dot=value.find('.');auto whole=value.substr(0,dot);int watts=integer(whole,0,10000,"GPU watt");int fraction=0;
    if(dot!=std::string::npos){auto digits=value.substr(dot+1);if(digits.empty()||digits.size()>3||digits.find_first_not_of("0123456789")!=std::string::npos)throw Error("GPU watt biçimi hatalı.");while(digits.size()<3)digits+='0';fraction=integer(digits,0,999,"Watt kesri");}
    return watts*1000+fraction;
}
static std::string gpuId(const Io&io){
    if(trim(io.read("/sys/class/dmi/id/sys_vendor"))!="GAME GARAJ"||trim(io.read("/sys/class/dmi/id/product_name"))!="SLAYER R9T")throw Error("GPU güç yazması bu cihaz kimliği için tanımlı değil.",4);
    for(const auto&n:io.list("/sys/bus/pci/devices")){
        auto p="/sys/bus/pci/devices/"+n;
        if(n.size()!=12||n[4]!=':'||n[7]!=':'||n[10]!='.'||n.find_first_not_of("0123456789abcdef:.")!=std::string::npos)continue;
        if(trim(io.read(p+"/vendor"))=="0x10de"&&trim(io.read(p+"/device"))=="0x2d19"&&trim(io.read(p+"/subsystem_vendor"))=="0x1d05"&&trim(io.read(p+"/subsystem_device"))=="0x605c")return n;
    }
    throw Error("R9T RTX5060 PCI kimliği doğrulanamadı.",4);
}
struct Limits{int low,high,standard,selected;};
static Limits readLimits(const Io&io,const std::string&id){
    std::string data=io.fixture()?io.read(std::string(testBase)+"/limits"):capture({"/usr/bin/nvidia-smi","--id="+id,"--query-gpu=power.min_limit,power.max_limit,power.default_limit,power.limit","--format=csv,noheader,nounits"},3000);
    std::vector<std::string> fields;std::size_t pos=0;
    while(pos<data.size()){auto end=data.find(',',pos);if(end==std::string::npos)end=data.size();fields.push_back(data.substr(pos,end-pos));pos=end+1;}
    if(fields.size()!=4)throw Error("NVIDIA güç bilgisi biçimi tanınmadı.",4);
    Limits l{milliwatts(fields[0]),milliwatts(fields[1]),milliwatts(fields[2]),milliwatts(fields[3])};
    if(io.fixture())l.selected=milliwatts(io.read(std::string(testBase)+"/power_limit"));
    if(l.low<0||l.high<l.low||l.standard<l.low||l.standard>l.high)throw Error("GPU güç sınırları geçerli değil.",4);
    return l;
}
static std::string watts(int mw){if(mw<0)return "N/A";std::ostringstream s;s<<std::fixed<<std::setprecision(3)<<mw/1000.0;return s.str();}
static void set(const Io&io,const std::string&id,int mw){
    if(io.fixture())io.write(std::string(testBase)+"/power_limit",watts(mw)+"\n");
    else capture({"/usr/bin/nvidia-smi","--id="+id,"--power-limit="+watts(mw)},3000);
}
void gpuLimits(const Io&io,const std::vector<std::string>&a){
    if(a.empty()||((a[0]=="limits")&&a.size()!=1)||(a[0]!="limits"&&(a[0]!="power-limit"||a.size()!=2)))throw Error("Kullanım: gpu limits | gpu power-limit WATT|reset");
    if(a[0]=="power-limit"&&a[1]!="reset")integer(a[1],1,10000,"GPU watt");
    if(a[0]=="power-limit")io.requireCompatible();
    auto id=gpuId(io);
    std::unique_ptr<FileLock> guard;
    if(a[0]=="power-limit")guard=std::make_unique<FileLock>(io,"gpu");
    auto l=readLimits(io,id);bool writable=l.selected>=l.low&&l.selected<=l.high;
    if(a[0]=="limits"){
        std::cout<<"GPU limits\npci: "<<id<<"\npower_min: "<<watts(l.low)<<"\npower_max: "<<watts(l.high)<<"\npower_default: "<<watts(l.standard)<<"\npower_current: "<<watts(l.selected)<<"\npower_settable: "<<(writable?"candidate":"no")<<"\n";
        std::cout<<(writable?"Değerler okunuyor; sürücü yazmayı ayrıca reddedebilir.\n":"Mevcut limit N/A veya geçersiz; geri yükleme/okuma doğrulanamadığından güç yazması kapalı.\n");return;
    }
    if(!writable)throw Error("NVIDIA mevcut güç limitini bildirmiyor; min/max tek başına yazma desteği değildir. GPU'ya değer yazılmadı.",4);
    int desired=a[1]=="reset"?l.standard:integer(a[1],1,10000,"GPU watt")*1000;
    if(desired<l.low||desired>l.high)throw Error("GPU isteği donanımın bildirdiği sınırlar dışında.");
    if(io.dry()){std::cout<<"Önizleme: GPU "<<id<<" güç limiti "<<watts(desired)<<" W.\n";return;}
    if(!io.fixture()&&geteuid()!=0)throw Error("GPU güç limiti için sudo gerekir.",3);
    try{if(desired!=l.selected)set(io,id,desired);if(readLimits(io,id).selected!=desired)throw Error("GPU güç limiti geri okuması uyuşmadı.",4);}
    catch(...){
        try{set(io,id,l.selected);if(readLimits(io,id).selected!=l.selected)throw Error("GPU geri okuması uyuşmadı.");}
        catch(...){throw Error("GPU işlemi başarısız; eski limit geri yüklenemedi. gpu limits ile denetleyin.",4);}
        throw;
    }
    std::cout<<"GPU sürücüsü güç limiti isteği uygulandı; bu OEM cTGP/DynamicBoost kilidini açmaz.\n";
}
}
