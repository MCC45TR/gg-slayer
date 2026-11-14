// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include "profile_sync.hpp"
#include "../kernel/model_contract.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

namespace slayer {
std::string hardwareProfilePath(const Io& io){
    const std::string native="/sys/devices/platform/slayerctl/thermal_mode";
    return io.exists(native)?native:"/sys/firmware/acpi/platform_profile";
}

static std::string get(const Io &io, const std::string &p) { return trim(io.read(p)); }
static bool contains(const std::vector<std::string> &v, const std::string &s) { return std::find(v.begin(),v.end(),s)!=v.end(); }
static const std::string platform = "/sys/devices/platform/slayerctl";
std::vector<std::string> batteries(const Io &io) {
    std::vector<std::string> out;
    for (const auto &n:io.list("/sys/class/power_supply")) {
        auto p="/sys/class/power_supply/"+n;
        if (n.starts_with("BAT") && get(io,p+"/type")=="Battery") out.push_back(p);
    }
    return out;
}
const std::vector<Led>& leds(const Io &io) {
    if(io.ledCache)return *io.ledCache;
    std::vector<Led> result;
    for (const auto &name:io.list("/sys/class/leds")) {
        if (name.find("kbd_backlight")==std::string::npos) continue;
        auto p="/sys/class/leds/"+name;
        const auto maximum=get(io,p+"/max_brightness");
        if (maximum.empty()) continue;
        Led led{p,integer(maximum,1,65535,"LED üst sınırı"),0,{},{}};
        led.channels=words(get(io,p+"/multi_index"));
        for (const auto &v:words(get(io,p+"/multi_max_intensity"))) led.channelMax.push_back(integer(v,1,65535,"Kanal üst sınırı"));
        if (led.channelMax.size()!=led.channels.size()) led.channelMax.assign(led.channels.size(),led.maximum);
        // ITE 8291 supports an independent 0..255 color value. The fallback
        // above is for older generic LED multicolor ABI versions.
        if (name=="rgb:kbd_backlight") led.index=0;
        else if (name.starts_with("rgb:kbd_backlight_")) led.index=integer(name.substr(18),0,1024,"Tuş indeksi");
        else led.index=static_cast<int>(result.size());
        result.push_back(std::move(led));
    }
    std::sort(result.begin(),result.end(),[](const Led&a,const Led&b){return a.index<b.index;});
    io.ledCache=std::move(result);
    return *io.ledCache;
}
std::map<std::string,std::string> status(const Io &io) {
    std::map<std::string,std::string> out;
    out["version"]=version;
    out["supported_bios"]=SLAYER_BIOS;
    out["hardware_writes"]=io.compatibilityIssue().empty()?"allowed":"blocked";
    out["hardware_write_reason"]=io.compatibilityIssue();
    for (const auto &entry:std::array<std::pair<const char*,const char*>,6>{{
        {"vendor","sys_vendor"},{"model","product_name"},{"sku","product_sku"},
        {"bios","bios_version"},{"bios_date","bios_date"},{"board","board_name"}}})
        out[entry.first]=get(io,"/sys/class/dmi/id/"+std::string(entry.second));
    auto cpu=io.read("/proc/cpuinfo"); auto start=cpu.find("model name");
    if (start!=std::string::npos) { auto colon=cpu.find(':',start);out["cpu"]=trim(cpu.substr(colon+1,cpu.find('\n',colon)-colon-1)); }
    out["kernel"]=get(io,"/proc/sys/kernel/osrelease");
    out["nvidia"]=io.exists("/sys/module/nvidia")?"loaded":(io.exists("/sys/module/nouveau")?"nouveau":"not-loaded");
    out["keyboard_driver"]=io.exists("/sys/module/ite_8291")?"ite_8291":"not-loaded";
    const auto&keys=leds(io);out["keyboard_leds"]=std::to_string(keys.size());
    if (!keys.empty()) {
        auto current=get(io,keys.front().path+"/brightness");
        if (!current.empty()) out["keyboard_brightness"]=std::to_string(integer(current,0,65535,"LED parlaklığı")*100/keys.front().maximum);
        out["keyboard_color"]=get(io,keys.front().path+"/multi_intensity");
    }
    const auto rgbDevice=hardwareDevice(io);
    if(!rgbDevice.empty())out["keyboard_white_point"]=get(io,"/sys/class/hidraw/"+rgbDevice+"/device/controls/white_point");
    if(!rgbDevice.empty()){
        const auto model=get(io,"/sys/class/hidraw/"+rgbDevice+"/device/controls/color_profile");
        out["keyboard_color_profile_active"]=model.empty()?"":model=="off"?"0":"1";
    }
    out["profile"]=get(io,hardwareProfilePath(io));
    out["profile_choices"]=get(io,hardwareProfilePath(io)+"_choices");
    auto bs=batteries(io);
    if (!bs.empty()) {
        out["battery"]=get(io,bs.front()+"/capacity");out["battery_status"]=get(io,bs.front()+"/status");
        out["battery_limit"]=get(io,bs.front()+"/charge_control_end_threshold");
        out["battery_start"]=get(io,bs.front()+"/charge_control_start_threshold");
        auto full=get(io,bs.front()+"/charge_full"),design=get(io,bs.front()+"/charge_full_design");
        if(full.empty()||design.empty()){full=get(io,bs.front()+"/energy_full");design=get(io,bs.front()+"/energy_full_design");}
        if(!full.empty()&&!design.empty()){
            auto f=integer(full,0,2000000000,"Tam kapasite"),d=integer(design,1,2000000000,"Tasarım kapasitesi");
            out["battery_health"]=std::to_string(std::min(100,static_cast<int>(static_cast<long long>(f)*100/d)));
        }
        out["battery_cycles"]=get(io,bs.front()+"/cycle_count");
        out["battery_profile"]=get(io,platform+"/charging_profile");
        if(out["battery_profile"].empty())out["battery_profile"]=get(io,bs.front()+"/charge_types");
    }
    // Only device/vendor identifiers are read: no GPU wake-up, NVML polling,
    // USB control transfers, serial numbers, or raw EC requests in status.
    std::string panel;
    for (const auto &n:io.list("/sys/class/drm")) {
        if (n.find("-eDP-")==std::string::npos || get(io,"/sys/class/drm/"+n+"/status")!="connected") continue;
        auto p="/sys/class/drm/"+n+"/device/vendor";
        auto vendor=get(io,p);
        if (vendor.empty()) vendor=get(io,"/sys/class/drm/"+n.substr(0,n.find('-'))+"/device/vendor");
        panel=n+(vendor=="0x10de"?" (NVIDIA)":vendor=="0x1002"?" (AMD)":"");
    }
    out["panel"]=panel;
    out["tpm"]=io.exists("/sys/class/tpm/tpm0")?"available":"unavailable";
    const auto secure=io.read("/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c",8);
    out["secure_boot"]=secure.size()>=5?(secure[4]?"enabled":"disabled"):"unknown";
    for(auto name:{"fn_lock","fan_mode","usb_sleep_charging_supported","mux_mode","profile_events","thermal_control"})out[name]=get(io,platform+"/"+name);
    out["usb_charging"]=get(io,platform+"/usb_sleep_charging");
    out["cpu_smt"]=get(io,"/sys/devices/system/cpu/smt/control");
    out["cpu_online"]=get(io,"/sys/devices/system/cpu/online");
    try{auto camera=cameraPath(io);out["camera"]=camera.empty()?"unavailable":get(io,camera);}
    catch(const Error&e){out["camera"]="ambiguous";out["camera_reason"]=e.what();}
    out["ec_backend"]= get(io,platform+"/backend_version");
    if(out["ec_backend"].empty())out["ec_backend"]="not-loaded";
    auto policies=cpuPolicies(io);
    out["cpu_policies"]=std::to_string(policies.size());
    if(!policies.empty()){
        const auto&p=policies[0];
        for(auto name:{"cpuinfo_min_freq","cpuinfo_max_freq","scaling_min_freq","scaling_max_freq","boost","energy_performance_preference"})out[std::string("cpu_")+name]=get(io,p+"/"+name);
    }
    try{out["ac_online"]=acOnline(io)?"1":"0";}catch(const Error&){out["ac_online"]="unknown";}
    int usbDevices=0,usbHubs=0;
    for(const auto& name:io.list("/sys/bus/usb/devices")){
        if(name.starts_with("usb")||name.find(':')!=std::string::npos)continue;
        const auto base="/sys/bus/usb/devices/"+name;
        if(get(io,base+"/idVendor").empty())continue;
        if(get(io,base+"/bDeviceClass")=="09")++usbHubs;else ++usbDevices;
    }
    out["usb_connected_devices"]=std::to_string(usbDevices);
    out["usb_connected_hubs"]=std::to_string(usbHubs);
    if(out["vendor"]==SLAYER_VENDOR&&out["model"]==SLAYER_PRODUCT&&out["sku"]==SLAYER_SKU){
        out["usb_external_a"]="3";out["usb_external_c"]="2";
        out["usb_a_speed_mbps"]="5000";out["usb_c_speed_mbps"]="10000";
        out["usb_c_pd_left_w"]="100";out["usb_c_pd_rear_w"]="140";
    }
    return out;
}
std::vector<Capability> capabilities(const Io &io) {
    const auto&keys=leds(io); const bool key=!keys.empty();const auto bs=batteries(io);const auto cpus=cpuPolicies(io);
    const bool threshold=!bs.empty()&&io.exists(bs.front()+"/charge_control_end_threshold");
    auto native=[&](const std::string&p){return io.exists(platform+"/"+p);};
    std::string cameraReason="Sabit 2b7e:c906 rev0004 dahili kameranın Linux USB yetkilendirmesi. Firmware E-shutter değildir; görüşme kesilebilir.";
    bool cameraReady=false;
    try{cameraReady=!cameraPath(io).empty();}catch(const Error&e){cameraReason=e.what();}
    auto reason="Windows desteği bulundu; R9T BIOS/EC yazma yolu henüz doğrulanmadı.";
    std::vector<Capability> out={
        {"info","Cihaz / BIOS / sürücüler","ready","Salt okunur sysfs ve procfs."},
        {"sensors","Sensörler ve pil durumu","ready","Mevcut hwmon/power_supply dosyaları; etkin GPU sorgusu yok."},
        {"keyboard","Klavye parlaklığı",key?"ready":"blocked",key?"Standart Linux LED arayüzü.":"ITE sürücüsü yüklenmemiş; Secure Boot/MOK kaydını denetleyin."},
        {"rgb","RGB renk / tuş / efekt",key&&io.exists(keys.front().path+"/multi_intensity")?"ready":"blocked","Standart multicolor ABI. Efekt yalnız komut çalışırken CPU kullanır."},
        {"hardware-rgb","Klavyede çalışan RGB efektleri",hardwareDevice(io).empty()?"blocked":"ready","Yalnız R9T / ITE 048d:600b rev0003 arayüz 1; tek LED raporu, flash yazması ve daemon yok."},
        {"accent","KDE vurgu rengi","session","KDE Kameleon; kullanıcı oturumunda sudo olmadan çalışır."},
        {"battery-limit","Pil dolum alt/üst eşiği",threshold?"ready":"blocked",threshold?"Kernel standart charge_control_* arayüzünü sağlıyor.":"Bu kernel/firmware yüzde şarj sınırı sunmuyor; KDE de yerel yüzde ayarını bu nedenle gösteremez. Koruma profili yüzde eşiği değildir."},
        {"battery-profile","Pil koruma profili",(native("charging_profile")||(!bs.empty()&&io.exists(bs.front()+"/charge_types")))?"ready":"blocked","R9T model/BIOS/proje ve destek bitiyle sınırlı üç profil; yüzde dolum eşiği değildir."},
        {"desktop-power","KDE / GNOME ortak güç profili","session","Mevcut UPower PowerProfiles D-Bus servisi; OEM Turbo/MUX ile aynı ayar değildir."},
        {"performance-sync","KDE / EC performans eşitlemesi",get(io,platform+"/profile_events").starts_with("acpi-query")&&get(io,hardwareProfilePath(io)+"_choices").find("low-power")!=std::string::npos?"ready":"blocked","Güncel R9T Office modu ve WMI olay izleyicisi gerekir; sürekli EC sorgulaması yapılmaz."},
        {"profile","Güç / termal profil",io.exists(hardwareProfilePath(io))?"ready":"blocked","Kernel platform_profile ABI; yalnız bildirilen seçenekler."},
        {"mux","Fiziksel MUX / sonraki açılış",native("mux_mode")?"ready":"blocked","180 bayt yerel OEM alanı + tam BIOS/anakart/proje kontrolü. Sonraki açılış; otomatik yeniden başlatma yok. Fiziksel geçiş yeniden başlatmayla doğrulanmalı."},
        {"igpu","Yalnız iGPU / Hibrit / NVIDIA",native("mux_mode")?"ready":"blocked","Firmware MUX seçimi; yalnız sürücü kara listeleme değildir. iGPU-only harici NVIDIA bağlantılarını kapatabilir."},
        {"fan","Fan Auto / Boost",native("fan_mode")?"ready":"blocked","EC fabrika fan denetimi / maksimum soğutma. PWM veya özel fan tablosu yazılmaz."},
        {"fan-curve","Özel CPU/GPU fan eğrisi","blocked","Tablo ölçeği ve güvenlik sınırları doğrulanmadan EC tablo yazması yok."},
        {"cpu-smt","CPU SMT / iş parçacıkları",contains({"on","off"},get(io,"/sys/devices/system/cpu/smt/control"))?"ready":"blocked","Standart Linux SMT on/off; fiziksel CCD kapatılmaz, forceoff aşılmaz."},
        {"cpu-ccd","Fiziksel CCD seçimi","blocked","BIOS seçeneği doğrulandı; OEM değişkeninde güvenli runtime commit yolu bulunmadı. BIOS'tan ayarlanır."},
        {"cpu-frequency","CPU frekans alt/üst sınırı",cpus.empty()?"blocked":"ready","CPUFreq donanım sınırlarıyla doğrulanır; SMU/voltaj yazması yok."},
        {"cpu-boost","CPU boost",(!cpus.empty()&&(io.exists("/sys/devices/system/cpu/cpufreq/boost")||io.exists(cpus[0]+"/boost")))?"ready":"blocked","Global ve policy bazında kernel boost; masaüstü yöneticisi değiştirebilir."},
        {"acdc-profiles","Adaptör/pil için otomatik profiller","ready","udev/systemd olay tabanlı; yalnız seçilmiş RGB/masaüstü profilleri, sürekli daemon yok."},
        {"fan-curve-editor","Fan eğrisi hazırlama","ready","Çevrimdışı dosya oluşturma/doğrulama. Donanım eğrisi uygulaması hâlâ kapalı."},
        {"cpu-power","CPU SPL / SPPT / FPPT / Tcc","blocked",reason},
        {"gpu-power","GPU TGP / Dynamic Boost","blocked","NVIDIA sürücüsü ve OEM min/max doğrulaması gerekli."},
        {"overclock","CPU/GPU OC ve voltaj","blocked","0.0.1 voltaj/OC yazmaz; örnek Windows değerleri limit sayılmaz."},
        {"usb","USB aygıtları / çalışma durumu","ready","Pasif sysfs envanteri; aygıt sıfırlama veya sürücü ayırma yok."},
        {"usb-power","USB autosuspend / uyandırma","ready","Yalnız standart power/control ve power/wakeup; hub, depolama ve giriş aygıtları korunur."},
        {"usb-charging","Kapalı/uykuda USB şarj",native("usb_sleep_charging")?"ready":"blocked",native("usb_sleep_charging")?"EC destek bitiyle sınırlı Linux yöntemi.":"Bu model/BIOS kapalı USB şarj destek bitini bildirmiyor; yazma kilitli."},
        {"ac-recover","Adaptör takılınca açılma","blocked","Windows uygulama ve BIOS destek bilgisi çelişiyor."},
        {"fn","Fn kilidi",native("fn_lock")?"ready":"blocked","Doğrulanmış 0x074e bit4; diğer firmware bitleri korunur."},
        {"camera","Dahili RGB / IR kamera gizliliği",cameraReady?"ready":"blocked",cameraReason},
        {"touchpad","Touchpad","session","KDE sistem ayarları; firmware tuş durumu ayrı doğrulanmalı."},
        {"display","Ekran / Hz / ölçek","session","KDE'nin kendi Ekran Yapılandırması modülü."},
        {"lightbar","Işık şeridi","unsupported","Bu R9T için Windows destek kaydı 0."},
        {"local-dimming","Yerel karartma","unsupported","Bu panel için destek kaydı 0."},
        {"lcd-overdrive","LCD Overdrive","unsupported","Bu model için destek kaydı 0."},
        {"typec-priority","Type-C şarj önceliği","unsupported","Bu model için destek kaydı 0."},
        {"liquid-cooling","Sıvı soğutma","unsupported","Bu kasada destekli olarak doğrulanmadı."},
        {"audio","OEM AI ses işleme","unsupported","Windows bileşeni; bu modelin mikrofon destek kaydı 0."},
        {"firmware-update","BIOS/EC flash","unsupported","Firmware flash bu aracın görevi değil."}
    };
    const auto issue=io.compatibilityIssue();
    out.insert(out.begin(),{"model-bios","Model / SKU / BIOS eşleşmesi",issue.empty()?"ready":"blocked",issue.empty()?"Tam eşleşme: " SLAYER_PRODUCT " / " SLAYER_SKU " / " SLAYER_BIOS ".":issue});
    if(!issue.empty())for(auto&c:out)if(c.state=="ready"&&c.id!="model-bios"&&c.id!="info"&&c.id!="sensors"&&c.id!="usb"&&c.id!="fan-curve-editor"){
        c.state="blocked";c.reason=issue+" Donanım yazmaları kilitli.";
    }
    return out;
}
std::string capabilityReason(const Io &io,const std::string &id) {for(const auto&c:capabilities(io))if(c.id==id)return c.reason;return "Bilinmeyen yetenek.";}
void battery(const Io &io,const std::vector<std::string>&a) {
    auto bs=batteries(io);if(bs.empty())throw Error("Sistem pili bulunamadı.",4);const auto &p=bs.front();
    if(a.empty()||(a.size()==1&&a[0]=="status")) {
        for(auto n:{"status","capacity","cycle_count","charge_types","charge_full","charge_full_design","current_now","voltage_now","charge_control_start_threshold","charge_control_end_threshold"}) {
            auto s=get(io,p+"/"+n);if(!s.empty())std::cout<<n<<": "<<s<<'\n';
        }
        std::cout<<"Birimler: charge_* µAh, current_now µA, voltage_now µV; capacity %.\n";return;
    }
    io.requireCompatible();
    FileLock guard(io,"battery");
    if(a[0]=="profile") {
        if(a.size()!=2)throw Error("Kullanım: battery profile high-capacity|balanced|stationary");
        if(!contains({"high-capacity","balanced","stationary"},a[1]))throw Error("Geçersiz pil profili.");
        if(io.exists(p+"/charge_types")){
            const std::map<std::string,std::string> types={{"high-capacity","Standard"},{"balanced","Long_Life"},{"stationary","Trickle"}};
            const auto value=types.at(a[1]);auto available=get(io,p+"/charge_types");
            auto selected=[](const std::string&text){auto begin=text.find('['),end=text.find(']');return begin!=std::string::npos&&end>begin?text.substr(begin+1,end-begin-1):text;};
            auto old=selected(available);auto tokens=words(available);bool supported=false;
            for(auto token:tokens){if(token.starts_with('['))token.erase(0,1);if(token.ends_with(']'))token.pop_back();if(token==value)supported=true;}
            if(!supported)throw Error("Kernel bu şarj türünü bildirmiyor.",4);
            if(old!=value){
                try{io.write(p+"/charge_types",value+"\n");if(!io.dry()&&selected(get(io,p+"/charge_types"))!=value)throw Error("Pil profili geri okuması uyuşmadı.",4);}
                catch(...){
                    if(!io.dry()&&(old=="Standard"||old=="Long_Life"||old=="Trickle"))try{
                        io.write(p+"/charge_types",old+"\n");
                        if(selected(get(io,p+"/charge_types"))!=old)throw Error("Geri okuma uyuşmadı.");
                    }catch(...){throw Error("Pil profili uygulanamadı; eski profil geri yüklenemedi.",4);}
                    throw;
                }
            }
            std::cout<<(io.dry()?"Pil profili önizlemesi: ":"Pil koruma profili: ")<<a[1]<<" ("<<value<<"). Yüzde eşiği değildir.\n";return;
        }
        if(!io.exists(platform+"/charging_profile"))unverified("Pil profili");
        if(!contains(words(get(io,platform+"/charging_profiles_available")),a[1]))throw Error("Kernel bu pil profilini bildirmiyor.",4);
        auto old=get(io,platform+"/charging_profile");
        if(!contains(words(get(io,platform+"/charging_profiles_available")),old))throw Error("Mevcut pil profili tanınmadı.",4);
        try{
            if(io.dry()||old!=a[1])io.write(platform+"/charging_profile",a[1]+"\n");
            if(!io.dry()&&get(io,platform+"/charging_profile")!=a[1])throw Error("Pil profili geri okuması uyuşmadı.",4);
        }catch(...){
            if(!io.dry())try{io.write(platform+"/charging_profile",old+"\n");if(get(io,platform+"/charging_profile")!=old)throw Error("Geri okuma uyuşmadı.");}
            catch(...){throw Error("Pil profili uygulanamadı; eski profil geri yüklenemedi.",4);}
            throw;
        }
        std::cout<<"Pil koruma profili: "<<a[1]<<". Yüzde eşiği değildir.\n";return;
    }
    if(a[0]!="limit"||a.size()<2||a.size()>3)throw Error("Kullanım: battery limit ÜST [ALT] (yüzde)");
    const int end=integer(a[1],50,100,"Üst eşik");
    const int start=a.size()==3?integer(a[2],0,95,"Alt eşik"):-1;
    if(start>=end)throw Error("Alt eşik üst eşikten küçük olmalı.");
    if(!io.exists(p+"/charge_control_end_threshold"))throw Error("%"+std::to_string(end)+" şarj sınırı uygulanmadı: kernel/firmware standart yüzde eşiği sunmuyor. Koruma profili ayrı bir özelliktir; KDE yerel yüzde ayarı da bu arayüzü gerektirir.",4);
    if(start>=0&&!io.exists(p+"/charge_control_start_threshold"))throw Error("Kernel alt eşik desteği sunmuyor.",4);
    auto oldEnd=get(io,p+"/charge_control_end_threshold"),oldStart=get(io,p+"/charge_control_start_threshold");
    if(oldEnd.empty())throw Error("Mevcut üst eşik okunamadı; işlem iptal edildi.");
    const int oldE=integer(oldEnd,0,100,"Mevcut üst eşik");
    if(start>=0 && oldStart.empty())throw Error("Mevcut alt eşik okunamadı.");
    if(!oldStart.empty()&&integer(oldStart,0,100,"Mevcut alt eşik")>=oldE)throw Error("Mevcut pil eşikleri tutarsız; işlem iptal edildi.");
    if(start<0&&!oldStart.empty()&&integer(oldStart,0,100,"Mevcut alt eşik")>=end)throw Error("Yeni üst eşik mevcut alt eşikten büyük olmalı; ALT değerini de belirtin.");
    bool attempted=false;
    auto set=[&](const std::string&n,const std::string&v){if(io.dry()||get(io,p+"/"+n)!=v){attempted=true;io.write(p+"/"+n,v+"\n");}};
    try {
        if(start>=0&&start>=oldE){set("charge_control_end_threshold",std::to_string(end));set("charge_control_start_threshold",std::to_string(start));}
        else {
            if(start>=0)set("charge_control_start_threshold",std::to_string(start));
            set("charge_control_end_threshold",std::to_string(end));
        }
        if(!io.dry()&&get(io,p+"/charge_control_end_threshold")!=std::to_string(end))throw Error("Üst eşik geri okuması uyuşmadı.");
        if(!io.dry()&&start>=0&&get(io,p+"/charge_control_start_threshold")!=std::to_string(start))throw Error("Alt eşik geri okuması uyuşmadı.");
    }catch(...){
        if(attempted&&!io.dry())try{
            set("charge_control_end_threshold","100");if(!oldStart.empty())set("charge_control_start_threshold",oldStart);set("charge_control_end_threshold",oldEnd);
            if(get(io,p+"/charge_control_end_threshold")!=oldEnd||(!oldStart.empty()&&get(io,p+"/charge_control_start_threshold")!=oldStart))throw Error("Geri okuma uyuşmadı.");
        }catch(...){throw Error("Pil eşiği uygulanamadı; eski eşikler tam geri yüklenemedi.",4);}
        throw;
    }
    std::cout<<(io.dry()?"Pil eşiği önizlemesi tamamlandı.":"Pil eşiği ayarlandı. Gerçek şarj davranışı pil/firmware tarafından yönetilir.")<<'\n';
}
void profile(const Io &io,const std::vector<std::string>&a) {
    if(!a.empty()&&a[0]=="sync"){profileSynchronization(io,{a.begin()+1,a.end()});return;}
    const std::string p=hardwareProfilePath(io);
    auto choices=words(get(io,p+"_choices"));
    if(a.empty()||(a.size()==1&&a[0]=="status")) {std::cout<<"Profil: "<<get(io,p)<<"\nSeçenekler: "<<get(io,p+"_choices")<<'\n';return;}
    if(a.size()!=2||a[0]!="set")throw Error("Kullanım: profile set PROFİL");
    if(!io.exists(p))unverified("Güç profili");
    if(!contains(choices,a[1]))throw Error("Kernel bu profili bildirmiyor.",4);
    auto old=get(io,p);if(!contains(choices,old)&&old!="custom")throw Error("Mevcut OEM profili tanınmadı.",4);
    semanticWrite(io,p,a[1],old,"thermal");std::cout<<"OEM profil isteği uygulandı.\n";
}
void gpu(const Io &io,const std::vector<std::string>&a) {
    if(a.empty()||(a.size()==1&&a[0]=="status")) {auto s=status(io);std::cout<<"Sürücü: "<<s["nvidia"]<<"\nİç panel: "<<s["panel"]<<"\nSonraki açılış MUX seçimi: "<<s["mux_mode"]<<"\n";return;}
    if(a[0]=="mode") {
        if(a.size()!=2||!contains({"hybrid","dedicated","integrated"},a[1]))throw Error("Kullanım: gpu mode hybrid|dedicated|integrated");
        io.requireCompatible();const auto p=platform+"/mux_mode";
        if(!io.exists(p))unverified("Fiziksel MUX");
        auto choices=words(get(io,platform+"/mux_modes_available"));auto old=get(io,p);
        if(!contains(choices,a[1])||!contains(choices,old))throw Error("Kernel bu MUX sözleşmesini sunmuyor.",4);
        semanticWrite(io,p,a[1],old,"mux");
        std::cout<<(io.dry()?"MUX önizlemesi: ":"Sonraki açılış için MUX: ")<<a[1]<<". Yeniden başlatma gerekir; mevcut oturum değiştirilmedi.\n";
        if(a[1]=="integrated")std::cout<<"Yalnız iGPU: NVIDIA'ya bağlı harici ekran portları kullanılamayabilir. Dönüş: gpu mode hybrid veya BIOS Advanced Display Mode.\n";
        return;
    }
    if(a[0]=="limits"||a[0]=="power-limit"){gpuLimits(io,a);return;}
    if(a[0]=="dynamic-boost"||a[0]=="overclock")unverified("GPU güç/OC kontrolü");
    throw Error("Kullanım: gpu status | gpu mode hybrid|dedicated|integrated");
}
void fan(const Io &io,const std::vector<std::string>&a) {
    if(a.empty()||(a.size()==1&&a[0]=="status")) {sensors(io);std::cout<<"Mod: "<<get(io,platform+"/fan_mode")<<'\n';return;}
    if(a.size()==1&&contains({"auto","boost"},a[0])) {
        if(!io.exists(platform+"/fan_mode"))unverified("Fan modu");
        if(!contains(words(get(io,platform+"/fan_modes_available")),a[0]))throw Error("Kernel bu fan modunu bildirmiyor.",4);
        auto old=get(io,platform+"/fan_mode");if(!contains({"auto","boost"},old))throw Error("Firmware özel fan modunda; önce profile set balanced veya performance ile fabrika modunu seçin.",4);
        semanticWrite(io,platform+"/fan_mode",a[0],old,"thermal");std::cout<<"Fan modu: "<<a[0]<<'\n';return;
    }
    if(a[0]=="curve"){fanCurve(io,{a.begin()+1,a.end()});return;}
    if(a[0]=="set")unverified("Manuel fan hızı");
    throw Error("Kullanım: fan status | fan auto | fan boost | fan curve");
}
void sensors(const Io &io) {
    for(const auto &node:io.list("/sys/class/hwmon")) {
        auto base="/sys/class/hwmon/"+node;auto name=get(io,base+"/name");
        for(const auto &n:io.list(base)) {
            bool temp=n.starts_with("temp")&&n.ends_with("_input");bool rpm=n.starts_with("fan")&&n.ends_with("_input");
            if(!temp&&!rpm)continue;
            auto value=get(io,base+"/"+n);if(value.empty())continue;
            auto label=get(io,base+"/"+n.substr(0,n.size()-6)+"_label");
            if(label.empty())label=n;
            if(temp){int v=integer(value,-100000,250000,"Sensör");std::cout<<displayText(name)<<" / "<<displayText(label)<<": "<<std::fixed<<std::setprecision(1)<<v/1000.0<<" °C\n";}
            else std::cout<<displayText(name)<<" / "<<displayText(label)<<": "<<value<<" RPM\n";
        }
    }
}
void firmware(const Io &io) {
    const std::string guid="-9f33f85c-13ca-4fd1-9c4a-96217722c593";
    auto bytes=io.read("/sys/firmware/efi/efivars/OemMagicVariable"+guid,4096);
    if(bytes.empty()){std::cout<<"OEM firmware verisi okunamadı (yetki veya değişken yok).\n";return;}
    if(bytes.size()<4){throw Error("OEM firmware verisi kesik.");}
    std::cout<<"OEM payload: "<<bytes.size()-4<<" bayt (yalnız okunur).\n";
    if(bytes.size()==184) {
        std::cout<<"Proje kimliği adayı: "<<static_cast<unsigned>(static_cast<unsigned char>(bytes[10]))<<'\n';
        std::cout<<"OEM MUX alanı (0=hibrit,1=NVIDIA,2=iGPU): "<<static_cast<unsigned>(static_cast<unsigned char>(bytes[4+0x62]))<<" (yalnız tam eşleşen kernel arayüzü üzerinden seçilir).\n";
    }else std::cout<<"Bilinmeyen firmware veri sürümü; alanlar yorumlanmadı.\n";
}
void doctor(const Io &io) {
    auto s=status(io);
    std::cout<<"slayerctl "<<version<<" / salt okunur denetim\n";
    std::cout<<"Cihaz: "<<s["vendor"]<<" "<<s["model"]<<" / "<<s["sku"]<<"\nBIOS: "<<s["bios"]<<'\n';
    std::cout<<"Yazma sözleşmesi: "<<(io.compatibilityIssue().empty()?"tam eşleşme / BIOS " SLAYER_BIOS:io.compatibilityIssue())<<'\n';
    if(s["vendor"]!="GAME GARAJ"||s["model"]!="SLAYER R9T")std::cout<<"UYARI: R9T kimliği eşleşmiyor; firmware yazmaları kapalıdır.\n";
    std::cout<<"NVIDIA: "<<s["nvidia"]<<"\nKlavye: "<<s["keyboard_driver"]<<" / "<<s["keyboard_leds"]<<" LED\n";
    if(s["nvidia"]!="loaded"||s["keyboard_driver"]!="ite_8291")std::cout<<"Sürücü eksikse Secure Boot/MOK kaydını kontrol edin; bu araç Secure Boot'u kapatmaz.\n";
    std::cout<<"TPM: "<<s["tpm"]<<" / Secure Boot: "<<s["secure_boot"]<<'\n';
    std::cout<<"R9T köprüsü: pil / Fn / fabrika fan-termal / sabit MUX alanı. Genel EC/Setup/SMI/TDP yazması yok.\n";
    if(!io.fixture()) {
        auto log=io.read("/run/slayerctl/driver-status");if(!log.empty())std::cout<<trim(log)<<'\n';
    }
}
static bool usbPort(const std::string& s) {
    if(s.empty()||s.size()>27)return false;
    std::size_t pos=0;
    auto number=[&](){std::size_t start=pos;while(pos<s.size()&&s[pos]>='0'&&s[pos]<='9')++pos;return pos>start&&pos-start<=3;};
    if(!number()||pos==s.size()||s[pos++]!='-'||!number())return false;
    unsigned dots=0;
    while(pos<s.size()){if(s[pos++]!='.'||++dots>5||!number())return false;}
    return true;
}
void usb(const Io &io,const std::vector<std::string>&a) {
    const std::string base="/sys/bus/usb/devices/";
    if(a.empty()||(a.size()==1&&(a[0]=="list"||a[0]=="status"))) {
        for(const auto &n:io.list("/sys/bus/usb/devices")) {
            if(!usbPort(n))continue;
            auto p=base+n;auto vid=get(io,p+"/idVendor"),pid=get(io,p+"/idProduct");if(vid.empty())continue;
            std::cout<<n<<"  "<<vid<<":"<<pid<<"  "<<displayText(get(io,p+"/product"))<<"  güç="<<get(io,p+"/power/control")<<"  durum="<<get(io,p+"/power/runtime_status")<<"  uyanma="<<get(io,p+"/power/wakeup")<<'\n';
        }return;
    }
    if(a[0]=="charging")unverified("Kapalı/uykuda USB şarj");
    if(a.size()!=3||!contains({"powersave","wake"},a[0])||!usbPort(a[1])||!contains({"on","off"},a[2]))
        throw Error("Kullanım: usb powersave|wake PORT on|off (örnek port: 3-2.1)");
    auto p=base+a[1];if(get(io,p+"/idVendor").empty())throw Error("USB aygıtı bulunamadı.");
    std::vector<std::string> classes{get(io,p+"/bDeviceClass")};
    for(const auto &n:io.list("/sys/bus/usb/devices"))if(n.starts_with(a[1]+":"))classes.push_back(get(io,base+n+"/bInterfaceClass"));
    if(classes.size()==1)throw Error("USB arayüz sınıfları okunamadı; işlem reddedildi.",4);
    for(auto cls:classes) {
        if(cls.size()!=2||cls.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw Error("USB sınıfı doğrulanamadı; işlem reddedildi.",4);
        std::transform(cls.begin(),cls.end(),cls.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(contains({"03","08","09"},cls))throw Error("Hub, klavye/fare ve depolama USB aygıtlarının güç ayarları korunur.",4);
    }
    const std::string file=a[0]=="wake"?"wakeup":"control";
    if(!io.exists(p+"/power/"+file))throw Error("USB aygıtı bu güç kontrolünü sunmuyor.",4);
    auto v=a[0]=="wake"?(a[2]=="on"?"enabled":"disabled"):(a[2]=="on"?"auto":"on");
    auto target=p+"/power/"+file;auto old=get(io,target);
    if(!contains(a[0]=="wake"?std::vector<std::string>{"enabled","disabled"}:std::vector<std::string>{"on","auto"},old))
        throw Error("Mevcut USB güç durumu doğrulanamadı; yazma yapılmadı.",4);
    semanticWrite(io,target,v,old,"usb");std::cout<<"USB güç ayarı uygulandı.\n";
}
}
