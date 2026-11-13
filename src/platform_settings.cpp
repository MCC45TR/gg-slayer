// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include <iostream>
#include <memory>
namespace slayer {
std::string cameraPath(const Io&io){
    std::string found;
    for(const auto&name:io.list("/sys/bus/usb/devices")){
        auto p="/sys/bus/usb/devices/"+name;
        if(trim(io.read(p+"/idVendor"))!="2b7e"||trim(io.read(p+"/idProduct"))!="c906"||
           trim(io.read(p+"/bcdDevice"))!="0004"||trim(io.read(p+"/removable"))!="fixed")continue;
        if(!io.exists(p+"/authorized"))continue;
        if(!found.empty())throw Error("Birden fazla eşleşen dahili kamera bulundu; seçim belirsiz.",4);
        found=p+"/authorized";
    }
    return found;
}
void semanticWrite(const Io&io,const std::string&target,const std::string&value,const std::string&old,const std::string&domain){
    io.requireCompatible(); FileLock guard(io,domain);
    // Read again while holding the process lock; do not restore a stale snapshot.
    if(trim(io.read(target))!=old)throw Error("Ayar başka bir işlem tarafından değiştirildi; yeniden deneyin.",4);
    try{
        if(io.dry()||old!=value)io.write(target,value+"\n");
        if(!io.dry()&&trim(io.read(target))!=value)throw Error("Ayar geri okuması uyuşmadı.",4);
    }catch(...){
        if(!io.dry())try{if(trim(io.read(target))!=old)io.write(target,old+"\n");if(trim(io.read(target))!=old)throw Error("Geri okuma uyuşmadı.");}
        catch(...){throw Error("Ayar başarısız; önceki değer geri yüklenemedi.",4);}
        throw;
    }
}
void platformSetting(const Io&io,const std::vector<std::string>&a){
    const std::map<std::string,std::string> attributes={{"fn-lock","fn_lock"},{"win-lock","win_lock"},{"usb-charging","usb_sleep_charging"},{"ac-recovery","ac_auto_boot"},{"camera",""}};
    if(a.empty()||!attributes.contains(a[0]))throw Error("Ayarlar: fn-lock, win-lock, usb-charging, ac-recovery, camera; değer on|off|status");
    if(a.size()!=2||(a[1]!="on"&&a[1]!="off"&&a[1]!="status"))throw Error("Kullanım: settings AYAR on|off|status");
    if(a[1]!="status")io.requireCompatible();
    std::string target;
    if(a[0]=="camera")target=cameraPath(io);
    else for(const auto&base:{std::string("/sys/devices/platform/slayerctl"),std::string("/sys/devices/platform/uniwill-laptop")})
        if(io.exists(base+"/"+attributes.at(a[0]))){target=base+"/"+attributes.at(a[0]);break;}
    if(target.empty()){
        if(a[0]=="usb-charging"&&trim(io.read("/sys/devices/platform/slayerctl/usb_sleep_charging_supported"))=="0")
            throw Error("EC bu BIOS/model için kapalı USB şarj desteği bildirmiyor; komut gönderilmedi.",4);
        unverified(a[0]);
    }
    auto old=trim(io.read(target));integer(old,0,1,"Kernel ayarı");
    if(a[1]=="status"){std::cout<<a[0]<<": "<<old<<" ("<<(old=="1"?"on":"off")<<")\n";return;}
    semanticWrite(io,target,a[1]=="on"?"1":"0",old,"settings");
    std::cout<<(io.dry()?"Ayar önizlemesi tamamlandı.":"Kernel ayarı uygulandı.")<<'\n';
    if(a[0]=="camera")std::cout<<"Dahili RGB/IR kamera: Linux USB yetkilendirmesi. Firmware E-shutter değildir. Açık görüntülü görüşme kesilebilir.\n";
}
bool acOnline(const Io&io){
    bool found=false,online=false;
    for(const auto&n:io.list("/sys/class/power_supply")){
        auto p="/sys/class/power_supply/"+n;auto type=trim(io.read(p+"/type"));
        if(type!="Mains"&&type!="USB"&&type!="USB_C"&&type!="USB_PD"&&type!="USB_PD_DRP")continue;
        auto state=trim(io.read(p+"/online"));integer(state,0,1,"Adaptör durumu");found=true;online|=state=="1";
    }
    if(!found)throw Error("Adaptör online arayüzü bulunamadı; AC/DC profili seçilmedi.",4);
    return online;
}
}
