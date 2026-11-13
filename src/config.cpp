// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include "profile_sync.hpp"
#include <cstdint>
#include <algorithm>
#include <charconv>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctime>
#include <cerrno>

namespace slayer {
static const std::string file="/etc/slayerctl/slayerctl.conf";
static void validate(const std::string&k,const std::string&v) {
    if(k=="rgb_profile"||k=="rgb_profile_ac"||k=="rgb_profile_battery")validateProfileName(v);
    else if(k=="battery_profile"){if(v!="high-capacity"&&v!="balanced"&&v!="stationary")throw Error("Geçersiz pil koruma profili.");}
    else if(k=="cpu_boost"||k=="cpu_smt"||k=="fn_lock"||k=="usb_charging"||k=="camera"){if(v!="on"&&v!="off")throw Error(k+" on/off olmalı.");}
    else if(k=="performance_sync"){if(v!="on"&&v!="off")throw Error("Eşitleme on/off olmalı.");}
    else if(k=="lighting_owner"){if(v!="kde"&&v!="accent"&&v!="custom")throw Error("Geçersiz ışık yöneticisi.");}
    else if(k=="brightness_owner"){if(v!="kde"&&v!="custom")throw Error("Geçersiz parlaklık yöneticisi.");}
    else if(k=="fan_mode"){if(v!="auto"&&v!="boost")throw Error("fan_mode auto/boost olmalı.");}
    else if(k=="cpu_min_mhz"||k=="cpu_max_mhz")integer(v,100,10000,k);
    else if(k=="desktop_profile"||k=="desktop_profile_ac"||k=="desktop_profile_battery"){if(v!="power-saver"&&v!="balanced"&&v!="performance")throw Error("Geçersiz masaüstü güç profili.");}
    else if(k=="keyboard_color_profile"){if(v!="off")validateProfileName(v);}
    else if(k=="keyboard_white_point"){
        auto text=v;std::replace(text.begin(),text.end(),',',' ');const auto rgb=words(text);
        if(rgb.size()!=3)throw Error("Beyaz noktası R,G,B biçiminde olmalı.");
        for(const auto& channel:rgb)integer(channel,1,255,"Beyaz noktası");
    }
    else if(k=="keyboard_brightness")integer(v,0,100,k);
    else if(k=="battery_end")integer(v,50,100,k);
    else if(k=="battery_start")integer(v,0,95,k);
    else if(k=="keyboard_color"){
        unsigned n=0;auto [p,e]=std::from_chars(v.data(),v.data()+v.size(),n,16);
        if(v.size()!=6||e!=std::errc{}||p!=v.data()+v.size())throw Error("keyboard_color altı basamaklı RGB olmalı.");
    }else if(k=="profile"){
        if(v!="low-power"&&v!="quiet"&&v!="balanced"&&v!="balanced-performance"&&v!="performance"&&v!="cool")throw Error("Geçersiz standart güç profili.");
    }else throw Error("Bilinmeyen yapılandırma anahtarı: "+k);
}
static const std::string marker="SLAYER-TPM2\n";
class CredentialFile {
    int fd=-1;
    std::string name;
public:
    explicit CredentialFile(const std::string& text){
        fd=memfd_create("slayerctl-credential",MFD_CLOEXEC);
        if(fd<0)throw Error("Bellek kimlik dosyası oluşturulamadı.");
        if(fchmod(fd,0600)){close(fd);throw Error("Kimlik dosyası izinleri uygulanamadı.");}
        std::size_t offset=0;
        while(offset<text.size()){
            auto count=::write(fd,text.data()+offset,text.size()-offset);
            if(count<0&&errno==EINTR)continue;
            if(count<=0){close(fd);throw Error("Kimlik dosyası yazılamadı.");}
            offset+=static_cast<std::size_t>(count);
        }
        name="/proc/"+std::to_string(getpid())+"/fd/"+std::to_string(fd);
    }
    ~CredentialFile(){if(fd>=0)close(fd);}
    const std::string& path()const{return name;}
};
static std::string cryptConfig(const std::string& text,bool encrypt){
    if(geteuid()!=0)throw Error("Şifreli ayarlar yetkili slayerctl servisi üzerinden okunmalı.",3);
    CredentialFile input(text);
    if(encrypt)return capture({"/usr/bin/systemd-creds","encrypt","--with-key=tpm2","--tpm2-pcrs=7","--name=slayerctl",input.path(),"-"},15000);
    return capture({"/usr/bin/systemd-creds","decrypt","--name=slayerctl",input.path(),"-"},15000);
}
std::string readProtected(const Io& io,const std::string& path,std::size_t limit){
    if(!io.fixture()&&io.exists(path)){
        struct stat st{};
        if(lstat(io.path(path).c_str(),&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||(st.st_mode&0022))throw Error("Ayar dosyası sahipliği veya izinleri güvenli değil.");
    }
    auto text=io.read(path,65536);
    if(text.size()>=65536)throw Error("Şifreli ayar dosyası çok büyük.");
    if(text.starts_with(marker)&&!io.fixture())text=cryptConfig(text.substr(marker.size()),false);
    if(text.size()>=limit)throw Error("Ayar dosyası sınırı aşıldı.");
    return text;
}
void saveProtected(const Io& io,const std::string& path,const std::string& text){
    if(io.fixture()||io.dry()){saveText(io,path,text);return;}
    if(readProtected(io,path,32768)==text)return;
    saveText(io,path,marker+cryptConfig(text,true));
}
static std::map<std::string,std::string> load(const Io&io){
    if(!io.fixture()&&geteuid()==0&&io.exists(file)){
        struct stat st{};
        if(lstat(io.path(file).c_str(),&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||(st.st_mode&0022))throw Error("Yapılandırma root sahibi, normal ve başkalarınca yazılamayan dosya olmalı.");
    }
    auto text=readProtected(io,file,8192);
    std::map<std::string,std::string> cfg;
    if(text.size()>=8192)throw Error("Yapılandırma dosyası beklenenden büyük.");
    std::size_t pos=0;
    while(pos<text.size()){
        auto end=text.find('\n',pos);if(end==std::string::npos)end=text.size();auto line=trim(text.substr(pos,end-pos));pos=end+1;
        if(line.empty()||line.starts_with('#'))continue;
        auto eq=line.find('=');if(eq==std::string::npos)throw Error("Yapılandırmada anahtar=değer bekleniyor.");
        auto k=trim(line.substr(0,eq)),v=trim(line.substr(eq+1));validate(k,v);
        if(!cfg.emplace(k,v).second)throw Error("Yinelenen yapılandırma anahtarı: "+k);
    }
    if(cfg.contains("cpu_min_mhz")!=cfg.contains("cpu_max_mhz"))throw Error("CPU alt ve üst frekansı birlikte kaydedilmeli.");
    if(cfg.contains("cpu_min_mhz")&&integer(cfg.at("cpu_min_mhz"),100,10000,"CPU alt")>integer(cfg.at("cpu_max_mhz"),100,10000,"CPU üst"))throw Error("CPU frekans sırası geçersiz.");
    if(cfg.contains("battery_start")&&!cfg.contains("battery_end"))throw Error("battery_start için battery_end de gerekli.");
    if(cfg.contains("battery_start")&&integer(cfg.at("battery_start"),0,95,"Alt eşik")>=integer(cfg.at("battery_end"),50,100,"Üst eşik"))throw Error("Alt pil eşiği üst eşikten küçük olmalı.");
    return cfg;
}
std::map<std::string,std::string> configurationValues(const Io& io){return load(io);}
void configuration(const Io&io,const std::vector<std::string>&a){
    if(a.empty()||a[0]=="show"){
        if(a.size()>1)throw Error("Kullanım: config show");
        auto cfg=load(io);if(cfg.empty())std::cout<<"Kalıcı donanım ayarı yok. Açılışta ek yazma yapılmayacak.\n";
        for(const auto&[k,v]:cfg)std::cout<<k<<'='<<v<<'\n';
        return;
    }
    FileLock guard(io,"config");
    if(a[0]=="apply"||a[0]=="apply-event"){
        if(a.size()!=1)throw Error("apply ek argüman kabul etmez.");
        auto cfg=load(io);int failed=0;
        const bool event=a[0]=="apply-event";
        if(!cfg.empty())io.requireCompatible();
        if(!cfg.empty()&&!io.fixture()&&!io.dry()&&geteuid()!=0)throw Error("Ayarları uygulamak için sudo gerekir.",3);
        const bool conditional=cfg.contains("rgb_profile_ac")||cfg.contains("rgb_profile_battery")||cfg.contains("desktop_profile_ac")||cfg.contains("desktop_profile_battery");
        if(a[0]=="apply-event"&&!conditional)return;
        bool ac=false;std::string cache;
        if(conditional){
            ac=acOnline(io);std::uint64_t signature=14695981039346656037ull;
            for(const auto&[k,v]:cfg)for(unsigned char c:k+"="+v+"\n"){signature^=c;signature*=1099511628211ull;}
            cache=std::string(ac?"ac:":"battery:")+std::to_string(signature)+"\n";
            if(a[0]=="apply-event"&&io.read("/run/slayerctl/power-state")==cache)return;
        }
        auto run=[&](auto action){try{action();}catch(const Error&e){++failed;std::cerr<<"Atlandı: "<<e.what()<<'\n';}};
        const std::string suffix=ac?"_ac":"_battery";
        if(conditional&&cfg.contains("desktop_profile"+suffix))run([&]{
            if(io.dry())std::cout<<"Önizleme: masaüstü profili "<<cfg.at("desktop_profile"+suffix)<<'\n';
            else if(io.fixture())throw Error("Test kipinde D-Bus'a erişilmez.",4);
            else std::cout<<capture({"/usr/libexec/slayerctl-desktop","--power-set",cfg.at("desktop_profile"+suffix)},7000)<<'\n';
        });
        const bool sync=cfg.contains("performance_sync")&&cfg.at("performance_sync")=="on";
        if(!event){
        if(!sync&&cfg.contains("desktop_profile"))run([&]{
            if(io.fixture())throw Error("Test kipinde sistem D-Bus'una erişilmez.",4);
            if(!io.dry())std::cout<<capture({"/usr/libexec/slayerctl-desktop","--power-set",cfg.at("desktop_profile")},7000);
        });
        if(cfg.contains("cpu_smt"))run([&]{cpuControl(io,{"smt",cfg.at("cpu_smt")});});
        for(const auto&[key,setting]:std::map<std::string,std::string>{{"fn_lock","fn-lock"},{"usb_charging","usb-charging"},{"camera","camera"}})
            if(cfg.contains(key))run([&]{platformSetting(io,{setting,cfg.at(key)});});
        if(cfg.contains("fan_mode"))run([&]{fan(io,{cfg.at("fan_mode")});});
        if(cfg.contains("cpu_boost"))run([&]{cpuControl(io,{"boost",cfg.at("cpu_boost")});});
        if(cfg.contains("cpu_min_mhz"))run([&]{cpuControl(io,{"limits",cfg.at("cpu_min_mhz"),cfg.at("cpu_max_mhz")});});
        if(cfg.contains("battery_profile"))run([&]{battery(io,{"profile",cfg.at("battery_profile")});});
        if(!sync&&cfg.contains("profile"))run([&]{profile(io,{"set",cfg.at("profile")});});
        if(cfg.contains("battery_end"))run([&]{std::vector<std::string>x{"limit",cfg.at("battery_end")};if(cfg.contains("battery_start"))x.push_back(cfg.at("battery_start"));battery(io,x);});
        }
        if(!event&&cfg.contains("keyboard_white_point"))run([&]{
            auto text=cfg.at("keyboard_white_point");std::replace(text.begin(),text.end(),',',' ');
            auto point=words(text);point.insert(point.begin(),"white-point");keyboard(io,point);
        });
        if(!event&&cfg.contains("keyboard_color_profile"))run([&]{
            const auto name=cfg.at("keyboard_color_profile");
            if(name=="off")keyboard(io,{"color-profile","off"});else keyboard(io,{"color-profile","apply",name});
        });
        const bool customLight=cfg.contains("lighting_owner")?cfg.at("lighting_owner")=="custom":true;
        const bool customBrightness=cfg.contains("brightness_owner")?cfg.at("brightness_owner")=="custom":true;
        std::string rgb=!event&&customLight&&cfg.contains("rgb_profile")?cfg.at("rgb_profile"):"";
        if(conditional&&customLight&&cfg.contains("rgb_profile"+suffix))rgb=cfg.at("rgb_profile"+suffix);
        if(!rgb.empty())run([&]{rgbProfile(io,{"apply",rgb});});
        if(!event&&customLight&&rgb.empty()&&cfg.contains("keyboard_color"))run([&]{keyboard(io,{"color",cfg.at("keyboard_color")});});
        if(!event&&customBrightness&&cfg.contains("keyboard_brightness"))run([&]{keyboard(io,{"brightness",cfg.at("keyboard_brightness")});});
        if(sync)run([&]{synchronizeProfiles(io,true);});
        if(failed)throw Error("Bazı kalıcı ayarlar mevcut kernel arayüzüyle uygulanamadı.",4);
        if(conditional&&!io.dry())saveText(io,"/run/slayerctl/power-state",cache);
        return;
    }
    if(a[0]=="recover"&&a.size()==1){
        if(io.fixture()||io.dry())throw Error("TPM kurtarma gerçek sistemde yetkili servis gerektirir.");
        if(geteuid()!=0)throw Error("Kurtarma yönetici doğrulaması gerektirir.",3);
        auto old=io.read(file,65536);
        if(!old.starts_with(marker))throw Error("Kurtarılacak şifreli yapılandırma yok.");
        // Verify replacement encryption before preserving the inaccessible ciphertext.
        auto fresh=marker+cryptConfig("# slayerctl: TPM kurtarma; önceki ayarlar otomatik uygulanmaz\n",true);
        saveText(io,file+".recovery-"+std::to_string(time(nullptr)),old);
        saveText(io,file,fresh);chmod(io.path(file).c_str(),0600);return;
    }
    auto cfg=load(io);
    const auto previous=cfg;
    const bool protect=a[0]=="protect"&&a.size()==1;
    if(protect&&io.read(file,65536).starts_with(marker))return;
    if(protect){}
    else if(a[0]=="clear"&&a.size()==1)cfg.clear();
    else if(a[0]=="unset"&&a.size()>=2&&a.size()<=7){for(std::size_t i=1;i<a.size();++i)cfg.erase(a[i]);}
    else if(a[0]=="set"&&a.size()>=3&&a.size()<=11&&a.size()%2==1){
        for(std::size_t i=1;i<a.size();i+=2){validate(a[i],a[i+1]);cfg[a[i]]=a[i+1];}
    }
    else throw Error("Kullanım: config set ANAHTAR DEĞER [ANAHTAR DEĞER...] | config show | config clear");
    if(cfg.contains("cpu_min_mhz")!=cfg.contains("cpu_max_mhz"))throw Error("CPU alt ve üst frekansı birlikte kaydedilmeli.");
    if(cfg.contains("cpu_min_mhz")&&integer(cfg.at("cpu_min_mhz"),100,10000,"CPU alt")>integer(cfg.at("cpu_max_mhz"),100,10000,"CPU üst"))throw Error("CPU frekans sırası geçersiz.");
    if(cfg.contains("battery_start")&&!cfg.contains("battery_end"))throw Error("Önce battery_end ayarlayın.");
    if(cfg.contains("battery_start")&&cfg.contains("battery_end")&&integer(cfg.at("battery_start"),0,95,"Alt eşik")>=integer(cfg.at("battery_end"),50,100,"Üst eşik"))throw Error("Alt pil eşiği üst eşikten küçük olmalı.");
    if(!protect&&cfg==previous)return;
    if(io.dry()){for(const auto&[k,v]:cfg)std::cout<<"Önizleme: "<<k<<'='<<v<<'\n';return;}
    if(!io.fixture()&&geteuid()!=0)throw Error("Kalıcı ayar için sudo gerekir.",3);
    std::string text="# slayerctl "+std::string(version)+" — yalnız kullanıcı tarafından seçilmiş ayarlar\n";
    for(const auto&[k,v]:cfg)text+=k+"="+v+"\n";
    if(!io.fixture()){
        text=marker+cryptConfig(text,true);
        if(io.read(file,65536)==text)return;
    }
    saveText(io,file,text);
    if(!io.fixture()&&chmod(io.path(file).c_str(),0600))throw Error("Ayar dosyası izinleri uygulanamadı.");
    std::error_code cacheError;fs::remove(io.path("/run/slayerctl/power-state"),cacheError);
    std::cout<<"Ayar kaydedildi. Şimdi uygulamak için: sudo slayerctl apply-config\n";
}
}
