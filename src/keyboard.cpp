// SPDX-License-Identifier: MIT
#include "rgb_transaction.hpp"
#include <exception>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <thread>
#include <iostream>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace slayer {
static bool colorLed(const Led &l) {
    return l.channels.size()==3 && std::find(l.channels.begin(),l.channels.end(),"red")!=l.channels.end()
        && std::find(l.channels.begin(),l.channels.end(),"green")!=l.channels.end()
        && std::find(l.channels.begin(),l.channels.end(),"blue")!=l.channels.end();
}
static std::array<int,3> parseColor(std::string s) {
    if(s=="white")s="ffffff";else if(s=="red")s="ff0000";else if(s=="green")s="00ff00";
    else if(s=="blue")s="0000ff";else if(s=="off")s="000000";
    if(s.starts_with('#'))s.erase(0,1);
    if(s.size()!=6)throw Error("Renk altı onaltılık basamak olmalı (örnek: 3DAEE9).");
    unsigned value=0;auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),value,16);
    if(ec!=std::errc{}||end!=s.data()+s.size())throw Error("Geçersiz RGB rengi.");
    return {static_cast<int>((value>>16)&255),static_cast<int>((value>>8)&255),static_cast<int>(value&255)};
}
void keyboard(const Io &io,const std::vector<std::string>&a) {
    if(a==std::vector<std::string>{"idle-reset"}){
        if(geteuid()!=0&&!io.fixture())throw Error("Geçici parlaklık ölçeğini yalnız servis sıfırlayabilir.",3);
        for(const auto& name:{"keyboard_activity_arm","keyboard_activity_monitor"}){
            const std::string path=std::string("/sys/devices/platform/slayerctl/")+name;
            if(io.exists(path))io.write(path,"0\n");
        }
        for(const auto& name:io.list("/sys/bus/hid/devices"))if(name.starts_with("0003:048D:600B.")){
            const auto path="/sys/bus/hid/devices/"+name+"/controls/brightness_scale";
            if(io.exists(path))io.write(path,"1000\n");
        }
        return;
    }
    if(!a.empty()&&a[0]=="accent") {
        if(a.size()!=2||(a[1]!="on"&&a[1]!="off"&&a[1]!="status"))throw Error("Kullanım: keyboard accent on|off|status");
        if(a[1]=="on")io.requireCompatible();
        if(io.fixture())throw Error("Test kipinde oturum D-Bus'una erişilmez.",4);
        if(geteuid()==0)throw Error("KDE vurgu rengi komutunu sudo olmadan, kendi oturumunuzda çalıştırın.");
        if(io.dry()){std::cout<<"Önizleme: KDE vurgu rengi "<<a[1]<<'\n';return;}
        std::cout<<capture({"/usr/libexec/slayerctl-desktop","--accent",a[1]},7000);return;
    }
    if(!a.empty()&&a[0]=="layout"){layout(io,{a.begin()+1,a.end()});return;}
    if(!a.empty()&&a[0]=="color-profile"){colorProfile(io,{a.begin()+1,a.end()});return;}
    if(!a.empty()&&a[0]=="profile"){rgbProfile(io,{a.begin()+1,a.end()});return;}
    if(!a.empty()&&a[0]=="hardware"){hardwareEffect(io,{a.begin()+1,a.end()});return;}
    if(!a.empty()&&a[0]=="identify"){
        if(a.size()!=2)throw Error("Kullanım: keyboard identify LED_İNDEKSİ");
        locate(io,integer(a[1],0,125,"LED indeksi"));return;
    }
    const auto&keys=leds(io);
    if(keys.empty())throw Error("Klavye LED aygıtı bulunamadı. 'slayerctl doctor' ile sürücü/MOK durumunu kontrol edin.",4);
    if(a.empty()||(a.size()==1&&a[0]=="status")) {
        std::cout<<"LED sayısı: "<<keys.size()<<"\nParlaklık: "<<trim(io.read(keys.front().path+"/brightness"))<<" / "<<keys.front().maximum<<'\n';
        std::cout<<"RGB: "<<trim(io.read(keys.front().path+"/multi_intensity"))<<"\nTuş indeksleri: ";
        for(const auto&k:keys)std::cout<<k.index<<' ';
        std::cout<<'\n';return;
    }
    if(a[0]=="white-point") {
        if((a.size()!=4&&a.size()!=5)||(a.size()==5&&a[4]!="--white"))throw Error("Kullanım: keyboard white-point R G B [--white]");
        const auto device=hardwareDevice(io);
        const auto target="/sys/class/hidraw/"+device+"/device/controls/white_point";
        if(device.empty()||!io.exists(target))throw Error("Beyaz dengesi için güncel R9T LED sürücüsü gerekli.",4);
        std::string point;
        for(unsigned i=1;i<=3;++i){if(i>1)point+=' ';point+=std::to_string(integer(a[i],1,255,"Beyaz noktası"));}
        const bool white=a.size()==5;
        RgbTransaction transaction(io,white);
        const auto previous=trim(io.read(target));
        const auto parts=words(previous);
        if(parts.size()!=3)throw Error("Önceki beyaz noktası okunamadı.",4);
        for(const auto& value:parts)integer(value,1,255,"Önceki beyaz noktası");
        if(previous==point&&!white){std::cout<<"Beyaz noktası zaten seçili.\n";return;}
        std::vector<std::pair<std::string,std::string>> colors;
        if(white)for(const auto& key:keys){auto value=trim(io.read(key.path+"/multi_intensity"));validateSavedColor(key,value);colors.emplace_back(key.path+"/multi_intensity",value);}
        bool attempted=false;
        try {
            attempted=true;transaction.begin();io.write(target,point+"\n");
            if(white)for(const auto& key:keys){std::string value;for(auto maximum:key.channelMax){if(!value.empty())value+=' ';value+=std::to_string(maximum);}io.write(key.path+"/multi_intensity",value+"\n");}
            transaction.finish();
            if(!io.dry()&&trim(io.read(target))!=point)throw Error("Beyaz noktası geri okuması uyuşmadı.",4);
        } catch(...) {
            bool restored=true;
            if(attempted&&!io.dry()){
                try{transaction.begin();}catch(...){restored=false;}
                try{io.write(target,previous+"\n");}catch(...){restored=false;}
                for(const auto& [path,value]:colors)try{io.write(path,value+"\n");}catch(...){restored=false;}
                try{transaction.finish();}catch(...){restored=false;}
                try{transaction.restoreEffect();}catch(...){restored=false;}
            }
            if(!restored)throw Error("Kalibrasyon uygulanamadı; önceki renkler tam geri yüklenemedi.",4);
            throw;
        }
        std::cout<<"Klavye beyaz noktası: "<<point<<".\n";return;
    }
    if(a[0]=="brightness") {
        if(a.size()!=2)throw Error("Kullanım: keyboard brightness 0..100");
        int percent=integer(a[1],0,100,"Parlaklık");
        RgbTransaction transaction(io,false);
        // ite_8291 has one shared hardware brightness register. Its kernel
        // callback updates every key's cached brightness: one write suffices.
        bool shared=io.exists("/sys/module/ite_8291")&&keys.front().path.ends_with("rgb:kbd_backlight");
        std::size_t count=shared?1:keys.size();
        std::vector<std::string> previous;
        for(std::size_t i=0;i<count;++i){
            auto old=trim(io.read(keys[i].path+"/brightness"));integer(old,0,keys[i].maximum,"Önceki parlaklık");previous.push_back(old);
        }
        std::size_t attempted=0;
        try{
            for(std::size_t i=0;i<count;++i){
                auto value=std::to_string((percent*keys[i].maximum+50)/100);auto target=keys[i].path+"/brightness";
                attempted=i+1;
                io.write(target,value+"\n"); // HID effects can change real brightness without updating the LED cache.
                if(!io.dry()&&trim(io.read(target))!=value)throw Error("Parlaklık geri okuması uyuşmadı.",4);
            }
        }catch(...){
            bool restored=true;
            if(!io.dry())for(std::size_t i=0;i<attempted;++i)try{
                io.write(keys[i].path+"/brightness",previous[i]+"\n");
                if(trim(io.read(keys[i].path+"/brightness"))!=previous[i])restored=false;
            }catch(...){restored=false;}
            if(!restored)throw Error("Parlaklık uygulanamadı; önceki durum tam geri yüklenemedi.",4);
            throw;
        }
        std::cout<<(io.dry()?"Parlaklık önizlemesi tamamlandı.":"Klavye parlaklığı ayarlandı.")<<'\n';return;
    }
    if(a[0]=="effect"){effect(io,a);return;}
    if(a[0]=="reset") {
        if(a.size()!=1)throw Error("Kullanım: keyboard reset");
        std::map<int,std::string> white;
        for(const auto& key:keys)white[key.index]="ffffff";
        applyColors(io,white);return;
    }
    std::vector<Led> selected;
    std::string value;
    if(a[0]=="color"&&a.size()==2){selected=keys;value=a[1];}
    else if(a[0]=="key"&&a.size()==3) {
        int index=-1;
        if(a[1].find_first_not_of("0123456789")==std::string::npos)index=integer(a[1],0,1024,"Tuş indeksi");
        else {for(const auto&key:physicalKeys(io))if(key.id==a[1])index=key.index;}
        if(index<0)throw Error("Bilinmeyen fiziksel tuş adı.");
        value=a[2];
        for(const auto&k:keys)if(k.index==index)selected.push_back(k);
        if(selected.empty())throw Error("Bu tuş indeksi aygıtta yok.");
    }else throw Error("Kullanım: keyboard status|brightness YÜZDE|color RRGGBB|key İNDEKS RRGGBB|effect TÜR");
    std::map<int,std::string> colors;for(const auto&key:selected)colors[key.index]=value;
    applyColors(io,colors);
}
void applyColors(const Io&io,const std::map<int,std::string>&colors,int brightness){
    const auto&keys=leds(io);if(keys.empty()||colors.empty())throw Error("RGB LED aygıtı bulunamadı.",4);
    std::map<int,std::array<int,3>> parsed;
    for(const auto&[index,text]:colors){
        auto it=std::find_if(keys.begin(),keys.end(),[&](const Led&k){return k.index==index;});
        if(it==keys.end()||!colorLed(*it))throw Error("Profil LED'leri doğrulanamadı; işlem iptal edildi.",4);
        parsed[index]=parseColor(text);
    }
    if(brightness < -1 || brightness>100)throw Error("Geçersiz profil parlaklığı.");
    RgbTransaction transaction(io,true);
    std::vector<std::pair<std::string,std::string>> previous;
    for(const auto&k:keys)if(parsed.contains(k.index)){
        auto value=trim(io.read(k.path+"/multi_intensity"));validateSavedColor(k,value);
        previous.emplace_back(k.path+"/multi_intensity",value);
    }
    std::vector<std::pair<std::string,std::string>> oldBrightness;
    const auto brightnessCount=sharedBrightness(io,keys)?std::size_t(1):keys.size();
    if(brightness>=0)for(std::size_t i=0;i<brightnessCount;++i){
        const auto&k=keys[i];auto value=trim(io.read(k.path+"/brightness"));integer(value,0,k.maximum,"Önceki parlaklık");oldBrightness.emplace_back(k.path+"/brightness",value);
    }
    bool attempted=false;
    std::vector<std::pair<std::string,std::string>> expected;
    try{
        attempted=true;
        transaction.begin();
        for(const auto&k:keys)if(parsed.contains(k.index)){
            const auto rgb=parsed.at(k.index);std::string output;
            for(std::size_t i=0;i<k.channels.size();++i){
                int channel=k.channels[i]=="red"?0:k.channels[i]=="green"?1:2;
                if(i)output+=' ';
                output+=std::to_string((rgb[channel]*k.channelMax[i]+127)/255);
            }
            expected.emplace_back(k.path+"/multi_intensity",output);
            attempted=true;
            io.write(k.path+"/multi_intensity",output+"\n");
        }
        if(brightness>=0){
            for(std::size_t i=0;i<brightnessCount;++i){
                expected.emplace_back(keys[i].path+"/brightness",std::to_string((brightness*keys[i].maximum+50)/100));
                io.write(expected.back().first,expected.back().second+"\n");
            }
        }
        transaction.finish();
        if(!io.dry())for(const auto&[path,value]:expected)if(trim(io.read(path))!=value)throw Error("RGB geri okuması uyuşmadı.",4);
    }catch(...){
        bool restored=true;
        if(!io.dry()&&attempted){
            try{transaction.begin();}catch(...){restored=false;}
            for(const auto&[p,v]:previous){try{io.write(p,v+"\n");if(trim(io.read(p))!=v)restored=false;}catch(...){restored=false;}}
            for(const auto&[p,v]:oldBrightness){try{io.write(p,v+"\n");if(trim(io.read(p))!=v)restored=false;}catch(...){restored=false;}}
        }
        try{transaction.finish();}catch(...){restored=false;}
        try{transaction.restoreEffect();}catch(...){restored=false;}
        if(!restored)throw Error("RGB uygulanamadı; önceki renk/parlaklık tam geri yüklenemedi.",4);
        throw;
    }
    std::cout<<(io.dry()?"RGB önizlemesi tamamlandı.":"Klavye renkleri uygulandı.")<<'\n';
}
namespace {volatile sig_atomic_t identifyStopped=0;void identifyStop(int){identifyStopped=1;}}
void locate(const Io&io,int index){
    io.requireCompatible();
    const auto&keys=leds(io);if(keys.empty())throw Error("Klavye LED sürücüsü yüklü değil.",4);
    bool found=false;for(const auto&k:keys){if(!colorLed(k))throw Error("RGB arayüzü gerekli.",4);if(k.index==index)found=true;}
    if(!found)throw Error("LED indeksi aygıtta bulunamadı.");
    if(io.dry()){std::cout<<"Önizleme: LED "<<index<<" iki saniye beyaz; sonra önceki renk/parlaklık.\n";return;}
    identifyStopped=0;
    auto si=std::signal(SIGINT,identifyStop),st=std::signal(SIGTERM,identifyStop),sh=std::signal(SIGHUP,identifyStop);
    struct Signals{decltype(si)i,t,h;~Signals(){std::signal(SIGINT,i);std::signal(SIGTERM,t);std::signal(SIGHUP,h);}}signals{si,st,sh};
    RgbTransaction transaction(io,true);
    std::vector<std::pair<std::string,std::string>> saved;
    for(const auto&k:keys){
        auto c=trim(io.read(k.path+"/multi_intensity")),b=trim(io.read(k.path+"/brightness"));
        validateSavedColor(k,c);integer(b,0,k.maximum,"Önceki parlaklık");
        saved.emplace_back(k.path+"/multi_intensity",c);
        if(!sharedBrightness(io,keys)||&k==&keys.front())saved.emplace_back(k.path+"/brightness",b);
    }
    std::exception_ptr failure;
    try {
        transaction.begin();
        for(const auto&k:keys){
            std::string color="0 0 0";
            if(k.index==index){color.clear();for(auto maximum:k.channelMax){if(!color.empty())color+=' ';color+=std::to_string(maximum);}}
            io.write(k.path+"/multi_intensity",color+"\n");
        }
        const auto count=sharedBrightness(io,keys)?std::size_t(1):keys.size();
        for(std::size_t i=0;i<count;++i)io.write(keys[i].path+"/brightness",std::to_string(keys[i].maximum/2)+"\n");
        transaction.finish();
        for(unsigned i=0;i<20&&!identifyStopped;++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } catch(...) {failure=std::current_exception();}
    bool restored=true;
    try{transaction.begin();}catch(...){restored=false;}
    for(const auto&[path,value]:saved)try{
        io.write(path,value+"\n");
        if(trim(io.read(path))!=value)restored=false;
    }catch(...){restored=false;}
    try{transaction.finish();}catch(...){restored=false;}
    try{transaction.restoreEffect();}catch(...){restored=false;}
    if(!restored)throw Error("LED tanıma bitti; önceki klavye durumu tam geri yüklenemedi.",4);
    if(failure)std::rethrow_exception(failure);

}
}
