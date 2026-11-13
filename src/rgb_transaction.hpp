// SPDX-License-Identifier: MIT
#pragma once
#include "rgb.hpp"
#include <algorithm>

namespace slayer {
inline bool sharedBrightness(const Io& io, const std::vector<Led>& keys) {
    return !keys.empty() && io.exists("/sys/module/ite_8291") &&
           keys.front().path.ends_with("/rgb:kbd_backlight");
}
inline void validateSavedColor(const Led& led, const std::string& color) {
    const auto values=words(color);
    if(values.size()!=led.channels.size() || values.empty())
        throw Error("Önceki RGB kanalları okunamadı; renk yazılmadı.",4);
    for(std::size_t i=0;i<values.size();++i)
        integer(values[i],0,led.channelMax.at(i),"Önceki RGB kanalı");
}
// Lock first, snapshot/validate in the caller, then explicitly begin a batch.
// The active flag is set before store: a failed store may have applied already.
class RgbTransaction {
    const Io& io;
    FileLock lock;
    std::string buffer;
    bool active=false;
public:
    explicit RgbTransaction(const Io& value,bool batch=true):io(value),lock(value,"rgb") {
        io.requireCompatible();
        if(!batch)return;
        for(const auto& name:io.list("/sys/bus/hid/devices")) {
            if(name.find("048D:600B")==std::string::npos)continue;
            const auto path="/sys/bus/hid/devices/"+name+"/controls/buffer_input";
            if(!io.exists(path))continue;
            if(!buffer.empty())throw Error("Birden fazla RGB tamponu bulundu; aygıt seçimi belirsiz.",4);
            if(trim(io.read(path))!="0")throw Error("RGB tamponu meşgul.",4);
            buffer=path;
        }
    }
    void begin() {
        if(buffer.empty())return;
        active=true;
        io.write(buffer,"1\n");
    }
    void finish() {
        if(!active)return;
        io.write(buffer,"0\n");
        if(!io.dry()&&trim(io.read(buffer))!="0")throw Error("RGB tamponu kapanmadı.",4);
        active=false;
    }
    bool batched()const{return !buffer.empty();}
    ~RgbTransaction(){if(active)try{io.write(buffer,"0\n");}catch(...){}}
};
}
