// SPDX-License-Identifier: MIT
#include "rgb_transaction.hpp"
#include <exception>
#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <sys/file.h>
#include <thread>
#include <unistd.h>

namespace slayer {
namespace {
volatile sig_atomic_t stopped=0;
void stop(int){stopped=1;}
struct Saved {std::string path,color,brightness;};
class Animation {
    const Io &io;
    RgbTransaction transaction;
    std::vector<Saved> saved;
    std::vector<int> initial;
    bool pending=true;
public:
    explicit Animation(const Io& value,const std::vector<Led>& keys):io(value),transaction(value) {
        const bool shared=sharedBrightness(io,keys);
        for(const auto& key:keys){
            auto color=trim(io.read(key.path+"/multi_intensity"));
            std::string brightness;
            validateSavedColor(key,color);
            if(!shared || &key==&keys.front()){
                brightness=trim(io.read(key.path+"/brightness"));
                initial.push_back(integer(brightness,0,key.maximum,"Önceki parlaklık"));
            }
            saved.push_back({key.path,std::move(color),std::move(brightness)});
        }
    }
    const std::vector<int>& initialBrightness()const{return initial;}
    void begin(){transaction.begin();}
    void flush(){transaction.finish();}
    bool batched()const{return transaction.batched();}
    bool restore() noexcept {
        if(!pending)return true;
        bool okay=true;
        try{begin();}catch(...){okay=false;}
        for(const auto& entry:saved)try{
            io.write(entry.path+"/multi_intensity",entry.color+"\n");
            if(trim(io.read(entry.path+"/multi_intensity"))!=entry.color)okay=false;
        }catch(...){okay=false;}
        try{flush();}catch(...){okay=false;}
        for(const auto& entry:saved)if(!entry.brightness.empty())try{
            io.write(entry.path+"/brightness",entry.brightness+"\n");
            if(trim(io.read(entry.path+"/brightness"))!=entry.brightness)okay=false;
        }catch(...){okay=false;}
        try{transaction.restoreEffect();}catch(...){okay=false;}
        pending=!okay;
        return okay;
    }
    ~Animation(){if(pending)restore();}
};
std::array<int,3> wheel(unsigned x) {
    x%=1536;unsigned sector=x/256,t=x%256;
    switch(sector){case 0:return {255,static_cast<int>(t),0};case 1:return {255-static_cast<int>(t),255,0};
    case 2:return {0,255,static_cast<int>(t)};case 3:return {0,255-static_cast<int>(t),255};
    case 4:return {static_cast<int>(t),0,255};default:return {255,0,255-static_cast<int>(t)};}
}
}
void effect(const Io &io,const std::vector<std::string>&a) {
    io.requireCompatible();
    if(a.size()<2)throw Error("Efektler: rainbow, wave, breathe, cycle. Seçenekler: --duration SANİYE --fps 1..20");
    const auto&kind=a[1];if(kind!="rainbow"&&kind!="wave"&&kind!="breathe"&&kind!="cycle")throw Error("Geçersiz efekt.");
    int duration=10,fps=10;
    for(std::size_t i=2;i<a.size();i+=2) {
        if(i+1==a.size())throw Error("Efekt seçeneğinin değeri eksik.");
        if(a[i]=="--duration")duration=integer(a[i+1],1,3600,"Süre");
        else if(a[i]=="--fps")fps=integer(a[i+1],1,20,"FPS");
        else throw Error("Bilinmeyen efekt seçeneği.");
    }
    const auto&keys=leds(io);if(keys.empty()||keys.size()>256)throw Error("Efekt için geçerli LED aygıtı bulunamadı.",4);
    for(const auto&k:keys)if(k.channels.size()!=3||std::find(k.channels.begin(),k.channels.end(),"red")==k.channels.end()||std::find(k.channels.begin(),k.channels.end(),"green")==k.channels.end()||std::find(k.channels.begin(),k.channels.end(),"blue")==k.channels.end()||!io.exists(k.path+"/multi_intensity"))throw Error("Efekt için RGB LED gerekli.",4);
    if(io.dry()){std::cout<<"Önizleme: "<<kind<<", "<<duration<<" saniye, "<<fps<<" FPS; çıkışta önceki renk/parlaklık geri yüklenir.\n";return;}
    if(!io.fixture()&&geteuid()!=0)throw Error("Efekt için sudo gerekir. Önce KDE vurgu rengi eşlemesini kapatabilirsiniz.",3);
    // Without the ITE batching ABI, cap frame rate to avoid hundreds of USB
    // transfers per key. No continuous service exists when the command exits.
    stopped=0;
    auto oldInt=std::signal(SIGINT,stop),oldTerm=std::signal(SIGTERM,stop),oldHup=std::signal(SIGHUP,stop);
    struct SignalGuard{decltype(oldInt)i,t,h;~SignalGuard(){std::signal(SIGINT,i);std::signal(SIGTERM,t);std::signal(SIGHUP,h);}}guard{oldInt,oldTerm,oldHup};
    Animation animation(io,keys);
    const auto& initial=animation.initialBrightness();
    if(!animation.batched()&&keys.size()>8&&kind!="breathe")fps=std::min(fps,2);
    std::cout<<"Efekt: "<<kind<<" / "<<duration<<" saniye. Ctrl+C ile durdurun; önceki renk geri yüklenir.\n";
    auto start=std::chrono::steady_clock::now();
    const auto period=std::chrono::microseconds(1000000/fps);
    std::exception_ptr failure;
    try{
    for(unsigned frame=0;!stopped;++frame) {
        auto now=std::chrono::steady_clock::now();
        if(now-start>=std::chrono::seconds(duration))break;
        if(kind=="breathe") {
            unsigned phase=(frame*100/static_cast<unsigned>(fps))%200;
            int percent=static_cast<int>(phase<=100?phase:200-phase);
            // Brightness only; shared ITE hardware needs one write per frame.
            auto count=sharedBrightness(io,keys)?std::size_t(1):keys.size();
            for(std::size_t i=0;i<count;++i)io.write(keys[i].path+"/brightness",std::to_string(initial[i]*percent/100)+"\n");
        }else {
            animation.begin();
            for(const auto&k:keys) {
                auto hue=frame*30+(kind=="wave"?static_cast<unsigned>(k.index%21)*70:kind=="rainbow"?static_cast<unsigned>(k.index)*12:0);
                auto rgb=wheel(hue);std::string output;
                for(std::size_t c=0;c<k.channels.size();++c) {
                    int channel=k.channels[c]=="red"?0:k.channels[c]=="green"?1:2;
                    if(c)output+=' ';
                    output+=std::to_string((rgb[channel]*k.channelMax[c]+127)/255);
                }
                io.write(k.path+"/multi_intensity",output+"\n");
            }
            animation.flush();
        }
        // Relative sleep prevents catch-up bursts when the USB controller is slow.
        auto work=std::chrono::steady_clock::now()-now;
        if(work<period)std::this_thread::sleep_for(period-work);
    }
    }catch(...){failure=std::current_exception();}
    if(!animation.restore())throw Error("Efekt sonrası önceki renk/parlaklık tam geri yüklenemedi.",4);
    if(failure)std::rethrow_exception(failure);
}
}
