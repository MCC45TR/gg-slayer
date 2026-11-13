// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include <iostream>
#include <unistd.h>

namespace slayer { void configuration(const Io&,const std::vector<std::string>&); }
namespace {
void help(){std::cout<<"slayerctl "<<slayer::version<<R"( — Game Garaj Slayer R9T denetimi

Donanım yazmaları yalnız R9T V1 / BIOS N.1.46GAR16 tam eşleşmesinde açılır.
Farklı/okunamayan BIOS kilitlenir. Zorla atlama seçeneği yoktur.

Bilgi (yönetici yetkisi gerekmez):
  slayerctl status                 Kısa cihaz ve sürücü durumu
  slayerctl info                   BIOS, model, CPU, pil ve klavye
  slayerctl bios                   BIOS sürümü ve tarihi
  slayerctl doctor                 Salt okunur sürücü/güvenlik denetimi
  slayerctl capabilities           Hazır / doğrulanmamış / desteklenmeyen özellikler
  slayerctl sensors                Sıcaklık ve mevcut fan sensörleri
  slayerctl gpu status             Panel bağlantısı ve sürücü
  slayerctl usb list               USB aygıtları ve güç durumu
  slayerctl firmware               OEM firmware alanları (yalnız okuma; sudo gerekebilir)
  slayerctl probe                  İsteğe bağlı salt okunur kernel arka ucu verisi

Klavye:
  sudo slayerctl keyboard brightness 50
  sudo slayerctl keyboard color 3DAEE9
  sudo slayerctl keyboard key W FF8000
  slayerctl keyboard layout list
  sudo slayerctl keyboard identify 66
  sudo slayerctl keyboard layout set W 66
  sudo slayerctl keyboard profile create oyun 101010 50
  sudo slayerctl keyboard profile set-key oyun W FF0000
  sudo slayerctl keyboard profile apply oyun
  sudo slayerctl keyboard profile capture mevcut
  sudo slayerctl keyboard profile set-group oyun wasd FF0000
  sudo slayerctl keyboard profile base oyun 101010 50
  slayerctl keyboard profile list
  slayerctl keyboard hardware list
  sudo slayerctl keyboard hardware wave --speed 5 --brightness 50 --direction right
  sudo slayerctl keyboard hardware off
  sudo slayerctl keyboard profile effect dalga wave 5 50 right
  sudo slayerctl config set rgb_profile dalga
  sudo slayerctl config unset rgb_profile
  slayerctl keyboard accent on      KDE vurgu rengini izle (sudo kullanmayın)
  slayerctl keyboard accent off
  sudo slayerctl keyboard effect wave --duration 10 --fps 10
  Efektler: rainbow, wave, breathe, cycle. Ctrl+C ile durur ve eski durumu geri yükler.
  Tuş numaraları sürücünün 6×21 LED indeksleridir; fiziksel tuş etiketi değildir.

Standart kernel arayüzü sağlandığında:
  sudo slayerctl battery limit 80 60
  sudo slayerctl battery profile high-capacity|balanced|stationary
  sudo slayerctl profile sync on|off  KDE / EC modlarını olaylarla eşitle
  sudo slayerctl profile set low-power|balanced|performance
  sudo slayerctl fan auto|boost
  slayerctl cpu status
  sudo slayerctl cpu boost on|off
  sudo slayerctl cpu limits 500 2500
  sudo slayerctl cpu limits reset
  slayerctl gpu limits
  sudo slayerctl gpu power-limit WATT|reset
  sudo slayerctl fan curve save sessiz 40:30,60:60,90:100 40:30,65:65,90:100
  slayerctl fan curve show sessiz
  sudo slayerctl settings fn-lock on|off
  sudo slayerctl settings camera on|off   Dahili RGB/IR USB gizliliği
  sudo slayerctl settings usb-charging on|off  Yalnız destek biti varsa
  slayerctl cpu topology
  slayerctl cpu smt status
  sudo slayerctl cpu smt on|off     32/16 iş parçacığı; fiziksel CCD değildir
  sudo slayerctl gpu mode hybrid|dedicated|integrated  Sonraki açılış
  sudo slayerctl config set fn_lock on cpu_smt on fan_mode auto
  sudo slayerctl config set battery_profile stationary
  sudo slayerctl config set rgb_profile_ac oyun rgb_profile_battery gece
  sudo slayerctl config set desktop_profile_ac balanced desktop_profile_battery power-saver
  sudo slayerctl usb powersave 3-2.1 on
  sudo slayerctl usb wake 3-2.1 off
  Hub, klavye/fare ve depolama aygıtlarının USB güç ayarları korunur.

Kalıcı yapılandırma (JSON yok):
  slayerctl config show
  sudo slayerctl config set keyboard_brightness 50
  sudo slayerctl config set battery_end 80
  sudo slayerctl apply-config
  sudo slayerctl config clear       Açılışta ek ayar uygulamayı kapat

Diğer:
  slayerctl gui                     Slayer Control uygulamasını aç
  slayerctl kde                     KDE Sistem Ayarları modülünü aç
  slayerctl power status            KDE/GNOME ortak güç profili
  slayerctl power set balanced      Mevcut masaüstü servisine profil isteği
  slayerctl service status          systemd açılış/uyanma hizmetleri
  sudo systemctl enable --now slayerctl.service slayerctl-resume.service
  slayerctl --dry-run KOMUT         Yazmadan önizleme
  slayerctl --version

MUX sonraki açılışta uygulanır. Genel EC, fan eğrisi, voltaj/OC yazmaları kapalıdır.
Desteklenmeyen bir işlem sessizce başarılı sayılmaz. Genel USB reset/ayırma komutu yok; kamera gizliliği yalnız eşleşen dahili aygıta aittir.
)";}
}
int slayerMain(int argc,char**argv){
    try{
        slayer::Options opts;std::vector<std::string> args;
        const bool helper=slayer::fs::path(argv[0]).filename()=="slayerctl-helper";
        int i=1;
        for(;i<argc;++i){std::string a=argv[i];
            if(a=="--root"){if(helper)throw slayer::Error("Yardımcı test kökü kabul etmez.");if(++i==argc)throw slayer::Error("Test kökü eksik.");opts.root=argv[i];}
            else if(a=="--dry-run")opts.dry=true;
            else if(a=="--format=tsv")opts.tsv=true;
            else if(a=="--version"||a=="version"){std::cout<<slayer::version<<'\n';return 0;}
            else if(a=="--help"||a=="help"||a=="-h"){help();return 0;}
            else if(a.starts_with('-'))throw slayer::Error("Bilinmeyen seçenek: "+a);
            else break;
        }
        for(;i<argc;++i)args.emplace_back(argv[i]);
        if(args.empty()){help();return 0;}
        if(helper){
            if(geteuid()!=0)throw slayer::Error("Yardımcı yönetici yetkisiyle çağrılmalı.",3);
            const auto&c=args[0];
            bool allowed=(c=="keyboard"&&args.size()>1&&args[1]!="accent"&&args[1]!="status")
                ||(c=="battery"&&args.size()>1&&(args[1]=="limit"||args[1]=="profile"))
                ||(c=="power"&&args.size()==3&&args[1]=="set")
                ||(c=="profile"&&args.size()>1&&(args[1]=="set"||args[1]=="sync"))
                ||(c=="fan"&&args.size()>1&&(args[1]=="auto"||args[1]=="boost"||args[1]=="curve"))
                ||(c=="cpu"&&args.size()>1&&(args[1]=="boost"||args[1]=="limits"||args[1]=="smt"))
                ||(c=="gpu"&&args.size()>1&&(args[1]=="power-limit"||args[1]=="mode"))
                ||(c=="settings"&&args.size()==3)
                ||(c=="usb"&&args.size()>1&&(args[1]=="powersave"||args[1]=="wake"))
                ||(c=="config"&&args.size()>1&&(args[1]=="set"||args[1]=="clear"||args[1]=="unset"||args[1]=="protect"||args[1]=="recover"))
                ||(c=="apply-config"&&args.size()==1);
            if(!allowed)throw slayer::Error("Bu işlem yetkili yardımcı üzerinden sunulmuyor.",3);
        }
        slayer::Io io(opts);const auto command=args[0];args.erase(args.begin());
        if(command=="status"||command=="info"||command=="bios"||command=="ui-data"){
            if(!args.empty())throw slayer::Error("Bu bilgi komutu ek argüman kabul etmez.");
            auto data=command=="bios"?std::map<std::string,std::string>{{"bios",slayer::trim(io.read("/sys/class/dmi/id/bios_version"))},{"bios_date",slayer::trim(io.read("/sys/class/dmi/id/bios_date"))}}:slayer::status(io);
            for(const auto&[k,v]:data){
                if(command=="bios"&&k!="bios"&&k!="bios_date")continue;
                if(v.empty())continue;
                if(command=="ui-data")std::cout<<"value\t"<<k<<'\t'<<slayer::displayText(v)<<'\n';
                else std::cout<<k<<(opts.tsv?"\t":": ")<<slayer::displayText(v)<<'\n';
            }
            if(command=="ui-data"){
                for(const auto&k:slayer::physicalKeys(io))std::cout<<"key\t"<<k.id<<'\t'<<k.label<<'\t'<<k.row<<'\t'<<k.column<<'\t'<<k.width<<'\t'<<k.index<<'\t'<<(k.calibrated?1:0)<<'\n';
                for(const auto& led:slayer::leds(io))try{
                    const auto channels=slayer::words(io.read(led.path+"/multi_intensity"));
                    if(channels.size()!=led.channels.size()||channels.size()!=3)continue;
                    int rgb[3]={-1,-1,-1};
                    for(std::size_t i=0;i<channels.size();++i){
                        const int index=led.channels[i]=="red"?0:led.channels[i]=="green"?1:led.channels[i]=="blue"?2:-1;
                        if(index>=0)rgb[index]=slayer::integer(channels[i],0,led.channelMax.at(i),"RGB kanalı")*255/led.channelMax.at(i);
                    }
                    if(rgb[0]>=0&&rgb[1]>=0&&rgb[2]>=0)std::cout<<"key-rgb\t"<<led.index<<'\t'<<rgb[0]<<'\t'<<rgb[1]<<'\t'<<rgb[2]<<'\n';
                }catch(const slayer::Error&){/* An unreadable LED does not hide other device information. */}
                for(const auto&n:io.list("/etc/slayerctl/rgb-profiles"))if(n.ends_with(".conf")&&!n.starts_with("__slayer_")){auto name=n.substr(0,n.size()-5);slayer::validateProfileName(name);std::cout<<"rgb-profile\t"<<name<<'\n';}
            }
            if(command=="ui-data")for(const auto&c:slayer::capabilities(io))std::cout<<"cap\t"<<c.id<<'\t'<<c.state<<'\t'<<c.name<<'\t'<<c.reason<<'\n';
        }else if(command=="capabilities"||command=="caps"){
            for(const auto&c:slayer::capabilities(io)){
                if(opts.tsv)std::cout<<c.id<<'\t'<<c.state<<'\t'<<c.name<<'\t'<<c.reason<<'\n';
                else std::cout<<c.id<<" ["<<c.state<<"] "<<c.name<<" — "<<c.reason<<'\n';
            }
        }else if(command=="doctor"){if(!args.empty())throw slayer::Error("doctor ek argüman kabul etmez.");slayer::doctor(io);}
        else if(command=="telemetry"){if(!args.empty())throw slayer::Error("telemetry ek argüman kabul etmez.");slayer::telemetry(io);}
        else if(command=="sensors"){if(!args.empty())throw slayer::Error("sensors ek argüman kabul etmez.");slayer::sensors(io);}
        else if(command=="keyboard")slayer::keyboard(io,args);
        else if(command=="battery")slayer::battery(io,args);
        else if(command=="usb")slayer::usb(io,args);
        else if(command=="profile")slayer::profile(io,args);
        else if(command=="gpu"||command=="mux")slayer::gpu(io,args);
        else if(command=="fan")slayer::fan(io,args);
        else if(command=="firmware"){if(!args.empty())throw slayer::Error("firmware yalnız okunur.");slayer::firmware(io);}
        else if(command=="probe"){
            if(!args.empty())throw slayer::Error("probe yalnız okunur.");
            const std::string base="/sys/devices/platform/slayerctl";
            if(!io.exists(base))throw slayer::Error("İsteğe bağlı salt okunur slayerctl kernel modülü yüklü değil.",4);
            for(const auto&n:io.list(base))if(n.starts_with("ec_")||n=="backend_version")std::cout<<n<<": "<<slayer::trim(io.read(base+"/"+n))<<'\n';
        }else if(command=="cpu")slayer::cpuControl(io,args);
        else if(command=="settings")slayer::platformSetting(io,args);
        else if(command=="config")slayer::configuration(io,args);
        else if(command=="power-event"){if(!args.empty())throw slayer::Error("power-event ek argüman kabul etmez.");slayer::configuration(io,{"apply-event"});}
        else if(command=="apply-config"){if(!args.empty())throw slayer::Error("apply-config ek argüman kabul etmez.");slayer::configuration(io,{"apply"});}
        else if(command=="power"){
            if(args.empty()||(args.size()==1&&args[0]=="status"))args={"--power-list"};
            else if(args.size()==2&&args[0]=="set"&&(args[1]=="power-saver"||args[1]=="balanced"||args[1]=="performance"))args={"--power-set",args[1]};
            else throw slayer::Error("Kullanım: power status | power set power-saver|balanced|performance");
            if(io.fixture())throw slayer::Error("Test kipinde sistem D-Bus'una erişilmez.",4);
            if(io.dry())std::cout<<"Önizleme: mevcut KDE/GNOME güç servisi isteği; firmware/EC yazması yok.\n";
            else {args.insert(args.begin(),"/usr/libexec/slayerctl-desktop");std::cout<<slayer::capture(args,7000)<<'\n';}
        }else if(command=="service"){
            if(io.fixture()||(!args.empty()&&(args.size()!=1||args[0]!="status")))throw slayer::Error("Kullanım: service status (gerçek sistemde)");
            if(io.dry())std::cout<<"Önizleme: systemd hizmet durumu okunacak.\n";
            else std::cout<<slayer::capture({"/usr/bin/systemctl","show","slayerctl.service","slayerctl-resume.service","-p","Id","-p","UnitFileState","-p","ActiveState","-p","Result","-p","ExecMainStatus"})<<'\n';
        }else if(command=="gui"){
            if(io.fixture()||helper||geteuid()==0||!args.empty())throw slayer::Error("Slayer Control normal kullanıcı oturumunda açılmalı.");
            if(opts.dry){std::cout<<"Önizleme: slayer-control\n";return 0;}
            execl("/usr/bin/slayer-control","slayer-control",static_cast<char*>(nullptr));throw slayer::Error("Slayer Control başlatılamadı.");
        }else if(command=="kde"){
            if(io.fixture()||helper||geteuid()==0||!args.empty())throw slayer::Error("KDE modülünü normal kullanıcı oturumunda açın.");
            if(opts.dry){std::cout<<"Önizleme: kcmshell6 kcm_slayerctl\n";return 0;}
            execl("/usr/bin/kcmshell6","kcmshell6","kcm_slayerctl",static_cast<char*>(nullptr));
            throw slayer::Error("KDE ayar modülü başlatılamadı.");
        }else throw slayer::Error("Bilinmeyen komut: "+command+". 'slayerctl help' kullanın.");
        return 0;
    }catch(const slayer::Error&e){std::cerr<<"slayerctl: "<<slayer::displayText(e.what())<<'\n';return e.code;}
    catch(const std::exception&e){std::cerr<<"slayerctl: "<<slayer::displayText(e.what())<<'\n';return 1;}
}

#ifndef SLAYER_NO_MAIN
int main(int argc,char** argv){return slayerMain(argc,argv);}
#endif
