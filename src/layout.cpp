// SPDX-License-Identifier: MIT
#include "rgb.hpp"
#include <fcntl.h>
#include <cerrno>
#include <memory>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace slayer {
void saveText(const Io&io,const std::string&name,const std::string&text){
    if(io.dry()){std::cout<<"Önizleme: "<<name<<"\n"<<text;return;}
    if(!io.fixture()&&geteuid()!=0)throw Error("Kaydetmek için sudo gerekir.",3);
    auto target=io.path(name);fs::create_directories(target.parent_path());
    if(fs::is_symlink(target.parent_path())||fs::is_symlink(target))throw Error("Sembolik yapılandırma bağlantısı reddedildi.");
    std::string pattern=target.string()+".XXXXXX";
    int fd=mkostemp(pattern.data(),O_CLOEXEC);
    if(fd<0)throw Error("Geçici dosya oluşturulamadı.");
    bool ok=fchmod(fd,text.starts_with("SLAYER-TPM2\n")?0600:0644)==0;std::size_t pos=0;
    while(ok&&pos<text.size()){
        auto n=::write(fd,text.data()+pos,text.size()-pos);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){ok=false;break;}pos+=static_cast<std::size_t>(n);
    }
    if(ok)ok=fsync(fd)==0;
    close(fd);
    if(!ok){fs::remove(pattern);throw Error("Kaydetme başarısız.");}
    std::error_code ec;fs::rename(pattern,target,ec);
    if(ec){fs::remove(pattern);throw Error("Atomik kaydetme başarısız.");}
    int directory=open(target.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(directory<0)throw Error("Dizin kalıcılığı doğrulanamadı; dosya kaydedilmiş olabilir.");
    int result=fsync(directory);close(directory);
    if(result)throw Error("Dizin kalıcılığı doğrulanamadı; dosya kaydedilmiş olabilir.");

}
std::map<int,std::string> loadKeyColors(const Io&io,const std::string&name){
    std::map<int,std::string> out;auto text=io.read(name,16384);
    if(text.size()>=16384)throw Error("Dosya sınırı aşıldı.");
    std::size_t pos=0;
    while(pos<text.size()){
        auto end=text.find('\n',pos);if(end==std::string::npos)end=text.size();
        auto line=trim(text.substr(pos,end-pos));pos=end+1;
        if(line.empty()||line[0]=='#')continue;
        auto sep=line.find('=');if(sep==std::string::npos)throw Error("Geçersiz eşleme satırı.");
        int key=integer(line.substr(0,sep),0,125,"Tuş indeksi");
        if(!out.emplace(key,line.substr(sep+1)).second)throw Error("Yinelenen indeks.");
    }
    return out;
}
std::vector<PhysicalKey> physicalKeys(const Io&io){
    // R9T ISO/TR-Q visual template. LED guesses are visibly marked until
    // individually calibrated; a USB ID alone cannot prove a physical map.
    std::vector<PhysicalKey> keys={
        {"Esc","Esc",0,0,1,105},
        {"F1","F1",0,2,1,107},
        {"F2","F2",0,3,1,108},
        {"F3","F3",0,4,1,109},
        {"F4","F4",0,5,1,110},
        {"F5","F5",0,6,1,111},
        {"F6","F6",0,7,1,112},
        {"F7","F7",0,8,1,113},
        {"F8","F8",0,9,1,114},
        {"F9","F9",0,10,1,115},
        {"F10","F10",0,11,1,116},
        {"F11","F11",0,12,1,117},
        {"F12","F12",0,13,1,118},
        {"PrtSc","PrtSc",0,14,1,119},
        {"Insert","Insert",0,15,1,120},
        {"Delete","Delete",0,16,1,121},
        {"Pause","Pause",0,17,1,122},
        {"PageUp","PgUp",0,18,1,123},
        {"PageDown","PgDn",0,19,1,124},
        {"End","End",0,20,1,125},
        {"Grave","\"",1,0,1,84},
        {"1","1",1,1,1,85},
        {"2","2",1,2,1,86},
        {"3","3",1,3,1,87},
        {"4","4",1,4,1,88},
        {"5","5",1,5,1,89},
        {"6","6",1,6,1,90},
        {"7","7",1,7,1,91},
        {"8","8",1,8,1,92},
        {"9","9",1,9,1,93},
        {"0","0",1,10,1,94},
        {"Minus","*",1,11,1,95},
        {"Equal","-",1,12,1,96},
        {"Backspace","⌫",1,13,2,97},
        {"NumLock","Num",1,17,1,101},
        {"NumDivide","÷",1,18,1,102},
        {"NumMultiply","×",1,19,1,103},
        {"NumSubtract","−",1,20,1,104},
        {"Tab","Tab",2,0,2,63},
        {"Q","Q",2,2,1,65},
        {"W","W",2,3,1,66},
        {"E","E",2,4,1,67},
        {"R","R",2,5,1,68},
        {"T","T",2,6,1,69},
        {"Y","Y",2,7,1,70},
        {"U","U",2,8,1,71},
        {"I","ı",2,9,1,72},
        {"O","O",2,10,1,73},
        {"P","P",2,11,1,74},
        {"Gbreve","Ğ",2,12,1,75},
        {"Udia","Ü",2,13,1,76},
        {"Enter","Enter",2,14,2,77},
        {"Num7","7",2,17,1,80},
        {"Num8","8",2,18,1,81},
        {"Num9","9",2,19,1,82},
        {"NumAdd","+",2,20,1,83},
        {"CapsLock","Caps",3,0,2,42},
        {"A","A",3,2,1,44},
        {"S","S",3,3,1,45},
        {"D","D",3,4,1,46},
        {"F","F",3,5,1,47},
        {"G","G",3,6,1,48},
        {"H","H",3,7,1,49},
        {"J","J",3,8,1,50},
        {"K","K",3,9,1,51},
        {"L","L",3,10,1,52},
        {"Scedilla","Ş",3,11,1,53},
        {"Idot","İ",3,12,1,54},
        {"Comma",",",3,13,1,55},
        {"Num4","4",3,17,1,59},
        {"Num5","5",3,18,1,60},
        {"Num6","6",3,19,1,61},
        {"ShiftLeft","Shift",4,0,1,21},
        {"ISO","<",4,1,1,22},
        {"Z","Z",4,2,1,23},
        {"X","X",4,3,1,24},
        {"C","C",4,4,1,25},
        {"V","V",4,5,1,26},
        {"B","B",4,6,1,27},
        {"N","N",4,7,1,28},
        {"M","M",4,8,1,29},
        {"Odia","Ö",4,9,1,30},
        {"Ccedilla","Ç",4,10,1,31},
        {"Period",".",4,11,1,32},
        {"ShiftRight","Shift",4,12,2,33},
        {"Up","↑",4,14,1,35},
        {"Num1","1",4,17,1,38},
        {"Num2","2",4,18,1,39},
        {"Num3","3",4,19,1,40},
        {"NumEnter","↵",4,20,1,41},
        {"CtrlLeft","Ctrl",5,0,1,0},
        {"Fn","Fn",5,1,1,1},
        {"Meta","Win",5,2,1,2},
        {"Alt","Alt",5,3,1,3},
        {"Space","Boşluk",5,4,6,4},
        {"AltGr","AltGr",5,10,1,10},
        {"Menu","Menu",5,11,1,11},
        {"CtrlRight","Ctrl",5,12,1,12},
        {"Left","←",5,13,1,13},
        {"Down","↓",5,14,1,14},
        {"Right","→",5,15,1,15},
        {"Num0","0",5,17,2,17},
        {"NumDot",".",5,19,1,19},
    };
    std::map<std::string,int> assigned;
    for(const auto&[index,name]:loadKeyColors(io,"/etc/slayerctl/keyboard-layout.conf")){
        if(!assigned.emplace(name,index).second)throw Error("Fiziksel tuş birden fazla LED'e atanmış: "+name);
        bool found=false;
        for(auto&key:keys)if(key.id==name){key.index=index;key.calibrated=true;found=true;break;}
        if(!found)throw Error("Eşlemede bilinmeyen fiziksel tuş: "+name);
    }
    return keys;
}
void layout(const Io&io,const std::vector<std::string>&a){
    std::unique_ptr<FileLock> guard;
    if(!a.empty()&&a[0]!="list")guard=std::make_unique<FileLock>(io,"profiles");
    if(a.size()==1&&a[0]=="reset"){
        saveText(io,"/etc/slayerctl/keyboard-layout.conf","# slayerctl fiziksel tuş kalibrasyonu sıfırlandı\n");return;
    }
    auto keys=physicalKeys(io);
    if(a.empty()||(a.size()==1&&a[0]=="list")){
        for(const auto&k:keys)std::cout<<k.id<<'\t'<<k.label<<'\t'<<k.index<<'\t'<<(k.calibrated?"calibrated":"template")<<'\n';
        return;
    }
    auto map=loadKeyColors(io,"/etc/slayerctl/keyboard-layout.conf");
    if(a.size()==1&&a[0]=="reset")map.clear();
    else if(a.size()==3&&a[0]=="set"){
        bool found=false;for(const auto&k:keys)if(k.id==a[1])found=true;
        if(!found)throw Error("Bilinmeyen fiziksel tuş adı.");
        int index=integer(a[2],0,125,"LED indeksi");
        for(const auto&[i,n]:map)if(i==index&&n!=a[1])throw Error("LED başka bir fiziksel tuşa atanmış. Önce eski eşlemeyi sıfırlayın.");
        for(auto it=map.begin();it!=map.end();)if(it->second==a[1])it=map.erase(it);else ++it;
        map[index]=a[1];
    }else throw Error("Kullanım: keyboard layout list|set TUŞ İNDEKS|reset");
    std::string text="# slayerctl fiziksel tuş kalibrasyonu: LED_indeksi=tuş_adı\n";
    for(const auto&[i,n]:map)text+=std::to_string(i)+"="+n+"\n";
    saveText(io,"/etc/slayerctl/keyboard-layout.conf",text);
}
}
