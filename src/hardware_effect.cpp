// SPDX-License-Identifier: MIT
#include "rgb.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/hidraw.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace slayer {
std::string hardwareDevice(const Io&io){
    if(!io.compatibilityIssue().empty())return {};
    for(const auto&n:io.list("/sys/class/hidraw")){
        if(!n.starts_with("hidraw")||n.size()>12||n.substr(6).empty()||n.substr(6).find_first_not_of("0123456789")!=std::string::npos)continue;
        const auto base="/sys/class/hidraw/"+n+"/device";
        const auto u=io.read(base+"/uevent",4096);
        if(u.find("HID_ID=0003:0000048D:0000600B\n")==std::string::npos)continue;
        std::error_code ec;auto p=fs::canonical(io.path(base),ec);if(ec)continue;
        bool iface=false,usb=false;
        for(unsigned depth=0;depth<6&&!p.empty()&&p!=p.root_path();++depth,p=p.parent_path()){
            auto logical=io.fixture()?"/"+fs::relative(p,io.path("/")).string():p.string();
            if(trim(io.read(logical+"/bInterfaceNumber"))=="01")iface=true;
            if(trim(io.read(logical+"/idVendor"))=="048d"&&trim(io.read(logical+"/idProduct"))=="600b"&&trim(io.read(logical+"/bcdDevice"))=="0003")usb=true;
        }
        if(iface&&usb)return n;
    }
    return {};
}
void hardwareEffect(const Io&io,const std::vector<std::string>&a){
    static const std::map<std::string,int> effects={{"breathe",2},{"wave",3},{"reactive",4},{"rainbow",5},{"ripple",6},{"marquee",9},{"raindrop",10},{"aurora",14},{"spark",17},{"off",0}};
    if(a.size()==1&&a[0]=="list"){
        for(const auto&[name,id]:effects){(void)id;std::cout<<name<<'\n';}return;
    }
    if(a.empty()||!effects.contains(a[0]))throw Error("Kullanım: keyboard hardware EFEKT [--speed 1..10] [--brightness 0..100] [--direction right|left|up|down]");
    int speed=5,brightness=50,direction=1;bool hasDirection=false;
    for(std::size_t i=1;i<a.size();i+=2){
        if(i+1==a.size())throw Error("Efekt seçeneğinin değeri eksik.");
        if(a[i]=="--speed")speed=integer(a[i+1],1,10,"Hız");
        else if(a[i]=="--brightness")brightness=integer(a[i+1],0,100,"Parlaklık");
        else if(a[i]=="--direction"){
            const std::map<std::string,int> directions={{"right",1},{"left",2},{"up",3},{"down",4}};
            if(!directions.contains(a[i+1]))throw Error("Geçersiz yön.");
            direction=directions.at(a[i+1]);hasDirection=true;
        }else throw Error("Bilinmeyen donanım efekti seçeneği.");
    }
    if(hasDirection&&a[0]!="wave")throw Error("Yön yalnız wave efektinde kullanılır.");
    io.requireCompatible();
    auto device=hardwareDevice(io);if(device.empty())throw Error("R9T / ITE 048d:600b rev0003 arayüz 1 doğrulanamadı; USB'ye yazılmadı.",4);
    // One unnumbered HID feature report. Fixed LED-only opcode 08; last byte
    // ALWAYS zero: no controller flash/save command and no generic raw API.
    std::array<unsigned char,9> report{0,8,static_cast<unsigned char>(a[0]=="off"?1:2),static_cast<unsigned char>(effects.at(a[0])),static_cast<unsigned char>(speed),static_cast<unsigned char>((brightness+1)/2),8,static_cast<unsigned char>(a[0]=="wave"?direction:0),0};
    if(a[0]=="off")report={0,8,1,0,0,0,0,0,0};
    if(io.dry()){
        std::cout<<"Önizleme: "<<device<<" / "<<a[0]<<" / hız="<<speed<<" / parlaklık="<<brightness<<"%; tek HID LED raporu, flash kaydı yok.\n";return;
    }
    const auto native="/sys/class/hidraw/"+device+"/device/controls/hardware_effect";
    if(io.exists(native)) {
        FileLock lock(io,"rgb");
        io.write(native,std::to_string(effects.at(a[0]))+" "+std::to_string(speed)+" "+std::to_string((brightness+1)/2)+" "+std::to_string(a[0]=="wave"?direction:0)+"\n");
    } else if(io.fixture()){
        // The test transport can only write its pre-existing fixture endpoint.
        std::string out;for(auto b:report){if(!out.empty())out+=' ';out+=std::to_string(b);}
        io.write("/sys/class/hidraw/"+device+"/test_report",out+"\n");
    }else{
        if(geteuid()!=0)throw Error("Donanım efekti için sudo gerekir.",3);
        FileLock lock(io,"rgb");
        int fd=open(("/dev/"+device).c_str(),O_RDWR|O_CLOEXEC|O_NOFOLLOW);
        if(fd<0){throw Error("Doğrulanmış hidraw aygıtı açılamadı.",3);}
        hidraw_devinfo info{};struct stat st{};
        std::array<char,256> phys{};
        bool valid=fstat(fd,&st)==0&&S_ISCHR(st.st_mode)&&ioctl(fd,HIDIOCGRAWINFO,&info)==0&&info.bustype==3&&static_cast<unsigned short>(info.vendor)==0x048d&&static_cast<unsigned short>(info.product)==0x600b&&ioctl(fd,HIDIOCGRAWPHYS(phys.size()),phys.data())>0&&std::string(phys.data(),strnlen(phys.data(),phys.size())).ends_with("/input1");
        int result=valid?ioctl(fd,HIDIOCSFEATURE(report.size()),report.data()):-1;
        int error=errno;close(fd);
        if(!valid)throw Error("Açılan HID aygıtı eşleşmiyor; rapor gönderilmedi.",4);
        if(result!=static_cast<int>(report.size()))throw Error("LED efekti raporu tamamlanamadı: "+std::string(std::strerror(error)),4);
    }
    std::cout<<"Donanım efekti: "<<a[0]<<". İşlem bitti; animasyon klavyede çalışır. Flash'a kaydedilmedi.\n";
}
}
