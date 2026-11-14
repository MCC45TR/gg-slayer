// SPDX-License-Identifier: MIT
#include "rgb.hpp"
#include "../kernel/ite8291/color_model.h"
#include <algorithm>
#include <cctype>
#include <iostream>
namespace slayer {
static std::string modelText(std::string text){
    text=trim(text);
    if(text.empty()||text.size()%2||text.size()>SLAYER_COLOR_MODEL_MAX*2)throw Error("Geçersiz renk modeli boyutu.");
    std::vector<unsigned char> bytes;
    auto nibble=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;throw Error("Renk modeli onaltılık olmalı.");};
    for(std::size_t i=0;i<text.size();i+=2)bytes.push_back(static_cast<unsigned char>((nibble(text[i])<<4)|nibble(text[i+1])));
    if(!slayer_color_model_valid(bytes.data(),static_cast<unsigned int>(bytes.size())))throw Error("RGB küpünü kapsayan geçerli renk modeli gerekli.",4);
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return text;
}
void colorProfile(const Io& io,const std::vector<std::string>& a){
    if(a.empty())throw Error("Kullanım: keyboard color-profile set AD PARÇALAR... | apply AD | off | reset");
    io.requireCompatible();
    const auto device=hardwareDevice(io);
    const auto base="/sys/class/hidraw/"+device+"/device/controls/";
    if(device.empty()||!io.exists(base+"color_profile"))throw Error("Ekran renk eşleştirmesi için güncel R9T LED sürücüsü gerekli.",4);
    std::string name,desired,file,oldFile;
    bool save=false,hadFile=false;
    if(a[0]=="off"||a[0]=="reset"){
        if(a.size()!=1)throw Error("Bu işlem ek argüman kabul etmez.");
        desired=a[0];
    }else if((a[0]=="set"&&a.size()>=3&&a.size()<=9)||(a[0]=="apply"&&a.size()==2)){
        name=a[1];validateProfileName(name);if(name=="off")throw Error("Bu profil adı ayrılmış.");
        file="/etc/slayerctl/color-profiles/"+name+".conf";
        save=a[0]=="set";
        if(save){for(std::size_t i=2;i<a.size();++i)desired+=a[i];desired=modelText(desired);}
        else desired=modelText(readProtected(io,file,4096));
    }else throw Error("Geçersiz renk profili işlemi.");
    FileLock profiles(io,"profiles");FileLock lighting(io,"rgb");
    const auto previous=trim(io.read(base+"color_profile"));
    if(previous!="off")modelText(previous);
    const auto previousPoint=trim(io.read(base+"white_point"));
    const auto point=words(previousPoint);if(point.size()!=3)throw Error("Önceki beyaz dengesi okunamadı.");
    for(const auto& channel:point)integer(channel,1,255,"Beyaz noktası");
    if(save){hadFile=io.exists(file);if(hadFile)oldFile=readProtected(io,file,4096);}
    bool saved=false,attempted=false;
    try{
        if(save){saveProtected(io,file,desired+"\n");saved=true;}
        if(desired!=previous){attempted=true;io.write(base+"color_profile",desired+"\n");}
        if(!io.dry()){
            const auto expected=desired=="reset"?"off":desired;
            if(trim(io.read(base+"color_profile"))!=expected)throw Error("Renk modeli geri okuması uyuşmadı.",4);
            if(desired=="reset"&&trim(io.read(base+"white_point"))!="255 255 255")throw Error("Beyaz dengesi sıfırlanmadı.",4);
        }
    }catch(...){
        bool restored=true;
        if(attempted&&!io.dry()){
            try{io.write(base+"white_point",previousPoint+"\n");io.write(base+"color_profile",previous+"\n");}catch(...){restored=false;}
        }
        if(saved&&!io.dry())try{if(hadFile)saveProtected(io,file,oldFile);else fs::remove(io.path(file));}catch(...){restored=false;}
        if(!restored)throw Error("Renk modeli uygulanamadı; önceki durum tam geri alınamadı.",4);
        throw;
    }
    std::cout<<(name.empty()?"Renk düzeltmesi: "+desired:"Ekran renk profili: "+name)<<".\n";
}
}
