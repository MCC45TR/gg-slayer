// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include <iostream>
namespace slayer {
static std::string validateCurve(const std::string&s){
    if(s.size()>256)throw Error("Fan eğrisi çok uzun.");
    int lastT=0,lastDuty=0,count=0;std::size_t pos=0;
    while(pos<s.size()){
        auto end=s.find(',',pos);if(end==std::string::npos)end=s.size();auto point=s.substr(pos,end-pos);auto colon=point.find(':');if(colon==std::string::npos)throw Error("Eğri biçimi SICAKLIK:YÜZDE,... olmalı.");
        int t=integer(point.substr(0,colon),35,95,"Sıcaklık"),d=integer(point.substr(colon+1),30,100,"Fan yüzdesi");
        if(t<=lastT||d<lastDuty)throw Error("Sıcaklıklar artmalı, fan yüzdesi azalmamalı.");
        lastT=t;lastDuty=d;++count;pos=end+1;
    }
    if(s.empty()||s.back()==','||count<3||count>12||lastT<90||lastDuty!=100)throw Error("3–12 nokta gerekli; son nokta 90–95°C ve %100 olmalı.");
    return s;
}
void fanCurve(const Io&io,const std::vector<std::string>&a){
    const std::string base="/etc/slayerctl/fan-curves/";
    if(a.empty()||(a.size()==1&&a[0]=="list")){for(const auto&n:io.list(base))if(n.ends_with(".conf"))std::cout<<n.substr(0,n.size()-5)<<'\n';return;}
    if(a.size()<2)throw Error("Kullanım: fan curve save AD CPU_EĞRİSİ GPU_EĞRİSİ | show AD | apply AD");
    validateProfileName(a[1]);const auto file=base+a[1]+".conf";
    if(a[0]=="save"&&a.size()==4){auto cpu=validateCurve(a[2]),gpu=validateCurve(a[3]);FileLock guard(io,"curves");saveText(io,file,"# Çevrimdışı eğri; firmware desteği/güvenlik kanıtı değildir.\ncpu="+cpu+"\ngpu="+gpu+"\n");return;}
    if(!io.exists(file))throw Error("Fan eğrisi bulunamadı.");
    auto text=io.read(file,1024);if(text.size()>=1024)throw Error("Fan eğrisi boyutu geçersiz.");
    if(a[0]=="show"&&a.size()==2){std::cout<<text;return;}
    // No guessed duty, hysteresis, selector or EC register writes. A future
    // kernel fan_curve ABI must be separately reviewed before applying this.
    if(a[0]=="apply"&&a.size()==2)unverified("Fan eğrisini donanıma uygulama");
    throw Error("Geçersiz fan eğrisi komutu.");
}
}
