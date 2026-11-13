// SPDX-License-Identifier: MIT
#include "rgb.hpp"
#include <algorithm>
#include <charconv>
#include <iostream>
#include <memory>
#include <iomanip>
#include <sstream>
#include <unistd.h>
namespace slayer {
void validateProfileName(const std::string&s){
    if(s.empty()||s.size()>32||s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)throw Error("Profil adı 1–32 harf/rakam/_/- içermeli.");
}
static std::string color(const std::string&s){
    unsigned value=0;auto[p,e]=std::from_chars(s.data(),s.data()+s.size(),value,16);
    if(s.size()!=6||p!=s.data()+s.size()||e!=std::errc{})throw Error("Profil rengi RRGGBB olmalı.");
    return s;
}
static const std::string directory="/etc/slayerctl/rgb-profiles";
static std::string path(const std::string&name){validateProfileName(name);return directory+"/"+name+".conf";}
struct Profile{std::string type="static",background="000000",effect="wave",direction="right";int brightness=50,speed=5;std::map<std::string,std::string> keys;std::map<int,std::string> leds;};
static Profile load(const Io&io,const std::string&name){
    auto file=path(name);if(!io.exists(file))throw Error("RGB profili bulunamadı: "+name);
    auto text=readProtected(io,file,16384);if(text.size()>=16384)throw Error("RGB profili çok büyük.");
    Profile p;std::map<std::string,bool> seen;std::size_t pos=0;
    auto layout=physicalKeys(io);
    while(pos<text.size()){
        auto end=text.find('\n',pos);if(end==std::string::npos)end=text.size();auto line=trim(text.substr(pos,end-pos));pos=end+1;
        if(line.empty()||line[0]=='#')continue;
        auto eq=line.find('=');if(eq==std::string::npos)throw Error("Bozuk RGB profili.");auto k=line.substr(0,eq),v=line.substr(eq+1);
        if(seen.contains(k))throw Error("Yinelenen profil anahtarı.");
        seen[k]=true;
        if(k=="type"){if(v!="static"&&v!="hardware")throw Error("Geçersiz RGB profil türü.");p.type=v;}
        else if(k=="background")p.background=color(v);
        else if(k=="brightness")p.brightness=integer(v,0,100,"Parlaklık");
        else if(k=="speed")p.speed=integer(v,1,10,"Hız");
        else if(k=="effect"){
            p.effect=v;
        }else if(k=="direction")p.direction=v;
        else if(k.starts_with("led.")){p.leds[integer(k.substr(4),0,125,"LED indeksi")]=color(v);}
        else if(k.starts_with("key.")){
            bool valid=false;for(const auto&key:layout)if(key.id==k.substr(4))valid=true;
            if(!valid)throw Error("Profilde bilinmeyen tuş.");
            p.keys[k.substr(4)]=color(v);
        }else throw Error("Bilinmeyen RGB profil alanı.");
    }
    const std::vector<std::string> effects={"breathe","wave","reactive","rainbow","ripple","marquee","raindrop","aurora","spark","off"};
    if(std::find(effects.begin(),effects.end(),p.effect)==effects.end())throw Error("Bilinmeyen donanım efekti.");
    if(p.direction!="right"&&p.direction!="left"&&p.direction!="up"&&p.direction!="down")throw Error("Geçersiz efekt yönü.");
    if(p.type=="hardware"&&(!p.keys.empty()||!p.leds.empty()))throw Error("Donanım profilinde tuş rengi bulunamaz.");
    return p;
}
static std::string encode(const Profile&p){
    std::string text="# slayerctl RGB profili\ntype="+p.type+"\nbrightness="+std::to_string(p.brightness)+"\n";
    if(p.type=="hardware")return text+"effect="+p.effect+"\nspeed="+std::to_string(p.speed)+"\ndirection="+p.direction+"\n";
    text+="background="+p.background+"\n";
    for(const auto&[i,v]:p.leds)text+="led."+std::to_string(i)+"="+v+"\n";
    for(const auto&[k,v]:p.keys)text+="key."+k+"="+v+"\n";
    return text;
}
void rgbProfile(const Io&io,const std::vector<std::string>&a){
    if(a.empty()||(a.size()==1&&a[0]=="list")){
        for(const auto&n:io.list(directory))if(n.ends_with(".conf")&&!n.starts_with("__slayer_")){auto name=n.substr(0,n.size()-5);validateProfileName(name);std::cout<<name<<'\n';}
        return;
    }
    if(a.size()<2)throw Error("Kullanım: keyboard profile list|show|create|set-key|apply|delete|effect AD ...");
    auto file=path(a[1]);
    std::unique_ptr<FileLock> guard;
    if(a[0]!="show"&&a[0]!="apply")guard=std::make_unique<FileLock>(io,"profiles");
    if(a[0]=="create"&&(a.size()==2||a.size()==4)){
        if(io.exists(file))throw Error("Profil zaten var; üzerine yazılmadı.");
        Profile p;if(a.size()==4){p.background=color(a[2]);p.brightness=integer(a[3],0,100,"Parlaklık");}
        saveProtected(io,file,encode(p));return;
    }
    if((a[0]=="capture"||a[0]=="snapshot")&&a.size()==2){
        if(a[0]=="snapshot"&&a[1]!="__slayer_current")throw Error("Geçersiz iç profil adı.");
        if(io.exists(file)&&a[0]!="snapshot")throw Error("Profil zaten var; üzerine yazılmadı.");
        FileLock rgbGuard(io,"rgb");
        const auto&keys=leds(io);if(keys.empty())throw Error("Statik sürücü renklerini kopyalamak için LED sürücüsü gerekli.",4);
        Profile p;p.brightness=integer(trim(io.read(keys[0].path+"/brightness")),0,keys[0].maximum,"Parlaklık")*100/keys[0].maximum;
        for(const auto&k:keys){
            auto values=words(trim(io.read(k.path+"/multi_intensity")));
            if(values.size()!=3||k.channels.size()!=3)throw Error("RGB kanalları okunamadı.",4);
            std::map<std::string,int> rgb;
            for(std::size_t i=0;i<3;++i)rgb[k.channels[i]]=(integer(values[i],0,k.channelMax[i],"RGB")*255+k.channelMax[i]/2)/k.channelMax[i];
            if(!rgb.contains("red")||!rgb.contains("green")||!rgb.contains("blue"))throw Error("RGB kanalları eşleşmedi.",4);
            std::ostringstream hex;hex<<std::hex<<std::setfill('0');for(auto name:{"red","green","blue"})hex<<std::setw(2)<<rgb.at(name);
            p.leds[k.index]=hex.str();
        }
        saveProtected(io,file,encode(p));return;
    }
    if((a[0]=="effect"||a[0]=="snapshot-effect")&&a.size()>=3){
        if(a[0]=="snapshot-effect"&&a[1]!="__slayer_current")throw Error("Geçersiz iç profil adı.");
        if(io.exists(file)&&a[0]!="snapshot-effect")throw Error("Profil zaten var; üzerine yazılmadı.");
        Profile p;p.type="hardware";p.effect=a[2];
        if(a.size()!=3&&a.size()!=6)throw Error("Kullanım: keyboard profile effect AD EFEKT [HIZ PARLAKLIK YÖN]");
        if(a.size()==6){p.speed=integer(a[3],1,10,"Hız");p.brightness=integer(a[4],0,100,"Parlaklık");p.direction=a[5];}
        // Validate through the same parser without device access or writes.
        static const std::vector<std::string> names={"wave","breathe","reactive","rainbow","ripple","marquee","raindrop","aurora","spark","off"};
        if(std::find(names.begin(),names.end(),p.effect)==names.end()||(p.direction!="right"&&p.direction!="left"&&p.direction!="up"&&p.direction!="down"))throw Error("Geçersiz efekt veya yön.");
        saveProtected(io,file,encode(p));return;
    }
    auto p=load(io,a[1]);
    if(a[0]=="show"&&a.size()==2){std::cout<<encode(p);return;}
    if(a[0]=="delete"&&a.size()==2){
        if(io.dry()){std::cout<<"Önizleme: profil silinecek: "<<a[1]<<'\n';return;}
        if(!io.fixture()&&geteuid()!=0)throw Error("Profil silmek için sudo gerekir.",3);
        fs::remove(io.path(file));return;
    }
    if(a[0]=="base"&&a.size()==4){
        if(p.type!="static")throw Error("Bu komut statik profil içindir.");
        p.background=color(a[2]);p.brightness=integer(a[3],0,100,"Parlaklık");saveProtected(io,file,encode(p));return;
    }
    if(a[0]=="set-group"&&a.size()==4){
        if(p.type!="static")throw Error("Bu komut statik profil içindir.");
        std::vector<std::string> names;
        if(a[2]=="wasd")names={"W","A","S","D"};
        else if(a[2]=="arrows")names={"Up","Down","Left","Right"};
        else if(a[2]=="all")for(const auto&k:physicalKeys(io))names.push_back(k.id);
        else throw Error("Tuş grupları: wasd, arrows, all");
        auto value=color(a[3]);for(const auto&name:names)p.keys[name]=value;saveProtected(io,file,encode(p));return;
    }
    if(a[0]=="set-key"&&a.size()==4){
        if(p.type!="static")throw Error("Tuş renkleri yalnız statik profilde düzenlenir.");
        bool found=false;for(const auto&key:physicalKeys(io))if(key.id==a[2])found=true;
        if(!found)throw Error("Bilinmeyen fiziksel tuş.");
        p.keys[a[2]]=color(a[3]);saveProtected(io,file,encode(p));return;
    }
    if(a[0]=="apply"&&a.size()==2){
        auto config=configurationValues(io);
        const bool kdeBrightness=config.contains("brightness_owner")&&config.at("brightness_owner")=="kde";
        if(kdeBrightness&&p.type=="hardware"){
            const auto& keys=leds(io);
            if(!keys.empty())p.brightness=integer(trim(io.read(keys.front().path+"/brightness")),0,keys.front().maximum,"Parlaklık")*100/keys.front().maximum;
        }
        if(p.type=="hardware"){
            std::vector<std::string> args{p.effect,"--speed",std::to_string(p.speed),"--brightness",std::to_string(p.brightness)};
            if(p.effect=="wave"){args.push_back("--direction");args.push_back(p.direction);}hardwareEffect(io,args);return;
        }
        std::map<int,std::string> colors;for(const auto&led:leds(io))colors[led.index]=p.background;
        for(const auto&[i,c]:p.leds){if(!colors.contains(i))throw Error("Kaydedilmiş LED bu aygıtta yok; renk yazılmadı.",4);colors[i]=c;}
        for(const auto&key:physicalKeys(io))if(p.keys.contains(key.id)){
            if(!colors.contains(key.index))throw Error("Profilin gerektirdiği LED bulunamadı; hiçbir renk yazılmadı.",4);
            colors[key.index]=p.keys.at(key.id);
        }
        applyColors(io,colors,kdeBrightness?-1:p.brightness);return;
    }
    throw Error("Geçersiz RGB profil komutu.");
}
}
